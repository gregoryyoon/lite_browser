// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>


#if defined(_WIN32) || defined(OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
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
// Dedicated Popup Window Context and WndProc
//
typedef struct {
  HWND hwnd;
  HWND browser_hwnd;
  cef_browser_t* browser;
  browser_window_t* win_ctx;
} popup_window_ctx_t;

#define WM_USER_CLOSE_POPUP (WM_USER + 901)

static LRESULT CALLBACK LiteBrowserPopupWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  popup_window_ctx_t* ctx = (popup_window_ctx_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (message) {
  case WM_ERASEBKGND: {
    HDC hdc = (HDC)wParam;
    RECT r;
    GetClientRect(hwnd, &r);
    int dark = is_theme_dark();
    HBRUSH bg_brush = CreateSolidBrush(dark ? RGB(13, 15, 21) : RGB(228, 228, 231));
    FillRect(hdc, &r, bg_brush);
    DeleteObject(bg_brush);
    return 1;
  }
  case WM_SIZE: {
    if (ctx && ctx->browser_hwnd) {
      int w = LOWORD(lParam);
      int h = HIWORD(lParam);
      SetWindowPos(ctx->browser_hwnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return 0;
  }
  case WM_CLOSE: {
    LogMsg("LiteBrowserPopupWndProc: WM_CLOSE received for hwnd=%p\n", hwnd);
    if (ctx && ctx->browser) {
      cef_browser_host_t* host = ctx->browser->get_host(ctx->browser);
      if (host) {
        LogMsg("LiteBrowserPopupWndProc: calling host->try_close_browser for browser=%p\n", ctx->browser);
        host->try_close_browser(host);
        host->base.release(&host->base);
        return 0; // Let CEF close asynchronously, on_before_close will destroy window
      }
    }
    LogMsg("LiteBrowserPopupWndProc: calling DestroyWindow(hwnd=%p)\n", hwnd);
    DestroyWindow(hwnd);
    return 0;
  }
  case WM_USER_CLOSE_POPUP: {
    LogMsg("LiteBrowserPopupWndProc: WM_USER_CLOSE_POPUP received, calling DestroyWindow(hwnd=%p)\n", hwnd);
    DestroyWindow(hwnd);
    return 0;
  }
  case WM_DESTROY: {
    LogMsg("LiteBrowserPopupWndProc: WM_DESTROY received for hwnd=%p\n", hwnd);
    break;
  }
  case WM_NCDESTROY: {
    LogMsg("LiteBrowserPopupWndProc: WM_NCDESTROY received for hwnd=%p\n", hwnd);
    if (ctx) {
      ctx->browser = NULL;
      free(ctx);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
    }
    return 0;
  }
  }
  return DefWindowProcW(hwnd, message, wParam, lParam);
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
    } else if (handler->parent->type == BROWSER_TYPE_SIDEPANEL) {
      win_ctx->sidepanel_browser = browser;
      browser->base.add_ref(&browser->base);
      win_ctx->sidepanel_hwnd = hwnd;
      if (win_ctx->show_sidepanel) {
        ShowWindow(hwnd, SW_SHOW);
      } else {
        ShowWindow(hwnd, SW_HIDE);
      }
      LogMsg("Set win_ctx->sidepanel_browser = %p, hwnd = %p\n", browser, hwnd);
    } else if (handler->parent->type == BROWSER_TYPE_POPUP) {
      LogMsg("Created BROWSER_TYPE_POPUP browser=%p, hwnd=%p\n", browser, hwnd);
      if (hwnd) {
        HWND parent_wnd = GetParent(hwnd);
        if (parent_wnd) {
          popup_window_ctx_t* pctx = (popup_window_ctx_t*)GetWindowLongPtr(parent_wnd, GWLP_USERDATA);
          if (pctx) {
            pctx->browser_hwnd = hwnd;
            pctx->browser = browser;

            RECT rc;
            GetClientRect(parent_wnd, &rc);
            SetWindowPos(hwnd, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
          }
        }
      }
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
    SubclassAllChildWindows(win_ctx->main_hwnd);
  }
}

int CEF_CALLBACK life_span_handler_do_close(cef_life_span_handler_t *self,
                                             cef_browser_t *browser) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)self;

  if (handler->parent && handler->parent->type == BROWSER_TYPE_POPUP) {
    // Return 1 (true) to indicate that popup close is handled by client,
    // preventing CEF from sending WM_CLOSE to any parent/root windows.
    return 1;
  }

  if (browser_list_count(&handler->parent->browser_list) == 1) {
    handler->parent->is_closing = 1;
  }

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
  browser_window_t *win_ctx = handler->parent->window_ctx;

  int is_popup = 0;
  if (target_disposition == CEF_WOD_NEW_POPUP ||
      target_disposition == CEF_WOD_NEW_PICTURE_IN_PICTURE) {
    is_popup = 1;
  } else if (popupFeatures && (popupFeatures->isPopup || popupFeatures->widthSet || popupFeatures->heightSet)) {
    is_popup = 1;
  }

  LogMsg("life_span_handler_on_before_popup: target_url=%s, target_disposition=%d, is_popup=%d\n",
         target_url && target_url->str ? "valid" : "null", target_disposition, is_popup);

  if (is_popup) {
    // Explicit popup window requested (e.g. payment gateway, OAuth login dialog).
    // Create dedicated native popup window and host child browser inside it.
    UINT dpi = 96;
    if (win_ctx && win_ctx->main_hwnd) {
      dpi = GetDpiForWindow(win_ctx->main_hwnd);
    }
    if (dpi == 0) dpi = 96;
    float dpi_scale = (float)dpi / 96.0f;

    int client_w = 480;
    int client_h = 750;
    if (popupFeatures) {
      if (popupFeatures->widthSet && popupFeatures->width > 50) {
        client_w = popupFeatures->width;
      }
      if (popupFeatures->heightSet && popupFeatures->height > 50) {
        client_h = popupFeatures->height;
      }
    }
    if (client_w < 480) client_w = 480;
    if (client_h < 750) client_h = 750;

    int phys_client_w = (int)(client_w * dpi_scale + 0.5f);
    int phys_client_h = (int)(client_h * dpi_scale + 0.5f);

    RECT wr = {0, 0, phys_client_w, phys_client_h};
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

    HMONITOR hMonitor = MonitorFromWindow(win_ctx && win_ctx->main_hwnd ? win_ctx->main_hwnd : NULL, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);
    RECT work_rc = mi.rcWork;
    int work_w = work_rc.right - work_rc.left;
    int work_h = work_rc.bottom - work_rc.top;

    if (win_h > work_h - 20) {
      win_h = work_h - 20;
    }
    if (win_w > work_w - 20) {
      win_w = work_w - 20;
    }

    int popup_x = work_rc.left + (work_w - win_w) / 2;
    int popup_y = work_rc.top + (work_h - win_h) / 2;

    if (popup_y < work_rc.top) {
      popup_y = work_rc.top;
    }
    if (popup_y + win_h > work_rc.bottom) {
      popup_y = work_rc.bottom - win_h;
      if (popup_y < work_rc.top) popup_y = work_rc.top;
    }
    if (popup_x < work_rc.left) {
      popup_x = work_rc.left;
    }
    if (popup_x + win_w > work_rc.right) {
      popup_x = work_rc.right - win_w;
      if (popup_x < work_rc.left) popup_x = work_rc.left;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    static int s_popup_class_registered = 0;
    int dark_init = is_theme_dark();
    if (!s_popup_class_registered) {
      WNDCLASSEXW wcex = {0};
      wcex.cbSize = sizeof(WNDCLASSEXW);
      wcex.style = CS_HREDRAW | CS_VREDRAW;
      wcex.lpfnWndProc = LiteBrowserPopupWndProc;
      wcex.hInstance = hInstance;
      wcex.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(120), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
      wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
      wcex.hbrBackground = CreateSolidBrush(dark_init ? RGB(13, 15, 21) : RGB(228, 228, 231));
      wcex.lpszClassName = L"LiteBrowserPopupWnd";
      wcex.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(120), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
      RegisterClassExW(&wcex);
      s_popup_class_registered = 1;
    }

    HWND popup_hwnd = CreateWindowExW(
        0, L"LiteBrowserPopupWnd", L"Lite Browser",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        popup_x, popup_y, win_w, win_h,
        NULL, NULL, hInstance, NULL);

    if (popup_hwnd) {
      HBRUSH hbr = CreateSolidBrush(dark_init ? RGB(13, 15, 21) : RGB(228, 228, 231));
      HBRUSH old_hbr = (HBRUSH)SetClassLongPtrW(popup_hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hbr);
      if (old_hbr) DeleteObject(old_hbr);

      if (dark_init) {
        BOOL dark_mode_val = TRUE;
        if (FAILED(DwmSetWindowAttribute(popup_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode_val, sizeof(dark_mode_val)))) {
          DwmSetWindowAttribute(popup_hwnd, 19, &dark_mode_val, sizeof(dark_mode_val));
        }
      }
    }

    if (settings) {
      settings->background_color = dark_init ? 0xFF0D0F15 : 0xFFFFFFFF;
    }

    popup_window_ctx_t* pctx = (popup_window_ctx_t*)calloc(1, sizeof(popup_window_ctx_t));
    if (pctx) {
      pctx->hwnd = popup_hwnd;
      pctx->win_ctx = win_ctx;
      SetWindowLongPtr(popup_hwnd, GWLP_USERDATA, (LONG_PTR)pctx);
    }

    simple_handler_t* popup_handler = simple_handler_create(0);
    popup_handler->type = BROWSER_TYPE_POPUP;
    popup_handler->window_ctx = win_ctx;
    *client = &popup_handler->client;

    if (windowInfo) {
      RECT client_rc;
      GetClientRect(popup_hwnd, &client_rc);
      windowInfo->style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      windowInfo->parent_window = popup_hwnd;
      windowInfo->bounds.x = 0;
      windowInfo->bounds.y = 0;
      windowInfo->bounds.width = client_rc.right;
      windowInfo->bounds.height = client_rc.bottom;
      windowInfo->runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
    }

    return 0; // Return 0 (false) to let CEF create the child browser in popup_hwnd
  }

  // Regular link navigation / new tab request -> open as tab in LiteBrowser
  if (win_ctx && target_url && target_url->str) {
    cef_string_utf8_t url_utf8 = {};
    cef_string_to_utf8(target_url->str, target_url->length, &url_utf8);

    if (url_utf8.str && strlen(url_utf8.str) > 0) {
      char target_url_str[4096];
      strncpy(target_url_str, url_utf8.str, sizeof(target_url_str) - 1);
      target_url_str[sizeof(target_url_str) - 1] = '\0';
      cef_string_utf8_clear(&url_utf8);

      int found_tab_idx = -1;
      for (int i = 0; i < win_ctx->tab_count; i++) {
        if ((win_ctx->tabs[i].browser && browser->get_identifier(browser) == win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) ||
            (win_ctx->tabs[i].right_browser && browser->get_identifier(browser) == win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser))) {
          found_tab_idx = i;
          break;
        }
      }

      if (found_tab_idx != -1 && win_ctx->tabs[found_tab_idx].is_split) {
        cef_frame_t* target_frame = browser->get_main_frame(browser);
        if (target_frame) {
          cef_string_t url_str = {};
          cef_string_from_utf8(target_url_str, strlen(target_url_str), &url_str);
          target_frame->load_url(target_frame, &url_str);
          cef_string_clear(&url_str);
          target_frame->base.release(&target_frame->base);
        }
        return 1;
      }

      CreateNewTab(win_ctx, target_url_str);
      return 1;
    }
    cef_string_utf8_clear(&url_utf8);
  }

  return 1;
}

void CEF_CALLBACK life_span_handler_on_before_close(
    cef_life_span_handler_t *self, cef_browser_t *browser) {
  simple_life_span_handler_t *handler = (simple_life_span_handler_t *)self;

  LogMsg("life_span_handler_on_before_close: browser=%p, type=%d\n", browser, handler->parent ? handler->parent->type : -1);

  if (handler->parent && handler->parent->type == BROWSER_TYPE_POPUP) {
    LogMsg("on_before_close: popup browser %p closed\n", browser);
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      HWND browser_hwnd = host->get_window_handle(host);
      host->base.release(&host->base);
      if (browser_hwnd) {
        HWND parent_wnd = GetParent(browser_hwnd);
        if (parent_wnd && IsWindow(parent_wnd)) {
          LogMsg("on_before_close: posting WM_USER_CLOSE_POPUP to parent_wnd=%p\n", parent_wnd);
          PostMessage(parent_wnd, WM_USER_CLOSE_POPUP, 0, 0);
        }
      }
    }
    return;
  }

  browser_list_remove(&handler->parent->browser_list, browser);

  browser_window_t *win_ctx = handler->parent->window_ctx;
  if (win_ctx) {
    if (win_ctx->ui_browser &&
        browser->get_identifier(browser) ==
            win_ctx->ui_browser->get_identifier(win_ctx->ui_browser)) {
      win_ctx->ui_browser->base.release(&win_ctx->ui_browser->base);
      win_ctx->ui_browser = NULL;
      LogMsg("on_before_close: cleared ui_browser\n");
    } else if (win_ctx->sidepanel_browser &&
               browser->get_identifier(browser) ==
                   win_ctx->sidepanel_browser->get_identifier(win_ctx->sidepanel_browser)) {
      win_ctx->sidepanel_browser->base.release(&win_ctx->sidepanel_browser->base);
      win_ctx->sidepanel_browser = NULL;
      win_ctx->sidepanel_hwnd = NULL;
      win_ctx->sidepanel_handler = NULL;
      LogMsg("on_before_close: cleared sidepanel_browser\n");
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
    if (win_ctx->ui_browser != NULL || win_ctx->sidepanel_browser != NULL) {
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
    LogMsg("on_before_close: calling cef_quit_message_loop() because g_window_count == 0\n");
    cef_quit_message_loop();
  }
#else
  if (browser_list_count(&handler->parent->browser_list) == 0) {
    LogMsg("on_before_close: calling cef_quit_message_loop() because browser_list_count == 0\n");
    cef_quit_message_loop();
  }
#endif
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
