// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_parser_capi.h"
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
// Load handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_load_handler_t, load_handler, ref_count)

void CEF_CALLBACK load_handler_on_load_error(cef_load_handler_t* self,
                                             cef_browser_t* browser,
                                             cef_frame_t* frame,
                                             cef_errorcode_t errorCode,
                                             const cef_string_t* errorText,
                                             const cef_string_t* failedUrl) {
  simple_load_handler_t* handler = (simple_load_handler_t*)self;

  // Only display error page in alloy style and not for aborted downloads.
  // Allow Chrome to show the error page in other cases.
  if (handler->parent->is_alloy_style && errorCode != ERR_ABORTED) {
    // Display a load error message using a data: URI.
    char error_html[1024];
    snprintf(error_html, sizeof(error_html),
             "<html><body bgcolor=\"white\">"
             "<h2>Failed to load URL with error %d.</h2></body></html>",
             errorCode);

    // Convert to cef_string_t.
    cef_string_t error_str = {};
    cef_string_from_ascii(error_html, strlen(error_html), &error_str);

    // Create data URI.
    cef_string_t mime_type = {};
    cef_string_from_ascii("text/html", 9, &mime_type);

    // Base64 encode the error HTML.
    cef_string_userfree_t encoded =
        cef_base64_encode(error_str.str, error_str.length);

    if (encoded) {
      // Create the data URI.
      char data_uri[2048];
      cef_string_utf8_t encoded_utf8 = {};
      cef_string_to_utf8(encoded->str, encoded->length, &encoded_utf8);
      snprintf(data_uri, sizeof(data_uri), "data:text/html;base64,%s",
               encoded_utf8.str);
      cef_string_utf8_clear(&encoded_utf8);

      // Load the data URI.
      cef_string_t data_uri_str = {};
      cef_string_from_ascii(data_uri, strlen(data_uri), &data_uri_str);
      frame->load_url(frame, &data_uri_str);
      cef_string_clear(&data_uri_str);

      cef_string_userfree_free(encoded);
    }

    cef_string_clear(&error_str);
    cef_string_clear(&mime_type);
  }

  // Release all object parameters.
  browser->base.release(&browser->base);
  frame->base.release(&frame->base);
}

void CEF_CALLBACK
load_handler_on_loading_state_change(cef_load_handler_t* self,
                                     cef_browser_t* browser,
                                     int isLoading,
                                     int canGoBack,
                                     int canGoForward) {
  simple_load_handler_t* handler = (simple_load_handler_t*)self;
  LogMsg("load_handler_on_loading_state_change: isLoading=%d, canGoBack=%d, canGoForward=%d\n", isLoading, canGoBack, canGoForward);

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx) {
    int found_idx = -1;
    for (int i = 0; i < win_ctx->tab_count; i++) {
      if (win_ctx->tabs[i].browser &&
          browser->get_identifier(browser) ==
              win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) {
        found_idx = i;
        break;
      }
    }

    if (found_idx != -1) {
      win_ctx->tabs[found_idx].is_loaded = 1;
      if (win_ctx->active_tab_index == found_idx && win_ctx->tabs[found_idx].hwnd) {
        // Hide all other tabs and show this newly active tab immediately on load start/change
        for (int k = 0; k < win_ctx->tab_count; k++) {
          if (k != found_idx && win_ctx->tabs[k].hwnd) {
            ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
          }
        }
        ShowWindow(win_ctx->tabs[found_idx].hwnd, SW_SHOW);

        RECT rect;
        GetClientRect(win_ctx->main_hwnd, &rect);
        PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
      }
    }

    if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
      cef_browser_t* active_cb = win_ctx->tabs[win_ctx->active_tab_index].browser;
      if (active_cb && browser->get_identifier(browser) == active_cb->get_identifier(active_cb)) {
        update_ui_nav_state(win_ctx);
      }
    }
  }

  browser->base.release(&browser->base);
}

simple_load_handler_t* load_handler_create(simple_handler_t* parent) {
  simple_load_handler_t* handler =
      (simple_load_handler_t*)calloc(1, sizeof(simple_load_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_load_handler_t,
                           load_handler);

  // Set callbacks.
  handler->handler.on_load_error = load_handler_on_load_error;
  handler->handler.on_loading_state_change =
      load_handler_on_loading_state_change;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
