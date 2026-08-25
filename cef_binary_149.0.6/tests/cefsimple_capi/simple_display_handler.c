// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_window_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"
#include <stdarg.h>

static void LogMsg(const char* format, ...) {
  FILE* f = fopen("C:\\projects\\lite_browser\\debug_c.txt", "a");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fclose(f);
  }
}

//
// Display handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_display_handler_t,
                             display_handler,
                             ref_count)

void CEF_CALLBACK display_handler_on_title_change(cef_display_handler_t* self,
                                                  cef_browser_t* browser,
                                                  const cef_string_t* title) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;

  cef_string_utf8_t title_utf8 = {};
  if (title && title->str) {
    cef_string_to_utf8(title->str, title->length, &title_utf8);
  }
  LogMsg("display_handler_on_title_change: title=%s\n", title_utf8.str ? title_utf8.str : "(null)");

  if (handler->parent->type == BROWSER_TYPE_POPUP) {
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      HWND popup_hwnd = host->get_window_handle(host);
      host->base.release(&host->base);
      if (popup_hwnd) {
        HWND top_hwnd = GetAncestor(popup_hwnd, GA_ROOT);
        if (!top_hwnd) top_hwnd = popup_hwnd;
        if (top_hwnd && title && title->str && title->length > 0) {
          cef_string_wide_t title_wide = {};
          cef_string_to_wide(title->str, title->length, &title_wide);
          if (title_wide.str && wcslen(title_wide.str) > 0) {
            wchar_t window_title[1024];
            _snwprintf(window_title, sizeof(window_title) / sizeof(window_title[0]) - 1, L"%s - Lite Browser", title_wide.str);
            window_title[sizeof(window_title) / sizeof(window_title[0]) - 1] = L'\0';
            SetWindowTextW(top_hwnd, window_title);
          }
          cef_string_wide_clear(&title_wide);
        }
      }
    }
    cef_string_utf8_clear(&title_utf8);
    return;
  }

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx) {
    for (int i = 0; i < win_ctx->tab_count; i++) {
      int is_right = 0;
      if (win_ctx->tabs[i].browser &&
          browser->get_identifier(browser) ==
              win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
        is_right = 0;
      } else if (win_ctx->tabs[i].right_browser &&
                 browser->get_identifier(browser) ==
                     win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser)) {
        is_right = 1;
      } else {
        continue;
      }

      if (title_utf8.str) {
        if (is_right) {
          strncpy(win_ctx->tabs[i].right_title, title_utf8.str, sizeof(win_ctx->tabs[i].right_title) - 1);
          win_ctx->tabs[i].right_title[sizeof(win_ctx->tabs[i].right_title) - 1] = '\0';
        } else {
          strncpy(win_ctx->tabs[i].title, title_utf8.str, sizeof(win_ctx->tabs[i].title) - 1);
          win_ctx->tabs[i].title[sizeof(win_ctx->tabs[i].title) - 1] = '\0';
        }
      }

      if (i == win_ctx->active_tab_index && !is_right) {
        simple_handler_platform_title_change(handler->parent, browser, title);
      }
      break;
    }
    update_ui_tabs(win_ctx);
  }

  cef_string_utf8_clear(&title_utf8);
}

void CEF_CALLBACK
display_handler_on_address_change(cef_display_handler_t* self,
                                  cef_browser_t* browser,
                                  cef_frame_t* frame,
                                  const cef_string_t* url) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;

  cef_string_utf8_t url_utf8 = {};
  if (url && url->str) {
    cef_string_to_utf8(url->str, url->length, &url_utf8);
  }
  LogMsg("display_handler_on_address_change: url=%s\n", url_utf8.str ? url_utf8.str : "(null)");

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx && handler->parent->type != BROWSER_TYPE_POPUP) {
    int is_ui_browser = (win_ctx->ui_browser &&
                         browser->get_identifier(browser) ==
                             win_ctx->ui_browser->get_identifier(win_ctx->ui_browser));

    if (!is_ui_browser) {
      for (int i = 0; i < win_ctx->tab_count; i++) {
        int is_right = 0;
        if (win_ctx->tabs[i].browser &&
            browser->get_identifier(browser) ==
                win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
          is_right = 0;
        } else if (win_ctx->tabs[i].right_browser &&
                   browser->get_identifier(browser) ==
                       win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser)) {
          is_right = 1;
        } else {
          continue;
        }

        if (url_utf8.str) {
          if (is_right) {
            strncpy(win_ctx->tabs[i].right_url, url_utf8.str, sizeof(win_ctx->tabs[i].right_url) - 1);
            win_ctx->tabs[i].right_url[sizeof(win_ctx->tabs[i].right_url) - 1] = '\0';
          } else {
            strncpy(win_ctx->tabs[i].url, url_utf8.str, sizeof(win_ctx->tabs[i].url) - 1);
            win_ctx->tabs[i].url[sizeof(win_ctx->tabs[i].url) - 1] = '\0';
          }
        }
        break;
      }
      update_ui_tabs(win_ctx);
      update_ui_nav_state(win_ctx);
    }
  }

  cef_string_utf8_clear(&url_utf8);
}

int CEF_CALLBACK display_handler_on_contents_bounds_change(
    struct _cef_display_handler_t* self,
    struct _cef_browser_t* browser,
    const cef_rect_t* new_bounds) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;
  if (handler->parent->type == BROWSER_TYPE_POPUP && new_bounds) {
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      HWND browser_hwnd = host->get_window_handle(host);
      host->base.release(&host->base);
      if (browser_hwnd) {
        HWND top_hwnd = GetAncestor(browser_hwnd, GA_ROOT);
        if (top_hwnd) {
          UINT dpi = GetDpiForWindow(top_hwnd);
          if (dpi == 0) dpi = 96;
          float dpi_scale = (float)dpi / 96.0f;

          int client_w = new_bounds->width;
          int client_h = new_bounds->height;
          if (client_w < 480) client_w = 480;
          if (client_h < 750) client_h = 750;

          int phys_w = (int)(client_w * dpi_scale + 0.5f);
          int phys_h = (int)(client_h * dpi_scale + 0.5f);

          RECT wr = {0, 0, phys_w, phys_h};
          typedef BOOL (WINAPI *AdjustWindowRectExForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
          HMODULE hUser32 = GetModuleHandleA("user32.dll");
          AdjustWindowRectExForDpiFn pAdjustWindowRectExForDpi = hUser32 ? (AdjustWindowRectExForDpiFn)GetProcAddress(hUser32, "AdjustWindowRectExForDpi") : NULL;
          if (pAdjustWindowRectExForDpi) {
            pAdjustWindowRectExForDpi(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
          } else {
            AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
          }
          int win_w = wr.right - wr.left;
          int win_h = wr.bottom - wr.top;

          HMONITOR hMonitor = MonitorFromWindow(top_hwnd, MONITOR_DEFAULTTONEAREST);
          MONITORINFO mi = {0};
          mi.cbSize = sizeof(MONITORINFO);
          GetMonitorInfo(hMonitor, &mi);
          RECT work_rc = mi.rcWork;
          int work_w = work_rc.right - work_rc.left;
          int work_h = work_rc.bottom - work_rc.top;

          if (win_h > work_h - 20) win_h = work_h - 20;
          if (win_w > work_w - 20) win_w = work_w - 20;

          int popup_x = work_rc.left + (work_w - win_w) / 2;
          int popup_y = work_rc.top + (work_h - win_h) / 2;

          if (popup_y < work_rc.top) popup_y = work_rc.top;
          if (popup_y + win_h > work_rc.bottom) popup_y = work_rc.bottom - win_h;
          if (popup_x < work_rc.left) popup_x = work_rc.left;
          if (popup_x + win_w > work_rc.right) popup_x = work_rc.right - win_w;

          SetWindowPos(top_hwnd, NULL, popup_x, popup_y, win_w, win_h, SWP_NOZORDER | SWP_NOACTIVATE);
          return 1;
        }
      }
    }
  }
  return 0;
}

void CEF_CALLBACK
display_handler_on_favicon_urlchange(cef_display_handler_t* self,
                                     cef_browser_t* browser,
                                     cef_string_list_t icon_urls) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;
  if (!handler || !handler->parent) return;

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx && handler->parent->type != BROWSER_TYPE_POPUP) {
    char icon_url_str[2048] = {0};
    if (icon_urls && cef_string_list_size(icon_urls) > 0) {
      cef_string_t url_cef = {};
      if (cef_string_list_value(icon_urls, 0, &url_cef)) {
        cef_string_utf8_t url_utf8 = {};
        cef_string_to_utf8(url_cef.str, url_cef.length, &url_utf8);
        if (url_utf8.str && url_utf8.length > 0) {
          strncpy(icon_url_str, url_utf8.str, sizeof(icon_url_str) - 1);
        }
        cef_string_utf8_clear(&url_utf8);
        cef_string_clear(&url_cef);
      }
    }

    if (icon_url_str[0]) {
      for (int i = 0; i < win_ctx->tab_count; i++) {
        if (win_ctx->tabs[i].browser &&
            browser->get_identifier(browser) == win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
          strncpy(win_ctx->tabs[i].favicon_url, icon_url_str, sizeof(win_ctx->tabs[i].favicon_url) - 1);
          win_ctx->tabs[i].favicon_url[sizeof(win_ctx->tabs[i].favicon_url) - 1] = '\0';
          update_ui_tabs(win_ctx);
          break;
        }
        if (win_ctx->tabs[i].right_browser &&
            browser->get_identifier(browser) == win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser)) {
          strncpy(win_ctx->tabs[i].right_favicon_url, icon_url_str, sizeof(win_ctx->tabs[i].right_favicon_url) - 1);
          win_ctx->tabs[i].right_favicon_url[sizeof(win_ctx->tabs[i].right_favicon_url) - 1] = '\0';
          update_ui_tabs(win_ctx);
          break;
        }
      }
    }
  }
}

simple_display_handler_t* display_handler_create(simple_handler_t* parent) {
  simple_display_handler_t* handler =
      (simple_display_handler_t*)calloc(1, sizeof(simple_display_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_display_handler_t,
                           display_handler);

  // Set callbacks.
  handler->handler.on_title_change = display_handler_on_title_change;
  handler->handler.on_address_change = display_handler_on_address_change;
  handler->handler.on_favicon_urlchange = display_handler_on_favicon_urlchange;
  handler->handler.on_contents_bounds_change = display_handler_on_contents_bounds_change;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
