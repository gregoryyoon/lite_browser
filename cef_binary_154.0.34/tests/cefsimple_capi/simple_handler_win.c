// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <windows.h>

#include "include/capi/cef_browser_capi.h"

#include "tests/cefsimple_capi/browser_context.h"

void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title) {
  cef_browser_host_t* host = browser->get_host(browser);
  if (!host) {
    return;
  }

  cef_window_handle_t hwnd = host->get_window_handle(host);
  if (hwnd && title && title->str) {
    browser_window_t *win_ctx = handler->window_ctx;
    if (win_ctx) {
      int is_active_content = 0;
      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        cef_browser_t* active_cb = win_ctx->tabs[win_ctx->active_tab_index].browser;
        if (active_cb && browser->get_identifier(browser) == active_cb->get_identifier(active_cb)) {
          is_active_content = 1;
        }
      }

      if (is_active_content) {
        SetWindowTextW(win_ctx->main_hwnd, title->str);
      } else {
        SetWindowTextW(hwnd, title->str);
      }
    } else {
      SetWindowTextW(hwnd, title->str);
    }
  }

  host->base.release(&host->base);
}
