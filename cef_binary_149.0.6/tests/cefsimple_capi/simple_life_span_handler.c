// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "include/capi/cef_app_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

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
// Life span handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_life_span_handler_t,
                             life_span_handler,
                             ref_count)

void CEF_CALLBACK
life_span_handler_on_after_created(cef_life_span_handler_t* self,
                                   cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  LogMsg("life_span_handler_on_after_created: browser=%p\n", browser);

  // Add to the list of existing browsers.
  // browser_list_add adds its own reference, so we can release the parameter.
  browser_list_add(&handler->parent->browser_list, browser);

  extern HWND g_main_hwnd;

  if (!g_ui_browser) {
    g_ui_browser = browser;
    browser->base.add_ref(&browser->base);
    LogMsg("Set g_ui_browser = %p in on_after_created\n", browser);
  } else if (!g_content_browser) {
    g_content_browser = browser;
    browser->base.add_ref(&browser->base);
    LogMsg("Set g_content_browser = %p in on_after_created\n", browser);
  }

#if defined(OS_WIN)
  if (g_main_hwnd) {
    RECT r;
    GetClientRect(g_main_hwnd, &r);
    LogMsg("life_span_handler_on_after_created: posting WM_SIZE to g_main_hwnd\n");
    PostMessage(g_main_hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
  }
#endif

  // Release the browser callback parameter.
  // The list has its own reference now.
  browser->base.release(&browser->base);
}

int CEF_CALLBACK life_span_handler_do_close(cef_life_span_handler_t* self,
                                            cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Closing the main window requires special handling.
  if (browser_list_count(&handler->parent->browser_list) == 1) {
    // Set a flag to indicate that the window close should be allowed.
    handler->parent->is_closing = 1;
  }

  // Release the browser callback parameter before returning.
  browser->base.release(&browser->base);

  // Allow the close. Return false to proceed with closing.
  return 0;
}

void CEF_CALLBACK
life_span_handler_on_before_close(cef_life_span_handler_t* self,
                                  cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Remove from the list of existing browsers.
  // This releases the list's reference to the browser.
  browser_list_remove(&handler->parent->browser_list, browser);

  if (g_ui_browser &&
      browser->get_identifier(browser) ==
          g_ui_browser->get_identifier(g_ui_browser)) {
    g_ui_browser->base.release(&g_ui_browser->base);
    g_ui_browser = NULL;
  }

  if (g_content_browser &&
      browser->get_identifier(browser) ==
          g_content_browser->get_identifier(g_content_browser)) {
    g_content_browser->base.release(&g_content_browser->base);
    g_content_browser = NULL;
  }

  if (browser_list_count(&handler->parent->browser_list) == 0) {
    // All browser windows have closed. Quit the application message loop.
    cef_quit_message_loop();
  }

  // Release the browser callback parameter before returning.
  browser->base.release(&browser->base);
}

simple_life_span_handler_t* life_span_handler_create(simple_handler_t* parent) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)calloc(
      1, sizeof(simple_life_span_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_life_span_handler_t,
                           life_span_handler);

  // Set callbacks.
  handler->handler.on_after_created = life_span_handler_on_after_created;
  handler->handler.do_close = life_span_handler_do_close;
  handler->handler.on_before_close = life_span_handler_on_before_close;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
