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
  if (win_ctx && handler->parent->type != BROWSER_TYPE_POPUP) {
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
          if (k != found_idx) {
            if (win_ctx->tabs[k].hwnd) ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
            if (win_ctx->tabs[k].right_hwnd) ShowWindow(win_ctx->tabs[k].right_hwnd, SW_HIDE);
          }
        }
        ShowWindow(win_ctx->tabs[found_idx].hwnd, SW_SHOW);

        RECT rect;
        GetClientRect(win_ctx->main_hwnd, &rect);
        PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
      }
    }

    update_ui_tabs(win_ctx);

    if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
      cef_browser_t* active_cb = win_ctx->tabs[win_ctx->active_tab_index].browser;
      if (active_cb && browser->get_identifier(browser) == active_cb->get_identifier(active_cb)) {
        update_ui_nav_state(win_ctx);
      }
    }

    if (!isLoading) {
      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
        if (active_tab->browser && browser->get_identifier(browser) == active_tab->browser->get_identifier(active_tab->browser)) {
          if (active_tab->hwnd && IsWindowVisible(active_tab->hwnd)) {
            SetFocus(active_tab->hwnd);
          }
          cef_browser_host_t* host = active_tab->browser->get_host(active_tab->browser);
          if (host) {
            host->set_focus(host, 1);
            host->base.release(&host->base);
          }
        }
      }

      cef_frame_t* main_f = browser->get_main_frame(browser);
      if (main_f) {
        cef_string_userfree_t url_uf = main_f->get_url(main_f);
        if (url_uf) {
          cef_string_utf8_t url_utf8 = {};
          cef_string_to_utf8(url_uf->str, url_uf->length, &url_utf8);
          const char* cur_url = url_utf8.str;

          if (cur_url && strstr(cur_url, "gemini.google.com")) {
            const char* detect_js = 
              "(function() {"
              "  try {"
              "    if (window.__lite_auth_attempted) return;"
              "    let email = '';"
              "    const av = document.querySelector('img[alt*=\"@\"], a[aria-label*=\"@\"], [data-email]');"
              "    if (av) {"
              "      const raw = av.getAttribute('data-email') || av.getAttribute('aria-label') || av.getAttribute('alt') || '';"
              "      const m = raw.match(/[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}/);"
              "      if (m) email = m[0];"
              "    }"
              "    if (!email && window.WIZ_global_data) {"
              "      const str = JSON.stringify(window.WIZ_global_data);"
              "      const m = str.match(/[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}/);"
              "      if (m) email = m[0];"
              "    }"
              "    if (!email) {"
              "      const m = document.documentElement.innerHTML.match(/[a-zA-Z0-9._%+-]+@gmail\\.com/);"
              "      if (m) email = m[0];"
              "    }"
              "    const hasPrompt = !!document.querySelector('textarea, div[contenteditable=\"true\"], .ql-editor, bard-text-input');"
              "    if (email || hasPrompt) {"
              "      window.__lite_auth_attempted = true;"
              "      window.location.href = 'http://ui-action/auth-save-session?provider=gemini&email=' + encodeURIComponent(email || 'Google 계정') + '&tier=' + encodeURIComponent('Gemini Advanced') + '&access_token=google_web_session_' + Date.now();"
              "    }"
              "  } catch(e) {}"
              "})();";
            cef_string_t js_str = {};
            cef_string_from_utf8(detect_js, strlen(detect_js), &js_str);
            main_f->execute_java_script(main_f, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          } else if (cur_url && strstr(cur_url, "chatgpt.com")) {
            const char* detect_js = 
              "(function() {"
              "  try {"
              "    if (window.__lite_auth_attempted) return;"
              "    window.__lite_auth_attempted = true;"
              "    fetch('/api/auth/session').then(r => r.json()).then(data => {"
              "      if (data && data.accessToken) {"
              "        const email = (data.user && data.user.email) ? data.user.email : 'ChatGPT User';"
              "        const tier = (data.user && data.user.planType) ? data.user.planType : 'ChatGPT Plus';"
              "        window.location.href = 'http://ui-action/auth-save-session?provider=openai&email=' + encodeURIComponent(email) + '&tier=' + encodeURIComponent(tier) + '&access_token=' + encodeURIComponent(data.accessToken);"
              "      }"
              "    }).catch(e => {});"
              "  } catch(e) {}"
              "})();";
            cef_string_t js_str = {};
            cef_string_from_utf8(detect_js, strlen(detect_js), &js_str);
            main_f->execute_java_script(main_f, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          } else if (cur_url && strstr(cur_url, "claude.ai")) {
            const char* detect_js = 
              "(function() {"
              "  try {"
              "    if (window.__lite_auth_attempted) return;"
              "    window.__lite_auth_attempted = true;"
              "    fetch('/api/auth/session').then(r => r.json()).then(data => {"
              "      if (data) {"
              "        const email = (data.user && data.user.email) ? data.user.email : 'Claude User';"
              "        window.location.href = 'http://ui-action/auth-save-session?provider=anthropic&email=' + encodeURIComponent(email) + '&tier=Claude%20Pro&access_token=claude_session_' + Date.now();"
              "      }"
              "    }).catch(e => {});"
              "  } catch(e) {}"
              "})();";
            cef_string_t js_str = {};
            cef_string_from_utf8(detect_js, strlen(detect_js), &js_str);
            main_f->execute_java_script(main_f, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }

          cef_string_utf8_clear(&url_utf8);
          cef_string_userfree_free(url_uf);
        }
        main_f->base.release(&main_f->base);
      }
    }
  }
}

static void EscapeJsonStringLocal(const char* src, char* dest, size_t dest_len) {
  if (!src || !dest || dest_len == 0) return;
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j + 2 < dest_len; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == '\\') {
      dest[j++] = '\\'; dest[j++] = '\\';
    } else if (c == '"') {
      dest[j++] = '\\'; dest[j++] = '"';
    } else if (c == '\n') {
      dest[j++] = '\\'; dest[j++] = 'n';
    } else if (c == '\r') {
      dest[j++] = '\\'; dest[j++] = 'r';
    } else if (c == '\t') {
      dest[j++] = '\\'; dest[j++] = 't';
    } else if (c < 32) {
      // skip other control characters
    } else {
      dest[j++] = c;
    }
  }
  dest[j] = '\0';
}

void CEF_CALLBACK load_handler_on_load_end(cef_load_handler_t* self,
                                           cef_browser_t* browser,
                                           cef_frame_t* frame,
                                           int httpStatusCode) {
  simple_load_handler_t* handler = (simple_load_handler_t*)self;
  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (!win_ctx || handler->parent->type == BROWSER_TYPE_POPUP) return;

  // 1. Only process for the main (top-level) frame
  if (!frame || !frame->is_main(frame)) return;

  // 2. Ignore server error codes (>= 400)
  if (httpStatusCode >= 400) return;

  // 3. Ignore if this browser is the UI toolbar browser itself
  if (win_ctx->ui_browser &&
      browser->get_identifier(browser) == win_ctx->ui_browser->get_identifier(win_ctx->ui_browser)) {
    return;
  }

  cef_string_userfree_t url_uf = frame->get_url(frame);
  if (!url_uf) return;

  cef_string_utf8_t url_utf8 = {};
  cef_string_to_utf8(url_uf->str, url_uf->length, &url_utf8);
  const char* cur_url = url_utf8.str;

  // 4. Only record for web URLs (http, https)
  if (cur_url && (strncmp(cur_url, "http://", 7) == 0 || strncmp(cur_url, "https://", 8) == 0)) {
    if (win_ctx->ui_browser) {
      char escaped_url[4096] = {0};
      EscapeJsonStringLocal(cur_url, escaped_url, sizeof(escaped_url));

      char js_code[4500];
      snprintf(js_code, sizeof(js_code),
               "if (window.recordPageVisit) { window.recordPageVisit(\"%s\"); }",
               escaped_url);

      cef_frame_t* ui_frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
      if (ui_frame) {
        cef_string_t js_str = {};
        cef_string_from_utf8(js_code, strlen(js_code), &js_str);
        ui_frame->execute_java_script(ui_frame, &js_str, NULL, 0);
        cef_string_clear(&js_str);
        ui_frame->base.release(&ui_frame->base);
      }
    }
  }

  cef_string_utf8_clear(&url_utf8);
  cef_string_userfree_free(url_uf);
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
  handler->handler.on_load_end = load_handler_on_load_end;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
