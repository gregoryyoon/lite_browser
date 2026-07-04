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
  cef_string_utf8_clear(&title_utf8);

  if (g_content_browser &&
      browser->get_identifier(browser) ==
          g_content_browser->get_identifier(
              g_content_browser)) {
    LogMsg("display_handler_on_title_change: checkpoint 1\n");
    browser->base.add_ref(&browser->base);
    cef_browser_view_t* bv = cef_browser_view_get_for_browser(browser);
    LogMsg("display_handler_on_title_change: checkpoint 2 (bv=%p)\n", bv);
    if (bv) {
      cef_window_t* win = bv->base.get_window(&bv->base);
      LogMsg("display_handler_on_title_change: checkpoint 3 (win=%p)\n", win);
      if (win) {
        win->set_title(win, title);
        LogMsg("display_handler_on_title_change: checkpoint 4\n");
        win->base.base.base.release(&win->base.base.base);
      }
      bv->base.base.release(&bv->base.base);
    } else {
      LogMsg("display_handler_on_title_change: calling platform title change\n");
      simple_handler_platform_title_change(handler->parent, browser, title);
      LogMsg("display_handler_on_title_change: platform title change returned\n");
    }
  }

  LogMsg("display_handler_on_title_change: releasing browser\n");
  // Release the browser reference (CEF gave us one for this callback).
  browser->base.release(&browser->base);
  LogMsg("display_handler_on_title_change: done\n");
}

void CEF_CALLBACK
display_handler_on_address_change(cef_display_handler_t* self,
                                  cef_browser_t* browser,
                                  cef_frame_t* frame,
                                  const cef_string_t* url) {
  // (unused handler removed)

  if (g_content_browser &&
      browser->get_identifier(browser) ==
          g_content_browser->get_identifier(
              g_content_browser)) {
    if (g_ui_browser) {
      cef_browser_t* ui_b = g_ui_browser;
      cef_frame_t* ui_frame = ui_b->get_main_frame(ui_b);
      if (ui_frame) {
        cef_string_utf8_t url_utf8 = {};
        if (url && url->str) {
          cef_string_to_utf8(url->str, url->length, &url_utf8);
        }
        LogMsg("display_handler_on_address_change: url=%s\n", url_utf8.str ? url_utf8.str : "(null)");

        LogMsg("display_handler_on_address_change: checkpoint 1\n");
        if (url_utf8.str) {
          LogMsg("display_handler_on_address_change: checkpoint 2\n");
          size_t js_len = strlen(url_utf8.str) + 64;
          char* js_code = (char*)malloc(js_len);
          if (js_code) {
            LogMsg("display_handler_on_address_change: checkpoint 3\n");
            snprintf(js_code, js_len, "updateAddress('%s');", url_utf8.str);

            cef_string_t js_str = {};
            cef_string_from_utf8(js_code, strlen(js_code), &js_str);
            LogMsg("display_handler_on_address_change: checkpoint 4\n");

            cef_string_t script_url = {};

            LogMsg("display_handler_on_address_change: calling execute_java_script\n");
            ui_frame->execute_java_script(ui_frame, &js_str, &script_url, 0);
            LogMsg("display_handler_on_address_change: execute_java_script returned\n");

            cef_string_clear(&js_str);
            LogMsg("display_handler_on_address_change: checkpoint 5\n");

            free(js_code);
          }
        }

        LogMsg("display_handler_on_address_change: checkpoint 6\n");
        cef_string_utf8_clear(&url_utf8);
        LogMsg("display_handler_on_address_change: checkpoint 7\n");
        // ui_frame->base.release(&ui_frame->base); (removed to test ref count crash)
      }
    }
  }

  // Release callback parameters
  browser->base.release(&browser->base);
  frame->base.release(&frame->base);
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

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
