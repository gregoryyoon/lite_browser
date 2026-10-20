// Copyright (c) 2026 Gregory Yoon. All rights reserved.
// Permission Handler for Web Permissions (Local Network Access, Loopback, etc.)

#include "tests/cefsimple_capi/simple_permission_handler.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <commctrl.h>

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_request_context_capi.h"
#include "include/internal/cef_types_content_settings.h"

#pragma comment(lib, "comctl32.lib")

#define ID_ALWAYS_ALLOW  1001
#define ID_SESSION_ALLOW 1002
#define ID_ALWAYS_BLOCK  1003

// In-memory list for session-only allowed origins
#define MAX_SESSION_ORIGINS 64
static char s_session_origins[MAX_SESSION_ORIGINS][512];
static int s_session_origin_count = 0;
static CRITICAL_SECTION s_session_cs;
static int s_cs_initialized = 0;

static void ensure_cs_initialized(void) {
  if (!s_cs_initialized) {
    InitializeCriticalSection(&s_session_cs);
    s_cs_initialized = 1;
  }
}

static int is_session_allowed(const char* origin) {
  if (!origin || !*origin) return 0;
  ensure_cs_initialized();
  EnterCriticalSection(&s_session_cs);
  int found = 0;
  for (int i = 0; i < s_session_origin_count; i++) {
    if (_stricmp(s_session_origins[i], origin) == 0) {
      found = 1;
      break;
    }
  }
  LeaveCriticalSection(&s_session_cs);
  return found;
}

static void add_session_allowed(const char* origin) {
  if (!origin || !*origin) return;
  ensure_cs_initialized();
  EnterCriticalSection(&s_session_cs);
  if (!is_session_allowed(origin) && s_session_origin_count < MAX_SESSION_ORIGINS) {
    strncpy(s_session_origins[s_session_origin_count], origin, sizeof(s_session_origins[0]) - 1);
    s_session_origins[s_session_origin_count][sizeof(s_session_origins[0]) - 1] = '\0';
    s_session_origin_count++;
  }
  LeaveCriticalSection(&s_session_cs);
}

void simple_permission_handler_cleanup_session(cef_request_context_t* context) {
  ensure_cs_initialized();
  EnterCriticalSection(&s_session_cs);
  if (s_session_origin_count == 0) {
    LeaveCriticalSection(&s_session_cs);
    return;
  }

  cef_request_context_t* req_ctx = context;
  int release_ctx = 0;
  if (!req_ctx) {
    req_ctx = cef_request_context_get_global_context();
    release_ctx = 1;
  }

  if (req_ctx && req_ctx->set_website_setting) {
    for (int i = 0; i < s_session_origin_count; i++) {
      const char* orig = s_session_origins[i];
      if (orig && *orig) {
        cef_string_t origin_str = {};
        cef_string_from_utf8(orig, strlen(orig), &origin_str);

        // Reset LOCAL_NETWORK and LOOPBACK_NETWORK settings (passing NULL clears the setting)
        req_ctx->set_website_setting(req_ctx, &origin_str, &origin_str, CEF_CONTENT_SETTING_TYPE_LOCAL_NETWORK, NULL);
        req_ctx->set_website_setting(req_ctx, &origin_str, &origin_str, CEF_CONTENT_SETTING_TYPE_LOOPBACK_NETWORK, NULL);
        req_ctx->set_website_setting(req_ctx, &origin_str, &origin_str, CEF_CONTENT_SETTING_TYPE_LOCAL_NETWORK_ACCESS, NULL);

        cef_string_clear(&origin_str);
      }
    }
  }

  if (release_ctx && req_ctx) {
    cef_base_ref_counted_t* base_ref = (cef_base_ref_counted_t*)req_ctx;
    base_ref->release(base_ref);
  }

  s_session_origin_count = 0;
  LeaveCriticalSection(&s_session_cs);
}

//
// cef_permission_handler_t implementation
//
typedef struct _simple_permission_handler_t {
  cef_permission_handler_t handler;
  simple_handler_t* parent;
  atomic_int ref_count;
} simple_permission_handler_t;

IMPLEMENT_REFCOUNTING_SIMPLE(simple_permission_handler_t, permission_handler, ref_count)

static int CEF_CALLBACK permission_handler_on_request_media_access(
    cef_permission_handler_t* self,
    cef_browser_t* browser,
    cef_frame_t* frame,
    const cef_string_t* requesting_origin,
    uint32_t requested_permissions,
    cef_media_access_callback_t* callback) {
  // Let default media handling proceed
  return 0;
}

static int CEF_CALLBACK permission_handler_on_show_permission_prompt(
    cef_permission_handler_t* self,
    cef_browser_t* browser,
    uint64_t prompt_id,
    const cef_string_t* requesting_origin,
    uint32_t requested_permissions,
    cef_permission_prompt_callback_t* callback) {

  // 1. Check if the prompt is for Local Network or Loopback Network
  uint32_t kLocalNetworkFlags = CEF_PERMISSION_TYPE_LOCAL_NETWORK | CEF_PERMISSION_TYPE_LOOPBACK_NETWORK;
  if (!(requested_permissions & kLocalNetworkFlags)) {
    // Other permissions (geolocation, notifications, etc.) fall back to default
    return 0;
  }

  // Convert requesting_origin to UTF-8
  char origin_utf8[512] = {0};
  if (requesting_origin && requesting_origin->str && requesting_origin->length > 0) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(requesting_origin->str, requesting_origin->length, &u8);
    if (u8.str) {
      strncpy(origin_utf8, u8.str, sizeof(origin_utf8) - 1);
      cef_string_utf8_clear(&u8);
    }
  }

  if (origin_utf8[0] == '\0') {
    strncpy(origin_utf8, "현재 웹사이트", sizeof(origin_utf8) - 1);
  }

  // 2. Check if already session-allowed
  if (is_session_allowed(origin_utf8)) {
    callback->cont(callback, CEF_PERMISSION_RESULT_ACCEPT);
    return 1;
  }

  // 3. Resolve parent window handle
  simple_permission_handler_t* self_impl = (simple_permission_handler_t*)self;
  HWND parent_hwnd = NULL;
  if (self_impl && self_impl->parent && self_impl->parent->window_ctx) {
    parent_hwnd = self_impl->parent->window_ctx->main_hwnd;
  }
  if (!parent_hwnd && browser) {
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      parent_hwnd = host->get_window_handle(host);
      host->base.release(&host->base);
    }
  }
  if (!parent_hwnd) {
    parent_hwnd = GetActiveWindow();
  }

  // 4. Construct TaskDialog with Command Links
  wchar_t w_origin[512] = {0};
  MultiByteToWideChar(CP_UTF8, 0, origin_utf8, -1, w_origin, sizeof(w_origin)/sizeof(w_origin[0]));

  wchar_t main_inst[512];
  swprintf(main_inst, sizeof(main_inst)/sizeof(main_inst[0]),
           L"'%s'에서 로컬 네트워크 기기에 연결하려고 합니다.", w_origin);

  const wchar_t* content_text =
      L"이 사이트가 사설 네트워크(192.168.x.x 등) 또는 내 PC(localhost/127.0.0.1)의 장비 및 서비스와 통신하도록 허용하시겠습니까?\n\n"
      L"신뢰할 수 없는 사이트인 경우 사내망 및 기기 보안을 위해 차단하는 것을 권장합니다.";

  TASKDIALOG_BUTTON buttons[4];
  buttons[0].nButtonID = ID_ALWAYS_ALLOW;
  buttons[0].pszButtonText = L"항상 허용\n이 사이트의 로컬 네트워크 접근을 영구적으로 승인합니다.";

  buttons[1].nButtonID = ID_SESSION_ALLOW;
  buttons[1].pszButtonText = L"이번만 허용\n브라우저를 닫을 때까지만 허용하고 다음 방문 시 다시 확인합니다.";

  buttons[2].nButtonID = IDCANCEL;
  buttons[2].pszButtonText = L"취소\n이번 요청을 닫습니다. (다음 방문 시 다시 확인)";

  buttons[3].nButtonID = ID_ALWAYS_BLOCK;
  buttons[3].pszButtonText = L"항상 차단\n이 사이트의 로컬 네트워크 접근을 영구적으로 차단합니다.";

  TASKDIALOGCONFIG tc = {0};
  tc.cbSize = sizeof(tc);
  tc.hwndParent = parent_hwnd;
  tc.hInstance = GetModuleHandle(NULL);
  tc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  tc.pszWindowTitle = L"로컬 네트워크 액세스 요청";
  tc.pszMainIcon = TD_SHIELD_ICON;
  tc.pszMainInstruction = main_inst;
  tc.pszContent = content_text;
  tc.pButtons = buttons;
  tc.cButtons = 4;
  tc.nDefaultButton = ID_SESSION_ALLOW;

  int nButton = 0;
  HRESULT hr = TaskDialogIndirect(&tc, &nButton, NULL, NULL);

  if (FAILED(hr)) {
    // Fallback: standard MessageBox
    int mb_res = MessageBoxW(parent_hwnd, content_text, main_inst, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (mb_res == IDYES) {
      nButton = ID_ALWAYS_ALLOW;
    } else if (mb_res == IDNO) {
      nButton = ID_ALWAYS_BLOCK;
    } else {
      nButton = IDCANCEL;
    }
  }

  // 5. Apply user decision
  if (nButton == ID_ALWAYS_ALLOW) {
    callback->cont(callback, CEF_PERMISSION_RESULT_ACCEPT);
  } else if (nButton == ID_SESSION_ALLOW) {
    add_session_allowed(origin_utf8);
    callback->cont(callback, CEF_PERMISSION_RESULT_ACCEPT);
  } else if (nButton == ID_ALWAYS_BLOCK) {
    callback->cont(callback, CEF_PERMISSION_RESULT_DENY);
  } else {
    // IDCANCEL or dialog dismissed
    callback->cont(callback, CEF_PERMISSION_RESULT_DISMISS);
  }

  return 1; // Handled directly
}

static void CEF_CALLBACK permission_handler_on_dismiss_permission_prompt(
    cef_permission_handler_t* self,
    cef_browser_t* browser,
    uint64_t prompt_id,
    cef_permission_request_result_t result) {
  // Optional cleanup on dismiss
}

cef_permission_handler_t* simple_permission_handler_create(simple_handler_t* parent_handler) {
  simple_permission_handler_t* h = (simple_permission_handler_t*)calloc(1, sizeof(simple_permission_handler_t));
  if (!h) return NULL;

  INIT_CEF_BASE_REFCOUNTED(&h->handler.base, cef_permission_handler_t, permission_handler);
  atomic_store(&h->ref_count, 1);
  h->parent = parent_handler;

  h->handler.on_request_media_access_permission = permission_handler_on_request_media_access;
  h->handler.on_show_permission_prompt = permission_handler_on_show_permission_prompt;
  h->handler.on_dismiss_permission_prompt = permission_handler_on_dismiss_permission_prompt;

  return &h->handler;
}
