// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>


#if defined(OS_WIN)
#include <windows.h>
#endif

#include "include/capi/cef_app_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_handler.h"
extern int GetUIHeightForWindow(HWND hwnd);
#include "tests/cefsimple_capi/simple_utils.h"
#include "tests/cefsimple_capi/browser_context.h"

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

//
// Life span handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_life_span_handler_t, life_span_handler,
                             ref_count)

void CEF_CALLBACK life_span_handler_on_after_created(
    cef_life_span_handler_t *self, cef_browser_t *browser) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)self;

  LogMsg("life_span_handler_on_after_created: browser=%p\n", browser);

  browser_list_add(&handler->parent->browser_list, browser);

  browser_window_t *win_ctx = handler->parent->window_ctx;

  if (win_ctx) {
    cef_browser_host_t *host = browser->get_host(browser);
    HWND hwnd = host->get_window_handle(host);
    host->base.release(&host->base);

    if (handler->parent->type == BROWSER_TYPE_UI) {
      win_ctx->ui_browser = browser;
      browser->base.add_ref(&browser->base);
      win_ctx->ui_hwnd = hwnd;
      LogMsg("Set win_ctx->ui_browser = %p, hwnd = %p\n", browser, hwnd);
    } else {
      int found_slot = -1;
      int is_right_slot = 0;
      for (int i = 0; i < win_ctx->tab_count; i++) {
        if (win_ctx->tabs[i].tab_handler == handler->parent) {
          found_slot = i;
          is_right_slot = 0;
          break;
        }
        if (win_ctx->tabs[i].right_tab_handler == handler->parent) {
          found_slot = i;
          is_right_slot = 1;
          break;
        }
      }
      
      // Fallback: If not found by handler pointer, find first NULL browser slot
      if (found_slot == -1) {
        for (int i = 0; i < win_ctx->tab_count; i++) {
          if (win_ctx->tabs[i].browser == NULL) {
            found_slot = i;
            is_right_slot = 0;
            break;
          }
        }
      }

      if (found_slot != -1) {
        int i = found_slot;
        if (is_right_slot) {
          win_ctx->tabs[i].right_browser = browser;
          browser->base.add_ref(&browser->base);
          win_ctx->tabs[i].right_hwnd = hwnd;
          ShowWindow(hwnd, SW_SHOW);
          LogMsg("Assigned right browser %p to tab %d\n", browser, win_ctx->tabs[i].tab_id);
        } else {
          win_ctx->tabs[i].browser = browser;
          browser->base.add_ref(&browser->base);
          win_ctx->tabs[i].hwnd = hwnd;
          LogMsg("Assigned browser %p to tab %d via handler matching\n", browser, win_ctx->tabs[i].tab_id);

          if (win_ctx->tabs[i].is_loaded) {
            for (int k = 0; k < win_ctx->tab_count; k++) {
              if (k != i) {
                if (win_ctx->tabs[k].hwnd) ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
                if (win_ctx->tabs[k].right_hwnd) ShowWindow(win_ctx->tabs[k].right_hwnd, SW_HIDE);
              }
            }
            win_ctx->active_tab_index = i;
            ShowWindow(hwnd, SW_SHOW);
          } else {
            ShowWindow(hwnd, SW_HIDE);
            LogMsg("Defer showing tab %d (HWND %p) until loaded\n", win_ctx->tabs[i].tab_id, hwnd);
          }
        }
      }
      // Notify UI about new tabs
      update_ui_tabs(win_ctx);
      update_ui_nav_state(win_ctx);
    }

    RECT r;
    GetClientRect(win_ctx->main_hwnd, &r);
    PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
  }

  browser->base.release(&browser->base);
}

int CEF_CALLBACK life_span_handler_do_close(cef_life_span_handler_t *self,
                                             cef_browser_t *browser) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)self;

  if (browser_list_count(&handler->parent->browser_list) == 1) {
    handler->parent->is_closing = 1;
  }

  browser->base.release(&browser->base);
  return 0;
}

int CEF_CALLBACK life_span_handler_on_before_popup(
    struct _cef_life_span_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    int popup_id,
    const cef_string_t* target_url,
    const cef_string_t* target_frame_name,
    cef_window_open_disposition_t target_disposition,
    int user_gesture,
    const cef_popup_features_t* popupFeatures,
    struct _cef_window_info_t* windowInfo,
    struct _cef_client_t** client,
    struct _cef_browser_settings_t* settings,
    struct _cef_dictionary_value_t** extra_info,
    int* no_javascript_access) {

  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;
  LogMsg("life_span_handler_on_before_popup: target_url=%s\n", target_url && target_url->str ? "valid" : "null");

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx && target_url && target_url->str) {
    cef_string_utf8_t url_utf8 = {};
    cef_string_to_utf8(target_url->str, target_url->length, &url_utf8);

    if (url_utf8.str && strlen(url_utf8.str) > 0) {
      char target_url_str[4096];
      strncpy(target_url_str, url_utf8.str, sizeof(target_url_str) - 1);
      target_url_str[sizeof(target_url_str) - 1] = '\0';
      cef_string_utf8_clear(&url_utf8);

      CreateNewTab(win_ctx, target_url_str);
      return 1;
    }
    cef_string_utf8_clear(&url_utf8);
  }

  return 0;
}

void CEF_CALLBACK life_span_handler_on_before_close(
    cef_life_span_handler_t *self, cef_browser_t *browser) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)self;

  LogMsg("life_span_handler_on_before_close: browser=%p\n", browser);

  browser_list_remove(&handler->parent->browser_list, browser);

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx) {
    if (win_ctx->ui_browser &&
        browser->get_identifier(browser) ==
            win_ctx->ui_browser->get_identifier(win_ctx->ui_browser)) {
      win_ctx->ui_browser->base.release(&win_ctx->ui_browser->base);
      win_ctx->ui_browser = NULL;
      LogMsg("on_before_close: cleared ui_browser\n");
    } else {
      for (int i = 0; i < win_ctx->tab_count; i++) {
        if (win_ctx->tabs[i].browser &&
            browser->get_identifier(browser) ==
                win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
          win_ctx->tabs[i].browser->base.release(&win_ctx->tabs[i].browser->base);
          win_ctx->tabs[i].browser = NULL;
          LogMsg("on_before_close: cleared content_browser tab %d\n", i);
          break;
        }
        if (win_ctx->tabs[i].right_browser &&
            browser->get_identifier(browser) ==
                win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser)) {
          win_ctx->tabs[i].right_browser->base.release(&win_ctx->tabs[i].right_browser->base);
          win_ctx->tabs[i].right_browser = NULL;
          win_ctx->tabs[i].right_hwnd = NULL;
          win_ctx->tabs[i].right_tab_handler = NULL;
          LogMsg("on_before_close: cleared right_browser tab %d\n", i);
          break;
        }
      }
    }

    int any_active = 0;
    if (win_ctx->ui_browser != NULL) {
      any_active = 1;
    }
    for (int i = 0; i < win_ctx->tab_count; i++) {
      if (win_ctx->tabs[i].browser != NULL || win_ctx->tabs[i].right_browser != NULL) {
        any_active = 1;
        break;
      }
    }

    if (!any_active) {
      LogMsg("on_before_close: all browsers closed for win_ctx %p, freeing context\n", win_ctx);
      free(win_ctx);
    }
  }

#if defined(_WIN32)
  if (g_window_count == 0) {
    cef_quit_message_loop();
  }
#else
  if (browser_list_count(&handler->parent->browser_list) == 0) {
    cef_quit_message_loop();
  }
#endif

  browser->base.release(&browser->base);
}

simple_life_span_handler_t *life_span_handler_create(simple_handler_t *parent) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)calloc(
      1, sizeof(simple_life_span_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_life_span_handler_t,
                           life_span_handler);

  // Set callbacks.
  handler->handler.on_after_created = life_span_handler_on_after_created;
  handler->handler.do_close = life_span_handler_do_close;
  handler->handler.on_before_close = life_span_handler_on_before_close;
  handler->handler.on_before_popup = life_span_handler_on_before_popup;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
