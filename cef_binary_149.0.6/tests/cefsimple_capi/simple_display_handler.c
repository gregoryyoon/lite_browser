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

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx) {
    for (int i = 0; i < win_ctx->tab_count; i++) {
      if (win_ctx->tabs[i].browser &&
          browser->get_identifier(browser) ==
              win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
        if (title_utf8.str) {
          strncpy(win_ctx->tabs[i].title, title_utf8.str, sizeof(win_ctx->tabs[i].title) - 1);
          win_ctx->tabs[i].title[sizeof(win_ctx->tabs[i].title) - 1] = '\0';
        }

        if (i == win_ctx->active_tab_index) {
          simple_handler_platform_title_change(handler->parent, browser, title);
        }
        break;
      }
    }
    update_ui_tabs(win_ctx);
  }

  cef_string_utf8_clear(&title_utf8);
  browser->base.release(&browser->base);
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
  if (win_ctx) {
    int is_ui_browser = (win_ctx->ui_browser &&
                         browser->get_identifier(browser) ==
                             win_ctx->ui_browser->get_identifier(win_ctx->ui_browser));

    if (!is_ui_browser) {
      for (int i = 0; i < win_ctx->tab_count; i++) {
        if (win_ctx->tabs[i].browser &&
            browser->get_identifier(browser) ==
                win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
          if (url_utf8.str) {
            strncpy(win_ctx->tabs[i].url, url_utf8.str, sizeof(win_ctx->tabs[i].url) - 1);
            win_ctx->tabs[i].url[sizeof(win_ctx->tabs[i].url) - 1] = '\0';
          }
          break;
        }
      }
      update_ui_tabs(win_ctx);
      update_ui_nav_state(win_ctx);
    }
  }

  cef_string_utf8_clear(&url_utf8);
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
