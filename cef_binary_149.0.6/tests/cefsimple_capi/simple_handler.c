// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_task_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_utils.h"

#if defined(OS_WIN)
#include <windows.h>
extern int GetUIHeightForWindow(HWND hwnd);
#endif

static void LogMsg(const char *format, ...) {
  FILE *f = fopen("C:\\projects\\lite_browser\\debug_c.txt", "a");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fclose(f);
  }
}

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const unsigned char* data, size_t input_length) {
  size_t output_length = 4 * ((input_length + 2) / 3);
  char* encoded_data = (char*)malloc(output_length + 1);
  if (encoded_data == NULL) return NULL;

  for (size_t i = 0, j = 0; i < input_length;) {
    size_t remaining = input_length - i;
    uint32_t octet_a = (unsigned char)data[i++];
    uint32_t octet_b = (remaining > 1) ? (unsigned char)data[i++] : 0;
    uint32_t octet_c = (remaining > 2) ? (unsigned char)data[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
    encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
    encoded_data[j++] = (remaining > 1) ? base64_table[(triple >> 6) & 0x3F] : '=';
    encoded_data[j++] = (remaining > 2) ? base64_table[triple & 0x3F] : '=';
  }
  encoded_data[output_length] = '\0';
  return encoded_data;
}

static int get_base64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static unsigned char* base64_decode(const char* src, size_t* out_len) {
  size_t len = strlen(src);
  if (len % 4 != 0) return NULL;

  size_t padding = 0;
  if (len > 0 && src[len - 1] == '=') padding++;
  if (len > 1 && src[len - 2] == '=') padding++;

  size_t decoded_len = (len / 4) * 3 - padding;
  unsigned char* out = (unsigned char*)malloc(decoded_len + 1);
  if (!out) return NULL;

  size_t i = 0, j = 0;
  while (i < len) {
    int a = get_base64_value(src[i++]);
    int b = get_base64_value(src[i++]);
    int c = get_base64_value(src[i++]);
    int d = get_base64_value(src[i++]);

    if (a < 0 || b < 0 || (c < 0 && src[i - 2] != '=') || (d < 0 && src[i - 1] != '=')) {
      free(out);
      return NULL;
    }

    uint32_t triple = (a << 18) + (b << 12) + ((c >= 0 ? c : 0) << 6) + (d >= 0 ? d : 0);

    if (j < decoded_len) out[j++] = (triple >> 16) & 0xFF;
    if (j < decoded_len) out[j++] = (triple >> 8) & 0xFF;
    if (j < decoded_len) out[j++] = triple & 0xFF;
  }

  out[decoded_len] = '\0';
  *out_len = decoded_len;
  return out;
}

static char* get_query_param(const char* query, const char* param_name) {
  size_t name_len = strlen(param_name);
  const char* p = query;
  while (p) {
    if (strncmp(p, param_name, name_len) == 0 && p[name_len] == '=') {
      const char* val_start = p + name_len + 1;
      const char* val_end = strchr(val_start, '&');
      size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

      char* decoded = (char*)malloc(val_len + 1);
      if (!decoded) return NULL;

      size_t i = 0, j = 0;
      while (i < val_len) {
        if (val_start[i] == '%' && i + 2 < val_len) {
          char hex[3] = {val_start[i + 1], val_start[i + 2], '\0'};
          decoded[j++] = (char)strtol(hex, NULL, 16);
          i += 3;
        } else if (val_start[i] == '+') {
          decoded[j++] = ' ';
          i++;
        } else {
          decoded[j++] = val_start[i];
          i++;
        }
      }
      decoded[j] = '\0';
      return decoded;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return NULL;
}

// Global instance pointer.
static simple_handler_t *g_instance = NULL;

// cef_browser_t *g_ui_browser = NULL;
// cef_browser_t *g_content_browser = NULL;
char g_startup_url[1024] = "https://gemini.google.com";

// Forward declarations for handler create functions.
simple_display_handler_t *display_handler_create(simple_handler_t *parent);
simple_life_span_handler_t *life_span_handler_create(simple_handler_t *parent);
simple_load_handler_t *load_handler_create(simple_handler_t *parent);
simple_request_handler_t *request_handler_create(simple_handler_t *parent);
simple_context_menu_handler_t *context_menu_handler_create(simple_handler_t *parent);

//
// Client handler implementation.
//

IMPLEMENT_REFCOUNTING_MANUAL(simple_handler_t, simple_handler, ref_count)

int CEF_CALLBACK simple_handler_release(cef_base_ref_counted_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    // Release all handlers.
    if (handler->display_handler) {
      handler->display_handler->handler.base.release(
          &handler->display_handler->handler.base);
    }
    if (handler->life_span_handler) {
      handler->life_span_handler->handler.base.release(
          &handler->life_span_handler->handler.base);
    }
    if (handler->load_handler) {
      handler->load_handler->handler.base.release(
          &handler->load_handler->handler.base);
    }
    if (handler->request_handler) {
      handler->request_handler->handler.base.release(
          &handler->request_handler->handler.base);
    }
    if (handler->context_menu_handler) {
      handler->context_menu_handler->handler.base.release(
          &handler->context_menu_handler->handler.base);
    }

    

    // Destroy the browser list.
    browser_list_destroy(&handler->browser_list);

    // Clear global instance if this is it.
    if (g_instance == handler) {
      g_instance = NULL;
    }

    free(handler);
    return 1;
  }
  return 0;
}

//
// Client handler getter implementations.
//

cef_context_menu_handler_t *CEF_CALLBACK
simple_handler_get_context_menu_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->context_menu_handler) {
    // Add reference before returning.
    handler->context_menu_handler->handler.base.add_ref(
        &handler->context_menu_handler->handler.base);
    return &handler->context_menu_handler->handler;
  }
  return NULL;
}

cef_display_handler_t *CEF_CALLBACK
simple_handler_get_display_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->display_handler) {
    // Add reference before returning.
    handler->display_handler->handler.base.add_ref(
        &handler->display_handler->handler.base);
    return &handler->display_handler->handler;
  }
  return NULL;
}

cef_life_span_handler_t *CEF_CALLBACK
simple_handler_get_life_span_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->life_span_handler) {
    // Add reference before returning.
    handler->life_span_handler->handler.base.add_ref(
        &handler->life_span_handler->handler.base);
    return &handler->life_span_handler->handler;
  }
  return NULL;
}

cef_load_handler_t *CEF_CALLBACK
simple_handler_get_load_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->load_handler) {
    // Add reference before returning.
    handler->load_handler->handler.base.add_ref(
        &handler->load_handler->handler.base);
    return &handler->load_handler->handler;
  }
  return NULL;
}

cef_request_handler_t *CEF_CALLBACK
simple_handler_get_request_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->request_handler) {
    // Add reference before returning.
    handler->request_handler->handler.base.add_ref(
        &handler->request_handler->handler.base);
    return &handler->request_handler->handler;
  }
  return NULL;
}

//
// Public API implementation.
//

simple_handler_t *simple_handler_create(int is_alloy_style) {
  simple_handler_t *handler =
      (simple_handler_t *)calloc(1, sizeof(simple_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->client.base, cef_client_t, simple_handler);

  // Set callbacks.
  handler->client.get_display_handler = simple_handler_get_display_handler;
  handler->client.get_life_span_handler = simple_handler_get_life_span_handler;
  handler->client.get_load_handler = simple_handler_get_load_handler;
  handler->client.get_request_handler = simple_handler_get_request_handler;
  handler->client.get_context_menu_handler = simple_handler_get_context_menu_handler;

  // Create sub-handlers.
  handler->display_handler = display_handler_create(handler);
  CHECK(handler->display_handler);
  handler->life_span_handler = life_span_handler_create(handler);
  CHECK(handler->life_span_handler);
  handler->load_handler = load_handler_create(handler);
  CHECK(handler->load_handler);
  handler->request_handler = request_handler_create(handler);
  CHECK(handler->request_handler);
  handler->context_menu_handler = context_menu_handler_create(handler);
  CHECK(handler->context_menu_handler);

  // Initialize other fields.
  handler->is_alloy_style = is_alloy_style;
  browser_list_init(&handler->browser_list);
  handler->is_closing = 0;
  handler->type = BROWSER_TYPE_CONTENT;
 

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  // Set global instance.
  if (!g_instance) {
    g_instance = handler;
  }

  return handler;
}

simple_handler_t *simple_handler_get_instance(void) { return g_instance; }

// Task for closing browsers on the UI thread.
typedef struct _close_browsers_task_t {
  cef_task_t task;
  atomic_int ref_count;
  simple_handler_t *handler;
  int force_close;
} close_browsers_task_t;

IMPLEMENT_REFCOUNTING_MANUAL(close_browsers_task_t, close_browsers_task,
                             ref_count)

int CEF_CALLBACK close_browsers_task_release(cef_base_ref_counted_t *self) {
  close_browsers_task_t *task = (close_browsers_task_t *)self;
  int count = atomic_fetch_sub(&task->ref_count, 1) - 1;
  if (count == 0) {
    // Don't release handler reference - we don't own it.
    free(task);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK close_browsers_task_execute(cef_task_t *self) {
  close_browsers_task_t *task = (close_browsers_task_t *)self;

  size_t count = browser_list_count(&task->handler->browser_list);
  if (count == 0) {
    return;
  }

  // Close all browsers.
  for (size_t i = 0; i < count; ++i) {
    cef_browser_t *browser = browser_list_get(&task->handler->browser_list, i);
    cef_browser_host_t *host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, task->force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_close_all_browsers(simple_handler_t *handler,
                                       int force_close) {
  CHECK(handler);

  if (!cef_currently_on(TID_UI)) {
    // Execute on the UI thread.
    close_browsers_task_t *task =
        (close_browsers_task_t *)calloc(1, sizeof(close_browsers_task_t));
    CHECK(task);

    INIT_CEF_BASE_REFCOUNTED(&task->task.base, cef_task_t, close_browsers_task);
    task->task.execute = close_browsers_task_execute;
    task->handler = handler;
    task->force_close = force_close;
    atomic_store(&task->ref_count, 1);

    cef_post_task(TID_UI, &task->task);
    return;
  }

  // Already on UI thread, execute directly.
  size_t count = browser_list_count(&handler->browser_list);
  if (count == 0) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    cef_browser_t *browser = browser_list_get(&handler->browser_list, i);
    cef_browser_host_t *host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_show_main_window(simple_handler_t *handler) {
  CHECK(handler);
  if (browser_list_count(&handler->browser_list) == 0) {
    return;
  }

  cef_browser_t *main_browser = browser_list_get(&handler->browser_list, 0);
  simple_handler_platform_show_window(handler, main_browser);
}

// Default platform implementations (can be overridden in platform-specific
// files).
#if !defined(OS_MAC)
void simple_handler_platform_show_window(simple_handler_t *handler,
                                         cef_browser_t *browser) {
  // Not implemented on this platform.
}
#endif

static void EscapeJsonString(const char* src, char* dest, size_t dest_len) {
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j < dest_len - 3; i++) {
    if (src[i] == '"') {
      dest[j++] = '\\';
      dest[j++] = '"';
    } else if (src[i] == '\\') {
      dest[j++] = '\\';
      dest[j++] = '\\';
    } else if (src[i] == '\n') {
      dest[j++] = '\\';
      dest[j++] = 'n';
    } else if (src[i] == '\r') {
      dest[j++] = '\\';
      dest[j++] = 'r';
    } else if (src[i] == '\t') {
      dest[j++] = '\\';
      dest[j++] = 't';
    } else {
      dest[j++] = src[i];
    }
  }
  dest[j] = '\0';
}

void update_ui_tabs(browser_window_t* win_ctx) {
  if (!win_ctx || !win_ctx->ui_browser) return;

  char json[4096] = "[";
  for (int i = 0; i < win_ctx->tab_count; i++) {
    char escaped_title[512] = {0};
    char escaped_url[1536] = {0};
    EscapeJsonString(win_ctx->tabs[i].title, escaped_title, sizeof(escaped_title));
    EscapeJsonString(win_ctx->tabs[i].url, escaped_url, sizeof(escaped_url));

    char tab_str[2100];
    snprintf(tab_str, sizeof(tab_str), 
             "{\"id\":%d,\"title\":\"%s\",\"url\":\"%s\"}%s", 
             win_ctx->tabs[i].tab_id, 
             escaped_title, 
             escaped_url,
             (i == win_ctx->tab_count - 1) ? "" : ",");
    
    if (strlen(json) + strlen(tab_str) < sizeof(json) - 5) {
      strcat(json, tab_str);
    }
  }
  strcat(json, "]");

  char js_code[4500];
  snprintf(js_code, sizeof(js_code), "if (window.updateTabsList) { window.updateTabsList(%s, %d); }", 
           json, 
           (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) ? 
           win_ctx->tabs[win_ctx->active_tab_index].tab_id : 0);

  cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
  if (frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
    frame->execute_java_script(frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    frame->base.release(&frame->base);
  }
}

void update_ui_nav_state(browser_window_t* win_ctx) {
  if (!win_ctx || win_ctx->active_tab_index < 0 || win_ctx->active_tab_index >= win_ctx->tab_count) return;

  cef_browser_t* cb = win_ctx->tabs[win_ctx->active_tab_index].browser;
  if (!cb || !win_ctx->ui_browser) return;

  int can_go_back = cb->can_go_back(cb);
  int can_go_forward = cb->can_go_forward(cb);
  int is_loading = cb->is_loading(cb);

  char escaped_url[1536] = {0};
  EscapeJsonString(win_ctx->tabs[win_ctx->active_tab_index].url, escaped_url, sizeof(escaped_url));

  char js_code[2048];
  snprintf(js_code, sizeof(js_code), 
           "if (window.updateNavState) { window.updateNavState(%d, %d, %d); } "
           "if (window.updateAddress) { window.updateAddress(\"%s\"); }", 
           can_go_back, can_go_forward, is_loading, escaped_url);

  cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
  if (frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
    frame->execute_java_script(frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    frame->base.release(&frame->base);
  }
}

//
// Request handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_request_handler_t, request_handler,
                             ref_count)

int CEF_CALLBACK request_handler_on_before_browse(
    cef_request_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    cef_request_t *request, int user_gesture, int is_redirect) {

  cef_string_userfree_t url_userfree = request->get_url(request);
  if (url_userfree) {
    cef_string_utf8_t url_utf8 = {};
    cef_string_to_utf8(url_userfree->str, url_userfree->length, &url_utf8);

    if (url_utf8.str && strncmp(url_utf8.str, "http://ui-action/", 17) == 0) {
      const char *action = url_utf8.str + 17;
      LogMsg("Interrupted ui-action: %s\n", action);

      simple_request_handler_t *req_handler = (simple_request_handler_t*)self;
      simple_handler_t *parent_handler = req_handler->parent;
      browser_window_t *win_ctx = parent_handler->window_ctx;

      if (win_ctx) {
        cef_browser_t *cb = NULL;
        if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
          cb = win_ctx->tabs[win_ctx->active_tab_index].browser;
        }

        if (strcmp(action, "back") == 0) {
          if (cb) cb->go_back(cb);
        } else if (strcmp(action, "forward") == 0) {
          if (cb) cb->go_forward(cb);
        } else if (strcmp(action, "reload") == 0) {
          if (cb) cb->reload(cb);
        } else if (strncmp(action, "show-menu?", 10) == 0) {
          int click_x = 0, click_y = 0;
          if (sscanf(action + 10, "x=%d&y=%d", &click_x, &click_y) == 2) {
            POINT pt = {click_x, click_y};
            ClientToScreen(win_ctx->ui_hwnd, &pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1001, L"새 탭");
            AppendMenuW(hMenu, MF_STRING, 1002, L"새 창");
            AppendMenuW(hMenu, MF_STRING, 1007, L"마크다운 에디터 토글");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 1003, L"인쇄...");
            AppendMenuW(hMenu, MF_STRING, 1004, L"개발자 도구 (Inspect)");
            AppendMenuW(hMenu, MF_STRING, 1005, L"페이지 소스 보기");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 1006, L"종료");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, win_ctx->main_hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 1001) {
              CreateNewTab(win_ctx, "https://gemini.google.com");
            } else if (cmd == 1002) {
              create_browser_window("https://gemini.google.com");
            } else if (cmd == 1003) {
              if (cb) {
                cef_browser_host_t* host = cb->get_host(cb);
                if (host) {
                  host->print(host);
                  host->base.release(&host->base);
                }
              }
            } else if (cmd == 1004) {
              if (cb) {
                cef_window_info_t windowInfo = {};
                windowInfo.size = sizeof(cef_window_info_t);
                windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                windowInfo.parent_window = NULL;
                windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

                cef_browser_settings_t settings = {};
                settings.size = sizeof(cef_browser_settings_t);

                cef_browser_host_t* host = cb->get_host(cb);
                if (host) {
                  host->show_dev_tools(host, &windowInfo, NULL, &settings, NULL);
                  host->base.release(&host->base);
                }
              }
            } else if (cmd == 1005) {
              if (win_ctx && win_ctx->tab_count < MAX_TABS && win_ctx->active_tab_index >= 0) {
                char vs_url[1200] = "view-source:";
                strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);

                RECT rect;
                GetClientRect(win_ctx->main_hwnd, &rect);
                int width = rect.right;
                int height = rect.bottom;

                int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
                int content_y = ui_height + 1;
                int content_h = height - content_y - 1;

                cef_browser_settings_t browser_settings = {};
                browser_settings.size = sizeof(cef_browser_settings_t);

                cef_window_info_t content_window_info = {};
                content_window_info.size = sizeof(cef_window_info_t);
                content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                content_window_info.parent_window = win_ctx->main_hwnd;
                content_window_info.bounds.x = 1;
                content_window_info.bounds.y = content_y;
                content_window_info.bounds.width = width - 2;
                content_window_info.bounds.height = content_h;
                content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

                cef_string_t content_url = {};
                cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);

                simple_handler_t *content_handler = simple_handler_create(0);
                content_handler->window_ctx = win_ctx;

                int next_idx = win_ctx->tab_count;
                int max_id = 0;
                for(int k=0; k<win_ctx->tab_count; k++) {
                  if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
                }
                win_ctx->tabs[next_idx].tab_id = max_id + 1;
                win_ctx->tabs[next_idx].browser = NULL;
                win_ctx->tabs[next_idx].hwnd = NULL;
                strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
                strcpy(win_ctx->tabs[next_idx].url, vs_url);
                win_ctx->tab_count++;

                cef_browser_host_create_browser(
                    &content_window_info, &content_handler->client, &content_url,
                    &browser_settings, NULL, NULL);
                cef_string_clear(&content_url);
              }
            } else if (cmd == 1006) {
              PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
            } else if (cmd == 1007) {
              if (win_ctx) {
                win_ctx->show_editor = !win_ctx->show_editor;
                if (win_ctx->editor_hwnd) {
                  ShowWindow(win_ctx->editor_hwnd, win_ctx->show_editor ? SW_SHOW : SW_HIDE);
                }
                RECT rect;
                GetClientRect(win_ctx->main_hwnd, &rect);
                PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
              }
            }
          }
        } else if (strcmp(action, "print") == 0) {
          if (cb) {
            cef_browser_host_t* host = cb->get_host(cb);
            if (host) {
              host->print(host);
              host->base.release(&host->base);
            }
          }
        } else if (strcmp(action, "devtools") == 0) {
          if (cb) {
            cef_window_info_t windowInfo = {};
            windowInfo.size = sizeof(cef_window_info_t);
            windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            windowInfo.parent_window = NULL;
            windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

            cef_browser_settings_t settings = {};
            settings.size = sizeof(cef_browser_settings_t);

            cef_browser_host_t* host = cb->get_host(cb);
            if (host) {
              host->show_dev_tools(host, &windowInfo, NULL, &settings, NULL);
              host->base.release(&host->base);
            }
          }
        } else if (strcmp(action, "view-source") == 0) {
          if (win_ctx && win_ctx->tab_count < MAX_TABS && win_ctx->active_tab_index >= 0) {
            char vs_url[1200] = "view-source:";
            strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);

            RECT rect;
            GetClientRect(win_ctx->main_hwnd, &rect);
            int width = rect.right;
            int height = rect.bottom;

            int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
            int content_y = ui_height + 1;
            int content_h = height - content_y - 1;

            cef_browser_settings_t browser_settings = {};
            browser_settings.size = sizeof(cef_browser_settings_t);

            cef_window_info_t content_window_info = {};
            content_window_info.size = sizeof(cef_window_info_t);
            content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            content_window_info.parent_window = win_ctx->main_hwnd;
            content_window_info.bounds.x = 1;
            content_window_info.bounds.y = content_y;
            content_window_info.bounds.width = width - 2;
            content_window_info.bounds.height = content_h;
            content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

            cef_string_t content_url = {};
            cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);

            simple_handler_t *content_handler = simple_handler_create(0);
            content_handler->window_ctx = win_ctx;

            int next_idx = win_ctx->tab_count;
            int max_id = 0;
            for(int k=0; k<win_ctx->tab_count; k++) {
              if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
            }
            win_ctx->tabs[next_idx].tab_id = max_id + 1;
            win_ctx->tabs[next_idx].browser = NULL;
            win_ctx->tabs[next_idx].hwnd = NULL;
            strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
            strcpy(win_ctx->tabs[next_idx].url, vs_url);
            win_ctx->tab_count++;

            cef_browser_host_create_browser(
                &content_window_info, &content_handler->client, &content_url,
                &browser_settings, NULL, NULL);
            cef_string_clear(&content_url);
          }
        } else if (strcmp(action, "exit") == 0) {
          PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
        } else if (strcmp(action, "drag-window") == 0) {
          ReleaseCapture();
          SendMessage(win_ctx->main_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        } else if (strcmp(action, "window-minimize") == 0) {
          ShowWindow(win_ctx->main_hwnd, SW_MINIMIZE);
        } else if (strcmp(action, "window-maximize") == 0) {
          if (IsZoomed(win_ctx->main_hwnd)) {
            ShowWindow(win_ctx->main_hwnd, SW_RESTORE);
          } else {
            ShowWindow(win_ctx->main_hwnd, SW_MAXIMIZE);
          }
        } else if (strcmp(action, "window-close") == 0) {
          PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
        } else if (strncmp(action, "load?url=", 9) == 0) {
          if (cb) {
            const char *encoded_url = action + 9;
            char *decoded = (char *)malloc(strlen(encoded_url) + 1);
            if (decoded) {
              size_t i = 0, j = 0;
              while (encoded_url[i]) {
                if (encoded_url[i] == '%' && encoded_url[i + 1] && encoded_url[i + 2]) {
                  char hex[3] = {encoded_url[i + 1], encoded_url[i + 2], '\0'};
                  decoded[j++] = (char)strtol(hex, NULL, 16);
                  i += 3;
                } else if (encoded_url[i] == '+') {
                  decoded[j++] = ' ';
                  i++;
                } else {
                  decoded[j++] = encoded_url[i];
                  i++;
                }
              }
              decoded[j] = '\0';

              cef_string_t cef_url = {};
              cef_string_from_utf8(decoded, strlen(decoded), &cef_url);

              cef_frame_t *main_frame = cb->get_main_frame(cb);
              if (main_frame) {
                main_frame->load_url(main_frame, &cef_url);
                main_frame->base.release(&main_frame->base);
              }
              cef_string_clear(&cef_url);
              free(decoded);
            }
          }
        } else if (strcmp(action, "toggle-editor") == 0) {
          if (win_ctx) {
            win_ctx->show_editor = !win_ctx->show_editor;
            if (win_ctx->editor_hwnd) {
              ShowWindow(win_ctx->editor_hwnd, win_ctx->show_editor ? SW_SHOW : SW_HIDE);
            }
            RECT rect;
            GetClientRect(win_ctx->main_hwnd, &rect);
            PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
          }
        } else if (strncmp(action, "editor-save-file?", 17) == 0) {
          const char* query = action + 17;
          char* name = get_query_param(query, "name");
          char* content_base64 = get_query_param(query, "content");
          int success = 0;
          
          if (name && content_base64) {
            char* p = name;
            while (*p) {
              if (*p == '/' || *p == '\\' || *p == ':') {
                *p = '_';
              }
              p++;
            }
            
            CreateDirectoryA("C:\\projects\\lite_browser\\documents", NULL);
            
            size_t decoded_len = 0;
            unsigned char* decoded = base64_decode(content_base64, &decoded_len);
            if (decoded) {
              char filepath[MAX_PATH];
              snprintf(filepath, sizeof(filepath), "C:\\projects\\lite_browser\\documents\\%s", name);
              
              FILE* f = fopen(filepath, "wb");
              if (f) {
                fwrite(decoded, 1, decoded_len, f);
                fclose(f);
                success = 1;
              }
              free(decoded);
            }
          }
          
          if (win_ctx && win_ctx->editor_browser) {
            char js_callback[512];
            snprintf(js_callback, sizeof(js_callback), 
                     "if (window.onFileSaved) { window.onFileSaved(%d, '%s'); }", 
                     success, name ? name : "");
                     
            cef_frame_t* e_frame = win_ctx->editor_browser->get_main_frame(win_ctx->editor_browser);
            if (e_frame) {
              cef_string_t js_str = {};
              cef_string_from_utf8(js_callback, strlen(js_callback), &js_str);
              e_frame->execute_java_script(e_frame, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              e_frame->base.release(&e_frame->base);
            }
          }
          
          if (name) free(name);
          if (content_base64) free(content_base64);
        } else if (strncmp(action, "editor-load-file?", 17) == 0) {
          const char* query = action + 17;
          char* name = get_query_param(query, "name");
          
          if (name) {
            char* p = name;
            while (*p) {
              if (*p == '/' || *p == '\\' || *p == ':') {
                *p = '_';
              }
              p++;
            }
            
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "C:\\projects\\lite_browser\\documents\\%s", name);
            
            FILE* f = fopen(filepath, "rb");
            if (f) {
              fseek(f, 0, SEEK_END);
              long file_size = ftell(f);
              fseek(f, 0, SEEK_SET);
              
              unsigned char* buf = (unsigned char*)malloc(file_size + 1);
              if (buf) {
                fread(buf, 1, file_size, f);
                buf[file_size] = '\0';
                
                char* base64_content = base64_encode(buf, file_size);
                if (base64_content) {
                  if (win_ctx && win_ctx->editor_browser) {
                    size_t js_len = strlen(base64_content) + 256;
                    char* js_callback = (char*)malloc(js_len);
                    if (js_callback) {
                      snprintf(js_callback, js_len, 
                               "if (window.onFileLoaded) { window.onFileLoaded('%s', '%s'); }", 
                               name, base64_content);
                               
                      cef_frame_t* e_frame = win_ctx->editor_browser->get_main_frame(win_ctx->editor_browser);
                      if (e_frame) {
                        cef_string_t js_str = {};
                        cef_string_from_utf8(js_callback, strlen(js_callback), &js_str);
                        e_frame->execute_java_script(e_frame, &js_str, NULL, 0);
                        cef_string_clear(&js_str);
                        e_frame->base.release(&e_frame->base);
                      }
                      free(js_callback);
                    }
                    free(base64_content);
                  }
                }
                free(buf);
              }
              fclose(f);
            }
            free(name);
          }
        } else if (strcmp(action, "editor-list-files") == 0) {
          CreateDirectoryA("C:\\projects\\lite_browser\\documents", NULL);
          
          char search_path[MAX_PATH];
          snprintf(search_path, sizeof(search_path), "C:\\projects\\lite_browser\\documents\\*.md");
          
          WIN32_FIND_DATAA find_data;
          HANDLE hFind = FindFirstFileA(search_path, &find_data);
          
          size_t json_cap = 4096;
          char* json = (char*)malloc(json_cap);
          if (json) {
            strcpy(json, "[");
            int first = 1;
            
            if (hFind != INVALID_HANDLE_VALUE) {
              do {
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                  size_t needed = strlen(find_data.cFileName) + 5;
                  if (strlen(json) + needed >= json_cap) {
                    json_cap *= 2;
                    char* temp = (char*)realloc(json, json_cap);
                    if (!temp) break;
                    json = temp;
                  }
                  
                  if (!first) strcat(json, ",");
                  strcat(json, "\"");
                  strcat(json, find_data.cFileName);
                  strcat(json, "\"");
                  first = 0;
                }
              } while (FindNextFileA(hFind, &find_data));
              FindClose(hFind);
            }
            strcat(json, "]");
            
            if (win_ctx && win_ctx->editor_browser) {
              size_t js_len = strlen(json) + 256;
              char* js_callback = (char*)malloc(js_len);
              if (js_callback) {
                snprintf(js_callback, js_len, 
                         "if (window.onFilesListed) { window.onFilesListed(%s); }", 
                         json);
                         
                cef_frame_t* e_frame = win_ctx->editor_browser->get_main_frame(win_ctx->editor_browser);
                if (e_frame) {
                  cef_string_t js_str = {};
                  cef_string_from_utf8(js_callback, strlen(js_callback), &js_str);
                  e_frame->execute_java_script(e_frame, &js_str, NULL, 0);
                  cef_string_clear(&js_str);
                  e_frame->base.release(&e_frame->base);
                }
                free(js_callback);
              }
            }
            free(json);
          }
        } else if (strncmp(action, "send-prompt?", 12) == 0) {
          const char* query = action + 12;
          char* text_base64 = get_query_param(query, "text");
          
          if (text_base64) {
            if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
              cef_browser_t* content_browser = win_ctx->tabs[win_ctx->active_tab_index].browser;
              if (content_browser) {
                size_t js_len = strlen(text_base64) + 1024;
                char* js_code = (char*)malloc(js_len);
                if (js_code) {
                  snprintf(js_code, js_len,
                    "(function() {"
                    "  const base64 = '%s';"
                    "  const binary = atob(base64);"
                    "  const len = binary.length;"
                    "  const bytes = new Uint8Array(len);"
                    "  for (let i = 0; i < len; i++) {"
                    "    bytes[i] = binary.charCodeAt(i);"
                    "  }"
                    "  const text = new TextDecoder().decode(bytes);"
                    "  let el = document.querySelector('#prompt-textarea');"
                    "  if (!el) {"
                    "    el = document.querySelector('div[contenteditable=\"true\"]') || document.querySelector('[contenteditable=\"true\"]');"
                    "  }"
                    "  if (!el) {"
                    "    el = document.querySelector('textarea') || document.querySelector('input[type=\"text\"]');"
                    "  }"
                    "  if (el) {"
                    "    el.focus();"
                    "    if (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT') {"
                    "      el.value = text;"
                    "    } else {"
                    "      el.innerText = text;"
                    "    }"
                    "    const inputEvent = new Event('input', { bubbles: true });"
                    "    el.dispatchEvent(inputEvent);"
                    "    const changeEvent = new Event('change', { bubbles: true });"
                    "    el.dispatchEvent(changeEvent);"
                    "    el.focus();"
                    "  }"
                    "})();",
                    text_base64
                  );
                  
                  cef_frame_t* c_frame = content_browser->get_main_frame(content_browser);
                  if (c_frame) {
                    cef_string_t js_str = {};
                    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
                    c_frame->execute_java_script(c_frame, &js_str, NULL, 0);
                    cef_string_clear(&js_str);
                    c_frame->base.release(&c_frame->base);
                  }
                  free(js_code);
                }
              }
            }
            free(text_base64);
          }
        } else if (strcmp(action, "new-tab") == 0) {
          CreateNewTab(win_ctx, "https://gemini.google.com");
        } else if (strncmp(action, "switch-tab?id=", 14) == 0) {
          int target_id = atoi(action + 14);
          int found_idx = -1;
          for (int i = 0; i < win_ctx->tab_count; i++) {
            if (win_ctx->tabs[i].tab_id == target_id) {
              found_idx = i;
              break;
            }
          }
          if (found_idx != -1 && found_idx != win_ctx->active_tab_index) {
            for (int k = 0; k < win_ctx->tab_count; k++) {
              if (k != found_idx && win_ctx->tabs[k].hwnd) {
                ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
              }
            }

            win_ctx->active_tab_index = found_idx;
            win_ctx->tabs[found_idx].is_loaded = 1;
            if (win_ctx->tabs[found_idx].hwnd) {
              ShowWindow(win_ctx->tabs[found_idx].hwnd, SW_SHOW);
              
              RECT rect;
              GetClientRect(win_ctx->main_hwnd, &rect);
              PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));

              cef_browser_host_t* host = win_ctx->tabs[found_idx].browser->get_host(win_ctx->tabs[found_idx].browser);
              if (host) {
                host->was_resized(host);
                host->set_focus(host, 1);
                host->base.release(&host->base);
              }
            }
            update_ui_tabs(win_ctx);
            update_ui_nav_state(win_ctx);
          }
        } else if (strncmp(action, "close-tab?id=", 13) == 0) {
          int target_id = atoi(action + 13);
          int found_idx = -1;
          for (int i = 0; i < win_ctx->tab_count; i++) {
            if (win_ctx->tabs[i].tab_id == target_id) {
              found_idx = i;
              break;
            }
          }
          if (found_idx != -1) {
            if (win_ctx->tab_count <= 1) {
              PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
            } else {
              cef_browser_t* target_browser = win_ctx->tabs[found_idx].browser;
              if (target_browser) {
                cef_browser_host_t* host = target_browser->get_host(target_browser);
                if (host) {
                  host->close_browser(host, 1);
                  host->base.release(&host->base);
                }
              }

              int old_active = win_ctx->active_tab_index;
              int new_active = old_active;
              if (old_active == found_idx) {
                new_active = (found_idx == win_ctx->tab_count - 1) ? found_idx - 1 : found_idx;
                if (win_ctx->tabs[new_active].hwnd) {
                  ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
                  RECT rect;
                  GetClientRect(win_ctx->main_hwnd, &rect);
                  PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
                  cef_browser_host_t* host = win_ctx->tabs[new_active].browser->get_host(win_ctx->tabs[new_active].browser);
                  if (host) {
                    host->was_resized(host);
                    host->set_focus(host, 1);
                    host->base.release(&host->base);
                  }
                }
              } else if (old_active > found_idx) {
                new_active = old_active - 1;
              }

              for (int i = found_idx; i < win_ctx->tab_count - 1; i++) {
                win_ctx->tabs[i] = win_ctx->tabs[i + 1];
              }
              win_ctx->tab_count--;
              win_ctx->active_tab_index = new_active;

              update_ui_tabs(win_ctx);
              update_ui_nav_state(win_ctx);
            }
          }
        } else if (strcmp(action, "new-window") == 0) {
          create_browser_window("https://gemini.google.com");
        } else if (strncmp(action, "detach-tab?id=", 14) == 0) {
          int target_id = atoi(action + 14);
          int found_idx = -1;
          for (int i = 0; i < win_ctx->tab_count; i++) {
            if (win_ctx->tabs[i].tab_id == target_id) {
              found_idx = i;
              break;
            }
          }
          if (found_idx != -1 && win_ctx->tab_count > 1) {
            cef_browser_t* detached_browser = win_ctx->tabs[found_idx].browser;
            HWND detached_hwnd = win_ctx->tabs[found_idx].hwnd;
            char target_url[1024];
            char target_title[256];
            strcpy(target_url, win_ctx->tabs[found_idx].url);
            strcpy(target_title, win_ctx->tabs[found_idx].title);

            browser_window_t* new_win = create_browser_window_for_detached(
                detached_browser, detached_hwnd, target_url, target_title, CW_USEDEFAULT, CW_USEDEFAULT);

            if (new_win) {
              int old_active = win_ctx->active_tab_index;
              int new_active = old_active;
              if (old_active == found_idx) {
                new_active = (found_idx == win_ctx->tab_count - 1) ? found_idx - 1 : found_idx;
                if (win_ctx->tabs[new_active].hwnd) {
                  ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
                  RECT rect;
                  GetClientRect(win_ctx->main_hwnd, &rect);
                  int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
                  int content_y = ui_height + 1;
                  int content_h = rect.bottom - content_y - 1;
                  MoveWindow(win_ctx->tabs[new_active].hwnd, 1, content_y, rect.right - 2, content_h, TRUE);
                  cef_browser_host_t* host = win_ctx->tabs[new_active].browser->get_host(win_ctx->tabs[new_active].browser);
                  if (host) {
                    host->was_resized(host);
                    host->set_focus(host, 1);
                    host->base.release(&host->base);
                  }
                }
              } else if (old_active > found_idx) {
                new_active = old_active - 1;
              }

              for (int i = found_idx; i < win_ctx->tab_count - 1; i++) {
                win_ctx->tabs[i] = win_ctx->tabs[i + 1];
              }
              win_ctx->tab_count--;
              win_ctx->active_tab_index = new_active;

              cef_browser_host_t* host = detached_browser->get_host(detached_browser);
              cef_client_t* client = host->get_client(host);
              if (client) {
                simple_handler_t* detached_handler = (simple_handler_t*)client;
                detached_handler->window_ctx = new_win;
              }
              host->base.release(&host->base);

              update_ui_tabs(win_ctx);
              update_ui_nav_state(win_ctx);
            }
          }
        } else if (strncmp(action, "drag-end?id=", 12) == 0) {
          int target_id = atoi(action + 12);
          POINT pt;
          GetCursorPos(&pt);
          RECT rect;
          GetWindowRect(win_ctx->main_hwnd, &rect);
          if (!PtInRect(&rect, pt)) {
            int found_idx = -1;
            for (int i = 0; i < win_ctx->tab_count; i++) {
              if (win_ctx->tabs[i].tab_id == target_id) {
                found_idx = i;
                break;
              }
            }
            if (found_idx != -1 && win_ctx->tab_count > 1) {
              cef_browser_t* detached_browser = win_ctx->tabs[found_idx].browser;
              HWND detached_hwnd = win_ctx->tabs[found_idx].hwnd;
              char target_url[1024];
              char target_title[256];
              strcpy(target_url, win_ctx->tabs[found_idx].url);
              strcpy(target_title, win_ctx->tabs[found_idx].title);

              browser_window_t* new_win = create_browser_window_for_detached(
                  detached_browser, detached_hwnd, target_url, target_title, pt.x - 100, pt.y - 10);

              if (new_win) {
                int old_active = win_ctx->active_tab_index;
                int new_active = old_active;
                if (old_active == found_idx) {
                  new_active = (found_idx == win_ctx->tab_count - 1) ? found_idx - 1 : found_idx;
                    ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
                    RECT r;
                    GetClientRect(win_ctx->main_hwnd, &r);
                    PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
                    cef_browser_host_t* host = win_ctx->tabs[new_active].browser->get_host(win_ctx->tabs[new_active].browser);
                    if (host) {
                      host->was_resized(host);
                      host->set_focus(host, 1);
                      host->base.release(&host->base);
                    }
                } else if (old_active > found_idx) {
                  new_active = old_active - 1;
                }

                for (int i = found_idx; i < win_ctx->tab_count - 1; i++) {
                  win_ctx->tabs[i] = win_ctx->tabs[i + 1];
                }
                win_ctx->tab_count--;
                win_ctx->active_tab_index = new_active;

                cef_browser_host_t* host = detached_browser->get_host(detached_browser);
                cef_client_t* client = host->get_client(host);
                if (client) {
                  simple_handler_t* detached_handler = (simple_handler_t*)client;
                  detached_handler->window_ctx = new_win;
                }
                host->base.release(&host->base);

                update_ui_tabs(win_ctx);
                update_ui_nav_state(win_ctx);
              }
            }
          }
        }
      }

      cef_string_utf8_clear(&url_utf8);
      cef_string_userfree_free(url_userfree);

      browser->base.release(&browser->base);
      frame->base.release(&frame->base);
      request->base.release(&request->base);

      return 1;
    }

    cef_string_utf8_clear(&url_utf8);
    cef_string_userfree_free(url_userfree);
  }

  browser->base.release(&browser->base);
  frame->base.release(&frame->base);
  request->base.release(&request->base);

  return 0;
}

simple_request_handler_t *request_handler_create(simple_handler_t *parent) {
  simple_request_handler_t *handler =
      (simple_request_handler_t *)calloc(1, sizeof(simple_request_handler_t));
  CHECK(handler);

  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_request_handler_t,
                           request_handler);

  handler->handler.on_before_browse = request_handler_on_before_browse;
  handler->parent = parent;

  atomic_store(&handler->ref_count, 1);

  return handler;
}

//
// Context menu handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_context_menu_handler_t, context_menu_handler,
                             ref_count)

void CEF_CALLBACK context_menu_on_before_context_menu(
    cef_context_menu_handler_t* self,
    cef_browser_t* browser,
    cef_frame_t* frame,
    cef_context_menu_params_t* params,
    cef_menu_model_t* model) {
  
  LogMsg("on_before_context_menu: start\n");
  
  // Clear default items to prevent CEF from displaying anything.
  model->clear(model);
  
  int click_x = params->get_xcoord(params);
  int click_y = params->get_ycoord(params);
  POINT pt = {click_x, click_y};
  
  cef_browser_host_t* host = browser->get_host(browser);
  if (host) {
    HWND hwnd = host->get_window_handle(host);
    ClientToScreen(hwnd, &pt);
    
    HMENU hMenu = CreatePopupMenu();
    int can_back = browser->can_go_back(browser);
    int can_forward = browser->can_go_forward(browser);
    
    AppendMenuW(hMenu, MF_STRING | (can_back ? 0 : MF_GRAYED), MENU_ID_BACK, L"뒤로 가기");
    AppendMenuW(hMenu, MF_STRING | (can_forward ? 0 : MF_GRAYED), MENU_ID_FORWARD, L"앞으로 가기");
    AppendMenuW(hMenu, MF_STRING, MENU_ID_RELOAD, L"새로고침");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_ID_PRINT, L"인쇄...");
    AppendMenuW(hMenu, MF_STRING, MENU_ID_VIEW_SOURCE, L"페이지 소스 보기");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_ID_USER_FIRST, L"검사 (Inspect)");
    AppendMenuW(hMenu, MF_STRING, 1007, L"마크다운 에디터 토글");
    
    LogMsg("on_before_context_menu: calling TrackPopupMenu (blocking)\n");
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    LogMsg("on_before_context_menu: TrackPopupMenu returned cmd=%d\n", cmd);
    DestroyMenu(hMenu);
    
    if (cmd > 0) {
      if (cmd == 1007) {
        simple_context_menu_handler_t* ctx_handler = (simple_context_menu_handler_t*)self;
        browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
        if (win_ctx) {
          win_ctx->show_editor = !win_ctx->show_editor;
          if (win_ctx->editor_hwnd) {
            ShowWindow(win_ctx->editor_hwnd, win_ctx->show_editor ? SW_SHOW : SW_HIDE);
          }
          RECT rect;
          GetClientRect(win_ctx->main_hwnd, &rect);
          PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
        }
      } else if (cmd == MENU_ID_USER_FIRST) {
        LogMsg("on_before_context_menu: show dev tools\n");
        cef_window_info_t windowInfo = {};
        windowInfo.size = sizeof(cef_window_info_t);
        windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        windowInfo.parent_window = NULL;
        windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
        
        cef_browser_settings_t settings = {};
        settings.size = sizeof(cef_browser_settings_t);
        
        cef_point_t inspect_at = { click_x, click_y };
        host->show_dev_tools(host, &windowInfo, NULL, &settings, &inspect_at);
      } else if (cmd == MENU_ID_BACK) {
        browser->go_back(browser);
      } else if (cmd == MENU_ID_FORWARD) {
        browser->go_forward(browser);
      } else if (cmd == MENU_ID_RELOAD) {
        browser->reload(browser);
      } else if (cmd == MENU_ID_PRINT) {
        host->print(host);
      } else if (cmd == MENU_ID_VIEW_SOURCE) {
        LogMsg("on_before_context_menu: view source trigger\n");
        simple_context_menu_handler_t* ctx_handler = (simple_context_menu_handler_t*)self;
        browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
        if (win_ctx && win_ctx->active_tab_index >= 0) {
          char vs_url[1200] = "view-source:";
          strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);
          
          RECT rect;
          GetClientRect(win_ctx->main_hwnd, &rect);
          int width = rect.right;
          int height = rect.bottom;
          
          cef_browser_settings_t browser_settings = {};
          browser_settings.size = sizeof(cef_browser_settings_t);
          
          cef_window_info_t content_window_info = {};
          content_window_info.size = sizeof(cef_window_info_t);
          content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
          content_window_info.parent_window = win_ctx->main_hwnd;
          content_window_info.bounds.x = 1;
          content_window_info.bounds.y = 101;
          content_window_info.bounds.width = width - 2;
          content_window_info.bounds.height = height - 102;
          content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
          
          cef_string_t content_url = {};
          cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);
          
          simple_handler_t *content_handler = simple_handler_create(0);
          content_handler->window_ctx = win_ctx;
          
          int next_idx = win_ctx->tab_count;
          int max_id = 0;
          for(int k=0; k<win_ctx->tab_count; k++) {
            if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
          }
          win_ctx->tabs[next_idx].tab_id = max_id + 1;
          win_ctx->tabs[next_idx].browser = NULL;
          win_ctx->tabs[next_idx].hwnd = NULL;
          strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
          strcpy(win_ctx->tabs[next_idx].url, vs_url);
          win_ctx->tab_count++;
          
          cef_browser_host_create_browser(
              &content_window_info, &content_handler->client, &content_url,
              &browser_settings, NULL, NULL);
          cef_string_clear(&content_url);
        }
      }
    }
    
    host->base.release(&host->base);
  }
  
  LogMsg("on_before_context_menu: end\n");
}

simple_context_menu_handler_t *context_menu_handler_create(simple_handler_t *parent) {
  simple_context_menu_handler_t *handler =
      (simple_context_menu_handler_t *)calloc(1, sizeof(simple_context_menu_handler_t));
  CHECK(handler);

  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_context_menu_handler_t,
                           context_menu_handler);

  handler->handler.on_before_context_menu = context_menu_on_before_context_menu;
  handler->handler.on_context_menu_command = NULL;
  handler->handler.run_context_menu = NULL;
  handler->parent = parent;

  atomic_store(&handler->ref_count, 1);

  return handler;
}

void CreateNewTab(browser_window_t* win_ctx, const char* url) {
  if (!win_ctx) return;
  if (win_ctx->tab_count >= MAX_TABS) return;

  RECT rect;
  GetClientRect(win_ctx->main_hwnd, &rect);
  int width = rect.right;
  int height = rect.bottom;

  int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
  int content_y = ui_height + 1;
  int content_h = height - content_y - 1;

  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  cef_window_info_t content_window_info = {};
  content_window_info.size = sizeof(cef_window_info_t);
  content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  content_window_info.parent_window = win_ctx->main_hwnd;
  content_window_info.bounds.x = 1;
  content_window_info.bounds.y = content_y;
  content_window_info.bounds.width = width - 2;
  content_window_info.bounds.height = content_h;
  content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t content_url = {};
  if (url && strlen(url) > 0) {
    cef_string_from_utf8(url, strlen(url), &content_url);
  } else {
    cef_string_from_ascii("https://gemini.google.com", 25, &content_url);
  }

  simple_handler_t *content_handler = simple_handler_create(0);
  content_handler->window_ctx = win_ctx;

  int next_idx = win_ctx->tab_count;
  int max_id = 0;
  for(int k=0; k<win_ctx->tab_count; k++) {
    if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
  }
  win_ctx->tabs[next_idx].tab_id = max_id + 1;
  win_ctx->tabs[next_idx].browser = NULL;
  win_ctx->tabs[next_idx].hwnd = NULL;
  strcpy(win_ctx->tabs[next_idx].title, "새 탭");
  if (url && strlen(url) > 0) {
    strncpy(win_ctx->tabs[next_idx].url, url, sizeof(win_ctx->tabs[next_idx].url) - 1);
  } else {
    strcpy(win_ctx->tabs[next_idx].url, "https://gemini.google.com");
  }
  win_ctx->tabs[next_idx].is_loaded = 0;
  win_ctx->tabs[next_idx].tab_handler = content_handler;
  win_ctx->active_tab_index = next_idx;
  win_ctx->tab_count++;
  update_ui_tabs(win_ctx);

  cef_browser_host_create_browser(
      &content_window_info, &content_handler->client, &content_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&content_url);
}
