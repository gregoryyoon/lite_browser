// Copyright (c) 2026 Gregory Yoon. All rights reserved.
// Custom URL Protocol Handler for external desktop applications (ETX, Zoom, Slack, Mailto, etc.)

#include "tests/cefsimple_capi/simple_protocol_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_frame_capi.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

static void get_allowed_protocols_path(char* out_path, size_t max_len) {
  char user_profile[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, user_profile, CSIDL_PROFILE, FALSE) ||
      GetEnvironmentVariableA("USERPROFILE", user_profile, MAX_PATH) > 0) {
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\.lite-browser", user_profile);
    CreateDirectoryA(dir_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\allowed_protocols.txt", user_profile);
  } else {
    snprintf(out_path, max_len, ".lite_browser_allowed_protocols.txt");
  }
}

static int is_protocol_always_allowed(const char* origin, const char* scheme) {
  if (!origin || !scheme || !*origin || !*scheme) return 0;
  char path[MAX_PATH];
  get_allowed_protocols_path(path, sizeof(path));
  FILE* fp = fopen(path, "r");
  if (!fp) return 0;

  char line[1024];
  int found = 0;
  while (fgets(line, sizeof(line), fp)) {
    char* newline = strpbrk(line, "\r\n");
    if (newline) *newline = '\0';

    char* pipe = strchr(line, '|');
    if (pipe) {
      *pipe = '\0';
      const char* file_origin = line;
      const char* file_scheme = pipe + 1;
      if (_stricmp(file_origin, origin) == 0 && _stricmp(file_scheme, scheme) == 0) {
        found = 1;
        break;
      }
    }
  }
  fclose(fp);
  return found;
}

static void save_allowed_protocol(const char* origin, const char* scheme) {
  if (!origin || !scheme || !*origin || !*scheme) return;
  if (is_protocol_always_allowed(origin, scheme)) return;

  char path[MAX_PATH];
  get_allowed_protocols_path(path, sizeof(path));
  FILE* fp = fopen(path, "a");
  if (fp) {
    fprintf(fp, "%s|%s\n", origin, scheme);
    fflush(fp);
    fclose(fp);
  }
}

static int is_standard_web_scheme(const char* scheme) {
  static const char* kStandardSchemes[] = {
    "http", "https", "file", "chrome", "edge", "lite", "about",
    "data", "blob", "javascript", "ws", "wss", NULL
  };
  for (int i = 0; kStandardSchemes[i] != NULL; i++) {
    if (_stricmp(scheme, kStandardSchemes[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int is_registered_url_protocol(const wchar_t* w_scheme, wchar_t* out_app_name, size_t max_app_name) {
  HKEY hKey = NULL;
  if (RegOpenKeyExW(HKEY_CLASSES_ROOT, w_scheme, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    return 0;
  }

  // Windows URL protocols MUST have a "URL Protocol" value under HKCR\<scheme>
  DWORD type = 0;
  if (RegQueryValueExW(hKey, L"URL Protocol", NULL, &type, NULL, NULL) != ERROR_SUCCESS) {
    RegCloseKey(hKey);
    return 0;
  }

  if (out_app_name && max_app_name > 0) {
    DWORD bytes = (DWORD)(max_app_name * sizeof(wchar_t));
    if (RegQueryValueExW(hKey, NULL, NULL, &type, (LPBYTE)out_app_name, &bytes) == ERROR_SUCCESS && bytes > sizeof(wchar_t)) {
      // Remove any prefix like "URL:xxx" or "etx12:xxx"
      if (_wcsnicmp(out_app_name, L"URL:", 4) == 0) {
        wmemmove(out_app_name, out_app_name + 4, wcslen(out_app_name + 4) + 1);
      }
      wchar_t scheme_prefix[64];
      swprintf(scheme_prefix, sizeof(scheme_prefix)/sizeof(scheme_prefix[0]), L"%s:", w_scheme);
      size_t prefix_len = wcslen(scheme_prefix);
      if (_wcsnicmp(out_app_name, scheme_prefix, prefix_len) == 0) {
        wmemmove(out_app_name, out_app_name + prefix_len, wcslen(out_app_name + prefix_len) + 1);
      }
      // Trim leading spaces
      wchar_t* start = out_app_name;
      while (*start == L' ' || *start == L'\t') start++;
      if (start != out_app_name) {
        wmemmove(out_app_name, start, wcslen(start) + 1);
      }
    } else {
      swprintf(out_app_name, max_app_name, L"%s 애플리케이션", w_scheme);
    }
  }

  RegCloseKey(hKey);
  return 1;
}

static void extract_origin(const char* full_url, char* out_origin, size_t max_len) {
  if (!full_url || !*full_url) {
    strncpy(out_origin, "현재 웹사이트", max_len - 1);
    out_origin[max_len - 1] = '\0';
    return;
  }

  const char* scheme_sep = strstr(full_url, "://");
  if (scheme_sep) {
    const char* path_start = strchr(scheme_sep + 3, '/');
    if (!path_start) path_start = strchr(scheme_sep + 3, '?');
    if (!path_start) path_start = strchr(scheme_sep + 3, '#');

    size_t origin_len = path_start ? (size_t)(path_start - full_url) : strlen(full_url);
    if (origin_len >= max_len) origin_len = max_len - 1;
    strncpy(out_origin, full_url, origin_len);
    out_origin[origin_len] = '\0';
  } else {
    strncpy(out_origin, full_url, max_len - 1);
    out_origin[max_len - 1] = '\0';
  }
}

int HandleExternalProtocolIfNeeded(HWND parent_hwnd, const char* url, const char* current_page_url) {
  if (!url || !*url) return 0;

  // 1. Extract scheme from url
  const char* colon = strchr(url, ':');
  if (!colon) return 0;

  size_t scheme_len = colon - url;
  if (scheme_len == 0 || scheme_len >= 64) return 0;

  char scheme[64] = {0};
  for (size_t i = 0; i < scheme_len; i++) {
    char c = url[i];
    if (!isalnum((unsigned char)c) && c != '+' && c != '-' && c != '.') {
      return 0; // Invalid scheme characters
    }
    scheme[i] = (char)tolower((unsigned char)c);
  }
  scheme[scheme_len] = '\0';

  // 2. Ignore standard web/browser schemes
  if (is_standard_web_scheme(scheme)) {
    return 0;
  }

  // 3. Verify protocol is registered in Windows Registry
  wchar_t w_scheme[64] = {0};
  MultiByteToWideChar(CP_UTF8, 0, scheme, -1, w_scheme, sizeof(w_scheme)/sizeof(w_scheme[0]));

  wchar_t app_name[256] = {0};
  if (!is_registered_url_protocol(w_scheme, app_name, sizeof(app_name)/sizeof(app_name[0]))) {
    return 0; // Not a registered URL protocol in Windows, let CEF handle/reject
  }

  // 4. Extract site origin
  char origin[512] = {0};
  extract_origin(current_page_url, origin, sizeof(origin));

  // Convert URL to UTF-16 for ShellExecuteW
  int w_url_len = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
  if (w_url_len <= 0) return 0;
  wchar_t* w_url = (wchar_t*)malloc(w_url_len * sizeof(wchar_t));
  if (!w_url) return 0;
  MultiByteToWideChar(CP_UTF8, 0, url, -1, w_url, w_url_len);

  // 5. Check if already marked as "always allow"
  if (is_protocol_always_allowed(origin, scheme)) {
    ShellExecuteW(parent_hwnd ? parent_hwnd : NULL, L"open", w_url, NULL, NULL, SW_SHOWNORMAL);
    free(w_url);
    return 1;
  }

  // 6. Show TaskDialog confirmation dialog
  wchar_t w_origin[512] = {0};
  MultiByteToWideChar(CP_UTF8, 0, origin, -1, w_origin, sizeof(w_origin)/sizeof(w_origin[0]));

  wchar_t main_inst[512];
  swprintf(main_inst, sizeof(main_inst)/sizeof(main_inst[0]), L"'%s'을(를) 여시겠습니까?", app_name);

  wchar_t content_text[1024];
  swprintf(content_text, sizeof(content_text)/sizeof(content_text[0]),
           L"%s에서 이 애플리케이션을 열려고 합니다.", w_origin);

  wchar_t verify_text[512];
  swprintf(verify_text, sizeof(verify_text)/sizeof(verify_text[0]),
           L"%s에서 %s 링크를 항상 열도록 허용", w_origin, w_scheme);

  TASKDIALOGCONFIG tc = {0};
  tc.cbSize = sizeof(tc);
  tc.hwndParent = parent_hwnd ? parent_hwnd : GetActiveWindow();
  tc.hInstance = GetModuleHandle(NULL);
  tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  tc.pszWindowTitle = L"외부 애플리케이션 열기";
  tc.pszMainIcon = TD_INFORMATION_ICON;
  tc.pszMainInstruction = main_inst;
  tc.pszContent = content_text;
  tc.pszVerificationText = verify_text;

  TASKDIALOG_BUTTON buttons[2];
  buttons[0].nButtonID = IDOK;
  buttons[0].pszButtonText = L"열기";
  buttons[1].nButtonID = IDCANCEL;
  buttons[1].pszButtonText = L"취소";

  tc.pButtons = buttons;
  tc.cButtons = 2;
  tc.nDefaultButton = IDOK;

  int nButton = 0;
  BOOL bVerificationChecked = FALSE;

  HRESULT hr = TaskDialogIndirect(&tc, &nButton, NULL, &bVerificationChecked);
  if (SUCCEEDED(hr)) {
    if (nButton == IDOK) {
      if (bVerificationChecked) {
        save_allowed_protocol(origin, scheme);
      }
      ShellExecuteW(parent_hwnd ? parent_hwnd : NULL, L"open", w_url, NULL, NULL, SW_SHOWNORMAL);
    }
  } else {
    // Fallback if TaskDialogIndirect fails
    int res = MessageBoxW(parent_hwnd ? parent_hwnd : GetActiveWindow(),
                          content_text, main_inst, MB_YESNO | MB_ICONQUESTION);
    if (res == IDYES) {
      ShellExecuteW(parent_hwnd ? parent_hwnd : NULL, L"open", w_url, NULL, NULL, SW_SHOWNORMAL);
    }
  }

  free(w_url);
  return 1; // Handled externally, cancel internal navigation
}

int HandleExternalProtocolForBrowser(HWND parent_hwnd, const char* url, struct _cef_browser_t* browser, struct _cef_frame_t* frame) {
  if (!url || !*url) return 0;

  // Quick check: does url contain a scheme colon?
  const char* colon = strchr(url, ':');
  if (!colon) return 0;
  size_t scheme_len = colon - url;
  if (scheme_len == 0 || scheme_len >= 64) return 0;

  char scheme[64] = {0};
  for (size_t i = 0; i < scheme_len; i++) {
    char c = url[i];
    if (!isalnum((unsigned char)c) && c != '+' && c != '-' && c != '.') {
      return 0;
    }
    scheme[i] = (char)tolower((unsigned char)c);
  }
  scheme[scheme_len] = '\0';

  if (is_standard_web_scheme(scheme)) {
    return 0;
  }

  // Extract current page URL from browser/frame
  char cur_page_url[2048] = {0};
  if (browser) {
    cef_frame_t* mf = browser->get_main_frame(browser);
    if (mf) {
      cef_string_userfree_t mf_url = mf->get_url(mf);
      if (mf_url && mf_url->length > 0) {
        cef_string_utf8_t u8 = {};
        cef_string_to_utf8(mf_url->str, mf_url->length, &u8);
        if (u8.str && u8.length > 0 && strcmp(u8.str, "about:blank") != 0) {
          strncpy(cur_page_url, u8.str, sizeof(cur_page_url) - 1);
        }
        cef_string_utf8_clear(&u8);
      }
      if (mf_url) cef_string_userfree_free(mf_url);
      mf->base.release(&mf->base);
    }
  }
  if (cur_page_url[0] == '\0' && frame) {
    cef_string_userfree_t f_url = frame->get_url(frame);
    if (f_url && f_url->length > 0) {
      cef_string_utf8_t u8 = {};
      cef_string_to_utf8(f_url->str, f_url->length, &u8);
      if (u8.str && u8.length > 0) {
        strncpy(cur_page_url, u8.str, sizeof(cur_page_url) - 1);
      }
      cef_string_utf8_clear(&u8);
    }
    if (f_url) cef_string_userfree_free(f_url);
  }

  return HandleExternalProtocolIfNeeded(parent_hwnd, url, cur_page_url);
}
