// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <windows.h>

#include "include/capi/cef_browser_capi.h"

extern HWND g_main_hwnd;

void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title) {
  cef_browser_host_t* host = browser->get_host(browser);
  if (!host) {
    return;
  }

  cef_window_handle_t hwnd = host->get_window_handle(host);
  if (hwnd && title && title->str) {
    // If this is the content browser, update the top-level main window title
    if (g_content_browser &&
        browser->get_identifier(browser) == g_content_browser->get_identifier(g_content_browser)) {
      if (g_main_hwnd) {
        SetWindowTextW(g_main_hwnd, title->str);
      }
    } else {
      // Otherwise, update the browser child window title
      SetWindowTextW(hwnd, title->str);
    }
  }

  host->base.release(&host->base);
}
