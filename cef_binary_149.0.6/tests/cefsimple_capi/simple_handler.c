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

// Global instance pointer.
static simple_handler_t *g_instance = NULL;

// cef_browser_t *g_ui_browser = NULL;
// cef_browser_t *g_content_browser = NULL;
char g_startup_url[1024] = "https://www.google.com";

// Forward declarations for handler create functions.
simple_display_handler_t *display_handler_create(simple_handler_t *parent);
simple_life_span_handler_t *life_span_handler_create(simple_handler_t *parent);
simple_load_handler_t *load_handler_create(simple_handler_t *parent);
simple_request_handler_t *request_handler_create(simple_handler_t *parent);

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

  // Create sub-handlers.
  handler->display_handler = display_handler_create(handler);
  CHECK(handler->display_handler);
  handler->life_span_handler = life_span_handler_create(handler);
  CHECK(handler->life_span_handler);
  handler->load_handler = load_handler_create(handler);
  CHECK(handler->load_handler);
  handler->request_handler = request_handler_create(handler);
  CHECK(handler->request_handler);

  // Initialize other fields.
  handler->is_alloy_style = is_alloy_style;
  browser_list_init(&handler->browser_list);
  handler->is_closing = 0;
 

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

void update_ui_tabs(browser_window_t* win_ctx) {
  if (!win_ctx || !win_ctx->ui_browser) return;

  char json[4096] = "[";
  for (int i = 0; i < win_ctx->tab_count; i++) {
    char tab_str[512];
    snprintf(tab_str, sizeof(tab_str), 
             "{\"id\":%d,\"title\":\"%s\",\"url\":\"%s\"}%s", 
             win_ctx->tabs[i].tab_id, 
             win_ctx->tabs[i].title, 
             win_ctx->tabs[i].url,
             (i == win_ctx->tab_count - 1) ? "" : ",");
    strcat(json, tab_str);
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

  char js_code[1536];
  snprintf(js_code, sizeof(js_code), 
           "if (window.updateNavState) { window.updateNavState(%d, %d, %d); } "
           "if (window.updateAddress) { window.updateAddress('%s'); }", 
           can_go_back, can_go_forward, is_loading, win_ctx->tabs[win_ctx->active_tab_index].url);

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
        } else if (strcmp(action, "new-tab") == 0) {
          if (win_ctx->tab_count < MAX_TABS) {
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
            cef_string_from_ascii("https://www.google.com", 22, &content_url);

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
            strcpy(win_ctx->tabs[next_idx].url, "https://www.google.com");
            win_ctx->tab_count++;

            cef_browser_host_create_browser(
                &content_window_info, &content_handler->client, &content_url,
                &browser_settings, NULL, NULL);
            cef_string_clear(&content_url);
          }
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
            int old_idx = win_ctx->active_tab_index;
            if (old_idx >= 0 && old_idx < win_ctx->tab_count && win_ctx->tabs[old_idx].hwnd) {
              ShowWindow(win_ctx->tabs[old_idx].hwnd, SW_HIDE);
            }

            win_ctx->active_tab_index = found_idx;
            if (win_ctx->tabs[found_idx].hwnd) {
              ShowWindow(win_ctx->tabs[found_idx].hwnd, SW_SHOW);
              
              RECT rect;
              GetClientRect(win_ctx->main_hwnd, &rect);
              MoveWindow(win_ctx->tabs[found_idx].hwnd, 1, 101, rect.right - 2, rect.bottom - 102, TRUE);

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
                  MoveWindow(win_ctx->tabs[new_active].hwnd, 1, 101, rect.right - 2, rect.bottom - 102, TRUE);
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
          create_browser_window("https://www.google.com");
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
                  MoveWindow(win_ctx->tabs[new_active].hwnd, 1, 101, rect.right - 2, rect.bottom - 102, TRUE);
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
                  if (win_ctx->tabs[new_active].hwnd) {
                    ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
                    RECT r;
                    GetClientRect(win_ctx->main_hwnd, &r);
                    MoveWindow(win_ctx->tabs[new_active].hwnd, 1, 101, r.right - 2, r.bottom - 102, TRUE);
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
