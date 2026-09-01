#include "tests/cefsimple_capi/simple_app.h"
#include "include/capi/cef_preference_capi.h"
#include "include/capi/cef_values_capi.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shellapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#endif
#include <stdarg.h>
#include <stdio.h>

#if defined(OS_WIN)
static void GetConfigFilePath(char* out_path, size_t max_len) {
  char user_profile[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, user_profile, CSIDL_PROFILE, FALSE)) {
    snprintf(out_path, max_len, "%s\\.lite-browser", user_profile);
    CreateDirectoryA(out_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\window_config.txt", user_profile);
  } else {
    snprintf(out_path, max_len, "C:\\projects\\lite_browser\\window_config.txt");
  }
}

int GetUIHeightForWindow(HWND hwnd) {
  UINT dpi = GetDpiForWindow(hwnd);
  double scale = (double)dpi / 96.0;
  return (int)(72.0 * scale);
}
#endif

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_window_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"
#include "tests/cefsimple_capi/simple_views.h"

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

// Resolves local path to ui/index.html
static void ResolveUIPath(cef_string_t *out_url) {
#if defined(OS_WIN)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  char path[MAX_PATH];
  strcpy(path, exe_path);

  int found = 0;
  for (int i = 0; i < 8; i++) {
    char *last_backslash = strrchr(path, '\\');
    if (!last_backslash)
      break;
    *last_backslash = '\0';

    char test_path[MAX_PATH];
    snprintf(test_path, sizeof(test_path), "%s\\ui\\index.html", path);
    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES &&
        !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
      char file_url[MAX_PATH + 16];
      snprintf(file_url, sizeof(file_url), "file:///%s/ui/index.html", path);
      for (size_t j = 8; file_url[j]; j++) {
        if (file_url[j] == '\\') {
          file_url[j] = '/';
        }
      }
      cef_string_from_ascii(file_url, strlen(file_url), out_url);
      found = 1;
      break;
    }
  }

  if (!found) {
    cef_string_from_ascii("file:///C:/projects/lite_browser/ui/index.html", 46,
                          out_url);
  }
#else
  // Fallback for non-Windows (e.g. Linux/Mac)
  cef_string_from_ascii("file:///projects/lite_browser/ui/index.html", 43,
                        out_url);
#endif
}

// Resolves local path to ui/sidepanel.html
static void ResolveSidepanelPath(cef_string_t *out_url) {
#if defined(OS_WIN)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  char path[MAX_PATH];
  strcpy(path, exe_path);

  int found = 0;
  for (int i = 0; i < 8; i++) {
    char *last_backslash = strrchr(path, '\\');
    if (!last_backslash)
      break;
    *last_backslash = '\0';

    char test_path[MAX_PATH];
    snprintf(test_path, sizeof(test_path), "%s\\ui\\sidepanel.html", path);
    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES &&
        !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
      char file_url[MAX_PATH + 16];
      snprintf(file_url, sizeof(file_url), "file:///%s/ui/sidepanel.html", path);
      for (size_t j = 8; file_url[j]; j++) {
        if (file_url[j] == '\\') {
          file_url[j] = '/';
        }
      }
      cef_string_from_ascii(file_url, strlen(file_url), out_url);
      found = 1;
      break;
    }
  }

  if (!found) {
    cef_string_from_ascii("file:///C:/projects/lite_browser/ui/sidepanel.html", 50,
                          out_url);
  }
#else
  cef_string_from_ascii("file:///projects/lite_browser/ui/sidepanel.html", 47,
                        out_url);
#endif
}



// Implement reference counting functions for simple_app_t.
IMPLEMENT_REFCOUNTING_MANUAL(simple_app_t, simple_app, ref_count)

// Release function for simple_app_t with custom cleanup logic.
int CEF_CALLBACK simple_app_release(cef_base_ref_counted_t *self) {
  simple_app_t *app = (simple_app_t *)self;
  int count = atomic_fetch_sub(&app->ref_count, 1) - 1;
  if (count == 0) {
    // Release the browser process handler if we own one.
    if (app->browser_process_handler) {
      app->browser_process_handler->handler.base.release(
          &app->browser_process_handler->handler.base);
    }
    free(app);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK simple_app_on_before_command_line_processing(
    cef_app_t* self,
    const cef_string_t* process_type,
    cef_command_line_t* command_line) {
  cef_string_t switch2 = {};
  cef_string_from_ascii("allow-file-access-from-files", 28, &switch2);
  command_line->append_switch(command_line, &switch2);
  cef_string_clear(&switch2);

#if defined(OS_WIN)
  cef_string_t lang_switch = {};
  cef_string_from_ascii("lang", 4, &lang_switch);
  if (!command_line->has_switch(command_line, &lang_switch)) {
    WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {0};
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
      cef_string_t lang_val = {};
      cef_string_wide_to_utf16(locale_name, wcslen(locale_name), &lang_val);
      command_line->append_switch_with_value(command_line, &lang_switch, &lang_val);
      cef_string_clear(&lang_val);
    }
  }
  cef_string_clear(&lang_switch);
#endif
}

// Returns the browser process handler.
// Adds a reference before returning (CEF will release it when done).
cef_browser_process_handler_t *CEF_CALLBACK
simple_app_get_browser_process_handler(cef_app_t *self) {
  simple_app_t *app = (simple_app_t *)self;
  if (app->browser_process_handler) {
    // Add reference for CEF (it will release when done).
    app->browser_process_handler->handler.base.add_ref(
        &app->browser_process_handler->handler.base);
    return &app->browser_process_handler->handler;
  }
  return NULL;
}

// Forward declarations for browser process handler functions.
void CEF_CALLBACK browser_process_handler_on_context_initialized(
    cef_browser_process_handler_t *self);
cef_client_t *CEF_CALLBACK
browser_process_handler_get_default_client(cef_browser_process_handler_t *self);
int CEF_CALLBACK browser_process_handler_on_already_running_app_relaunch(
    cef_browser_process_handler_t *self,
    struct _cef_command_line_t *command_line,
    const cef_string_t *current_directory);

// Implement reference counting functions for browser process handler.
IMPLEMENT_REFCOUNTING_SIMPLE(simple_browser_process_handler_t,
                             browser_process_handler, ref_count)

#if defined(OS_WIN)
#include "tests/cefsimple_capi/browser_context.h"

#define MAX_WINDOWS 10
static browser_window_t* g_windows[MAX_WINDOWS] = {NULL};
int g_window_count = 0;

LRESULT CALLBACK LiteBrowserMainWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam)
{
  browser_window_t* win_ctx = (browser_window_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (message)
  {
  case WM_GETMINMAXINFO:
  {
    MINMAXINFO* mmi = (MINMAXINFO*)lParam;
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(hMonitor, &mi)) {
      APPBARDATA abd = {sizeof(APPBARDATA)};
      UINT state = SHAppBarMessage(ABM_GETSTATE, &abd);
      int is_autohide = (state & ABS_AUTOHIDE);

      int has_taskbar = 0;
      if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        if (abd.rc.left >= mi.rcMonitor.left && abd.rc.right <= mi.rcMonitor.right &&
            abd.rc.top >= mi.rcMonitor.top && abd.rc.bottom <= mi.rcMonitor.bottom) {
          has_taskbar = 1;
        }
      }

      if (has_taskbar && is_autohide) {
        mmi->ptMaxSize.x = mi.rcMonitor.right - mi.rcMonitor.left;
        mmi->ptMaxSize.y = mi.rcMonitor.bottom - mi.rcMonitor.top;
        mmi->ptMaxPosition.x = 0;
        mmi->ptMaxPosition.y = 0;

        if (abd.uEdge == ABE_BOTTOM) {
          mmi->ptMaxSize.y -= 1;
        } else if (abd.uEdge == ABE_TOP) {
          mmi->ptMaxSize.y -= 1;
          mmi->ptMaxPosition.y += 1;
        } else if (abd.uEdge == ABE_LEFT) {
          mmi->ptMaxSize.x -= 1;
          mmi->ptMaxPosition.x += 1;
        } else if (abd.uEdge == ABE_RIGHT) {
          mmi->ptMaxSize.x -= 1;
        }
      } else {
        mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
        mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
        mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
      }
    }
    return 0;
  }
  case WM_MOUSEACTIVATE:
  case WM_PARENTNOTIFY:
  {
    if (win_ctx) {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);

      RECT r;
      GetClientRect(hwnd, &r);
      int ui_height = GetUIHeightForWindow(hwnd);
      int content_y = ui_height + 1;
      int total_w = r.right - 2;

      int sp_w = (win_ctx->show_sidepanel && win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : 0;
      int sp_splitter_w = (win_ctx->show_sidepanel) ? 5 : 0;
      int main_area_w = total_w - sp_w - sp_splitter_w;

      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
        if (active_tab->is_split && active_tab->right_browser) {
          int split_bar_w = 6;
          float ratio = active_tab->split_ratio;
          if (ratio < 0.2f || ratio > 0.8f) ratio = 0.5f;

          int left_w = (int)((main_area_w - split_bar_w) * ratio);
          int bar_x = 1 + left_w;

          if (pt.y >= content_y && pt.x < 1 + main_area_w) {
            int target_split = (pt.x < bar_x + split_bar_w / 2) ? 0 : 1;
            if (active_tab->active_split != target_split) {
              active_tab->active_split = target_split;
              update_ui_nav_state(win_ctx);
              InvalidateRect(hwnd, NULL, FALSE);
            }
          }
        }
      }
    }
    if (message == WM_MOUSEACTIVATE) {
      return MA_ACTIVATE;
    }
    break;
  }
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (win_ctx) {
      RECT r;
      GetClientRect(hwnd, &r);
      int ui_height = GetUIHeightForWindow(hwnd);
      int content_y = ui_height + 1;
      int content_h = r.bottom - content_y - 1;
      int total_w = r.right - 2;

      int sp_w = (win_ctx->show_sidepanel && win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : 0;
      int sp_splitter_w = (win_ctx->show_sidepanel) ? 5 : 0;
      int main_area_w = total_w - sp_w - sp_splitter_w;

      // 1. Paint AI Sidepanel splitter bar
      if (win_ctx->show_sidepanel) {
        int sp_bar_x = 1 + main_area_w;
        RECT sp_bar_rect = {sp_bar_x, content_y, sp_bar_x + sp_splitter_w, content_y + content_h};
        HBRUSH sp_bar_brush = CreateSolidBrush(RGB(228, 230, 235));
        FillRect(hdc, &sp_bar_rect, sp_bar_brush);
        DeleteObject(sp_bar_brush);
      }

      // 2. Paint Dual Split tab borders
      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
        if (active_tab->is_split && active_tab->right_browser) {
          int split_bar_w = 6;
          float ratio = active_tab->split_ratio;
          if (ratio < 0.2f || ratio > 0.8f) ratio = 0.5f;

          int left_w = (int)((main_area_w - split_bar_w) * ratio);
          int right_w = main_area_w - split_bar_w - left_w;
          int left_x = 1;
          int right_x = 1 + left_w + split_bar_w;
          int bar_x = 1 + left_w;

          HBRUSH active_brush = CreateSolidBrush(RGB(0, 102, 204));
          HBRUSH inactive_brush = CreateSolidBrush(RGB(220, 220, 225));
          HBRUSH bar_brush = CreateSolidBrush(RGB(230, 230, 235));

          RECT left_rect = {left_x, content_y, left_x + left_w, content_y + content_h};
          RECT bar_rect = {bar_x, content_y, bar_x + split_bar_w, content_y + content_h};
          RECT right_rect = {right_x, content_y, right_x + right_w, content_y + content_h};

          FrameRect(hdc, &left_rect, (active_tab->active_split == 0) ? active_brush : inactive_brush);
          RECT left_rect_inner = {left_x + 1, content_y + 1, left_x + left_w - 1, content_y + content_h - 1};
          FrameRect(hdc, &left_rect_inner, (active_tab->active_split == 0) ? active_brush : inactive_brush);

          FillRect(hdc, &bar_rect, bar_brush);

          FrameRect(hdc, &right_rect, (active_tab->active_split == 1) ? active_brush : inactive_brush);
          RECT right_rect_inner = {right_x + 1, content_y + 1, right_x + right_w - 1, content_y + content_h - 1};
          FrameRect(hdc, &right_rect_inner, (active_tab->active_split == 1) ? active_brush : inactive_brush);

          DeleteObject(active_brush);
          DeleteObject(inactive_brush);
          DeleteObject(bar_brush);
        }
      }
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_SETCURSOR:
  {
    if (win_ctx) {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);

      RECT r;
      GetClientRect(hwnd, &r);
      int ui_height = GetUIHeightForWindow(hwnd);
      int content_y = ui_height + 1;
      int content_h = r.bottom - content_y - 1;
      int total_w = r.right - 2;

      int sp_w = (win_ctx->show_sidepanel && win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : 0;
      int sp_splitter_w = (win_ctx->show_sidepanel) ? 5 : 0;
      int main_area_w = total_w - sp_w - sp_splitter_w;

      // Check AI Sidepanel splitter
      if (win_ctx->show_sidepanel) {
        int sp_bar_x = 1 + main_area_w;
        if (pt.x >= sp_bar_x - 1 && pt.x <= sp_bar_x + sp_splitter_w + 1 &&
            pt.y >= content_y && pt.y <= content_y + content_h) {
          SetCursor(LoadCursor(NULL, IDC_SIZEWE));
          return TRUE;
        }
      }

      // Check Dual Split splitter
      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
        if (active_tab->is_split && active_tab->right_browser) {
          int split_bar_w = 6;
          float ratio = active_tab->split_ratio;
          if (ratio < 0.2f || ratio > 0.8f) ratio = 0.5f;

          int left_w = (int)((main_area_w - split_bar_w) * ratio);
          int bar_x = 1 + left_w;

          if (pt.x >= bar_x - 1 && pt.x <= bar_x + split_bar_w + 1 &&
              pt.y >= content_y && pt.y <= content_y + content_h) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
          }
        }
      }
    }
    break;
  }
  case WM_LBUTTONDOWN:
  {
    if (win_ctx) {
      int pt_x = (short)LOWORD(lParam);
      int pt_y = (short)HIWORD(lParam);

      RECT r;
      GetClientRect(hwnd, &r);
      int ui_height = GetUIHeightForWindow(hwnd);
      int content_y = ui_height + 1;
      int content_h = r.bottom - content_y - 1;
      int total_w = r.right - 2;

      int sp_w = (win_ctx->show_sidepanel && win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : 0;
      int sp_splitter_w = (win_ctx->show_sidepanel) ? 5 : 0;
      int main_area_w = total_w - sp_w - sp_splitter_w;

      // Check AI Sidepanel splitter drag
      if (win_ctx->show_sidepanel) {
        int sp_bar_x = 1 + main_area_w;
        if (pt_x >= sp_bar_x - 1 && pt_x <= sp_bar_x + sp_splitter_w + 1 &&
            pt_y >= content_y && pt_y <= content_y + content_h) {
          SetCapture(hwnd);
          win_ctx->is_resizing_sidepanel = 1;
          win_ctx->sidepanel_drag_start_x = pt_x;
          win_ctx->sidepanel_drag_start_w = win_ctx->sidepanel_width;
          return 0;
        }
      }

      // Check Dual Split splitter drag
      if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
        tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
        if (active_tab->is_split && active_tab->right_browser) {
          int split_bar_w = 6;
          float ratio = active_tab->split_ratio;
          if (ratio < 0.2f || ratio > 0.8f) ratio = 0.5f;

          int left_w = (int)((main_area_w - split_bar_w) * ratio);
          int bar_x = 1 + left_w;

          if (pt_x >= bar_x - 1 && pt_x <= bar_x + split_bar_w + 1 &&
              pt_y >= content_y && pt_y <= content_y + content_h) {
            SetCapture(hwnd);
            win_ctx->is_resizing_splitter = 1;
            win_ctx->drag_start_x = pt_x;
            win_ctx->drag_start_ratio = ratio;
            return 0;
          }
        }
      }
    }
    break;
  }
  case WM_MOUSEMOVE:
  {
    if (win_ctx && win_ctx->is_resizing_sidepanel) {
      int cur_x = (short)LOWORD(lParam);
      RECT r;
      GetClientRect(hwnd, &r);
      int total_w = r.right - 2;

      int delta_x = cur_x - win_ctx->sidepanel_drag_start_x;
      int new_w = win_ctx->sidepanel_drag_start_w - delta_x;
      if (new_w < 260) new_w = 260;
      if (new_w > total_w - 300) new_w = total_w - 300;
      if (new_w < 260) new_w = 260;
      win_ctx->sidepanel_width = new_w;
      SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
      return 0;
    }
    if (win_ctx && win_ctx->is_resizing_splitter &&
        win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
      tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
      int cur_x = (short)LOWORD(lParam);

      RECT r;
      GetClientRect(hwnd, &r);
      int total_w = r.right - 2;
      int sp_w = (win_ctx->show_sidepanel && win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : 0;
      int sp_splitter_w = (win_ctx->show_sidepanel) ? 5 : 0;
      int main_area_w = total_w - sp_w - sp_splitter_w;
      int split_bar_w = 6;
      int avail_w = main_area_w - split_bar_w;

      if (avail_w > 0) {
        int delta_x = cur_x - win_ctx->drag_start_x;
        float new_ratio = win_ctx->drag_start_ratio + (float)delta_x / (float)avail_w;
        if (new_ratio < 0.2f) new_ratio = 0.2f;
        if (new_ratio > 0.8f) new_ratio = 0.8f;
        active_tab->split_ratio = new_ratio;
        SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
      }
      return 0;
    }
    break;
  }
  case WM_LBUTTONUP:
  {
    if (win_ctx && (win_ctx->is_resizing_splitter || win_ctx->is_resizing_sidepanel)) {
      ReleaseCapture();
      win_ctx->is_resizing_splitter = 0;
      win_ctx->is_resizing_sidepanel = 0;
      return 0;
    }
    break;
  }
  case WM_SETFOCUS:
  case WM_ACTIVATE:
  {
    if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
      tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
      HWND target_hwnd = (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_hwnd)
                             ? active_tab->right_hwnd : active_tab->hwnd;
      cef_browser_t* target_b = (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_browser)
                             ? active_tab->right_browser : active_tab->browser;
      if (target_hwnd && IsWindowVisible(target_hwnd)) {
        SetFocus(target_hwnd);
      }
      if (target_b) {
        cef_browser_host_t* host = target_b->get_host(target_b);
        if (host) {
          host->set_focus(host, 1);
          host->base.release(&host->base);
        }
      }
    }
    return 0;
  }
  case WM_SIZE:
  {
    if (!win_ctx) return 0;
    if (wParam == SIZE_MINIMIZED) return 0;

    int width = LOWORD(lParam);
    int height = HIWORD(lParam);
    if (width <= 0 || height <= 0) return 0;

    int ui_height = GetUIHeightForWindow(hwnd);
    int content_y = ui_height + 1;
    int content_h = height - content_y - 1;
    int total_w = width - 2;

    int sp_w = 0;
    int sp_splitter_w = 0;
    int sp_bar_x = 0;
    int sp_x = 0;
    int main_area_w = total_w;

    if (win_ctx->show_sidepanel) {
      sp_splitter_w = 5;
      int default_sp_w = (int)(380 * (ui_height / 72.0f));
      int min_sp_w = (int)(320 * (ui_height / 72.0f));
      if (min_sp_w < 280) min_sp_w = 280;

      sp_w = (win_ctx->sidepanel_width > 0) ? win_ctx->sidepanel_width : default_sp_w;
      if (sp_w < min_sp_w) sp_w = min_sp_w;

      if (total_w > min_sp_w + 300) {
        if (sp_w > total_w - 300) sp_w = total_w - 300;
      }
      if (sp_w < 200) sp_w = 200;

      main_area_w = total_w - sp_w - sp_splitter_w;
      if (main_area_w < 100) main_area_w = 100;
      sp_bar_x = 1 + main_area_w;
      sp_x = sp_bar_x + sp_splitter_w;
    }

    if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count)
    {
      tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
      if (active_tab->is_split && active_tab->right_browser)
      {
        int split_bar_w = 6;
        float ratio = active_tab->split_ratio;
        if (ratio < 0.2f || ratio > 0.8f) ratio = 0.5f;

        int left_w = (int)((main_area_w - split_bar_w) * ratio);
        int right_w = main_area_w - split_bar_w - left_w;
        int left_x = 1;
        int right_x = 1 + left_w + split_bar_w;

        if (active_tab->browser) {
          cef_browser_host_t *host = active_tab->browser->get_host(active_tab->browser);
          if (host) {
            HWND left_hwnd = host->get_window_handle(host);
            if (left_hwnd) {
              MoveWindow(left_hwnd, left_x + 2, content_y + 2, left_w - 4, content_h - 4, TRUE);
              ShowWindow(left_hwnd, SW_SHOW);
            }
            host->base.release(&host->base);
          }
        }

        cef_browser_host_t *r_host = active_tab->right_browser->get_host(active_tab->right_browser);
        if (r_host) {
          HWND right_hwnd = r_host->get_window_handle(r_host);
          if (right_hwnd) {
            MoveWindow(right_hwnd, right_x + 2, content_y + 2, right_w - 4, content_h - 4, TRUE);
            ShowWindow(right_hwnd, SW_SHOW);
          }
          r_host->base.release(&r_host->base);
        }

        InvalidateRect(hwnd, NULL, FALSE);
      }
      else
      {
        cef_browser_t* content_browser = active_tab->browser;
        if (content_browser)
        {
          cef_browser_host_t *host = content_browser->get_host(content_browser);
          if (host)
          {
            HWND content_hwnd = host->get_window_handle(host);
            if (content_hwnd)
            {
              MoveWindow(content_hwnd, 1, content_y, main_area_w, content_h, TRUE);
              ShowWindow(content_hwnd, SW_SHOW);
            }
            host->base.release(&host->base);
          }
        }
        if (active_tab->right_browser)
        {
          cef_browser_host_t *r_host = active_tab->right_browser->get_host(active_tab->right_browser);
          if (r_host)
          {
            HWND right_hwnd = r_host->get_window_handle(r_host);
            if (right_hwnd) ShowWindow(right_hwnd, SW_HIDE);
            r_host->base.release(&r_host->base);
          }
        }
      }

      for (int k = 0; k < win_ctx->tab_count; k++) {
        if (k != win_ctx->active_tab_index) {
          if (win_ctx->tabs[k].hwnd) ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
          if (win_ctx->tabs[k].right_hwnd) ShowWindow(win_ctx->tabs[k].right_hwnd, SW_HIDE);
        }
      }
    }

    // Position AI sidepanel browser
    if (win_ctx->sidepanel_browser)
    {
      cef_browser_host_t *sp_host = win_ctx->sidepanel_browser->get_host(win_ctx->sidepanel_browser);
      if (sp_host)
      {
        HWND sp_hwnd = sp_host->get_window_handle(sp_host);
        if (sp_hwnd)
        {
          if (win_ctx->show_sidepanel) {
            MoveWindow(sp_hwnd, sp_x, content_y, sp_w, content_h, TRUE);
            ShowWindow(sp_hwnd, SW_SHOW);
          } else {
            ShowWindow(sp_hwnd, SW_HIDE);
          }
        }
        sp_host->base.release(&sp_host->base);
      }
    }

    // Position UI browser LAST and bring it to TOP so it sits above content_hwnd
    if (win_ctx->ui_browser)
    {
      cef_browser_host_t *host = win_ctx->ui_browser->get_host(win_ctx->ui_browser);
      if (host)
      {
        HWND ui_hwnd = host->get_window_handle(host);
        if (ui_hwnd)
        {
          int target_ui_h = (win_ctx->is_ui_expanded && win_ctx->ui_expanded_height > 0) 
                              ? win_ctx->ui_expanded_height 
                              : ui_height;
          SetWindowPos(ui_hwnd, HWND_TOP, 0, 0, width, target_ui_h, SWP_SHOWWINDOW);
          BringWindowToTop(ui_hwnd);
        }
        host->base.release(&host->base);
      }
    }

    // Send maximize state to UI browser
    if (win_ctx->ui_browser) {
      char js_cmd[100];
      sprintf(js_cmd, "if (window.updateMaximizeState) { window.updateMaximizeState(%d); }", IsZoomed(hwnd));
      cef_frame_t *frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
      if (frame) {
        cef_string_t js_str = {};
        cef_string_from_utf8(js_cmd, strlen(js_cmd), &js_str);
        frame->execute_java_script(frame, &js_str, NULL, 0);
        cef_string_clear(&js_str);
        frame->base.release(&frame->base);
      }
    }

    return 0;
  }
  case WM_CLOSE:
    LogMsg("WM_CLOSE: hwnd = %p, win_ctx = %p\n", hwnd, win_ctx);
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
  {
    LogMsg("WM_DESTROY: start. hwnd = %p, win_ctx = %p, current g_window_count = %d\n", hwnd, win_ctx, g_window_count);
    
    WINDOWPLACEMENT wp = {0};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &wp)) {
      char config_path[MAX_PATH];
      GetConfigFilePath(config_path, sizeof(config_path));
      FILE* f = fopen(config_path, "w");
      if (f) {
        int saveCmd = wp.showCmd;
        if (saveCmd == SW_SHOWMINIMIZED || saveCmd == SW_MINIMIZE || saveCmd == SW_SHOWMINNOACTIVE) {
          saveCmd = SW_SHOWNORMAL;
        }
        fprintf(f, "%d %d %d %d %d\n",
                wp.rcNormalPosition.left,
                wp.rcNormalPosition.top,
                wp.rcNormalPosition.right,
                wp.rcNormalPosition.bottom,
                saveCmd);
        fclose(f);
        LogMsg("WM_DESTROY: Saved placement: left=%d, top=%d, right=%d, bottom=%d, showCmd=%d to %s\n",
               wp.rcNormalPosition.left, wp.rcNormalPosition.top,
               wp.rcNormalPosition.right, wp.rcNormalPosition.bottom,
               saveCmd, config_path);
      }
    }

    if (win_ctx)
    {
      // Remove from global tracker
      for (int i = 0; i < MAX_WINDOWS; i++)
      {
        if (g_windows[i] == win_ctx)
        {
          g_windows[i] = NULL;
          g_window_count--;
          LogMsg("WM_DESTROY: found win_ctx, decremented g_window_count. new count = %d\n", g_window_count);
          break;
        }
      }

      // Note: CEF browser close calls are asynchronous,
      // they will trigger on_before_close and cleanups.
      if (win_ctx->ui_browser)
      {
        cef_browser_host_t* host = win_ctx->ui_browser->get_host(win_ctx->ui_browser);
        if (host)
        {
          host->close_browser(host, 1);
          host->base.release(&host->base);
        }
      }

      if (win_ctx->sidepanel_browser)
      {
        cef_browser_host_t* host = win_ctx->sidepanel_browser->get_host(win_ctx->sidepanel_browser);
        if (host)
        {
          host->close_browser(host, 1);
          host->base.release(&host->base);
        }
      }

      for (int i = 0; i < win_ctx->tab_count; i++)
      {
        if (win_ctx->tabs[i].browser)
        {
          cef_browser_host_t* host = win_ctx->tabs[i].browser->get_host(win_ctx->tabs[i].browser);
          if (host)
          {
            host->close_browser(host, 1);
            host->base.release(&host->base);
          }
        }
      }
      // free(win_ctx); // Asynchronous cleanup will free this in on_before_close
    }

    LogMsg("WM_DESTROY: checking quit. g_window_count = %d\n", g_window_count);
    if (g_window_count == 0)
    {
      LogMsg("WM_DESTROY: calling cef_quit_message_loop()\n");
      cef_quit_message_loop();
    }
    return 0;
  }
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

browser_window_t* create_browser_window(const char* startup_url) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  static int class_registered = 0;
  if (!class_registered) {
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = LiteBrowserMainWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(228, 228, 231));
    wcex.lpszClassName = L"LiteBrowserMainWindowClass";
    RegisterClassEx(&wcex);
    class_registered = 1;
  }

  browser_window_t* win_ctx = (browser_window_t*)calloc(1, sizeof(browser_window_t));
  if (!win_ctx) return NULL;

  HWND main_hwnd = CreateWindowEx(
      0, L"LiteBrowserMainWindowClass", L"Lite Browser",
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, NULL, NULL, hInstance, NULL);

  if (!main_hwnd) {
    free(win_ctx);
    return NULL;
  }

  // DWM Shadow Effect
  MARGINS margins = { 1, 1, 1, 1 };
  DwmExtendFrameIntoClientArea(main_hwnd, &margins);

  win_ctx->main_hwnd = main_hwnd;
  SetWindowLongPtr(main_hwnd, GWLP_USERDATA, (LONG_PTR)win_ctx);

  // Load and apply window placement
  int loaded = 0;
  WINDOWPLACEMENT wp = {0};
  wp.length = sizeof(WINDOWPLACEMENT);

  char config_path[MAX_PATH];
  GetConfigFilePath(config_path, sizeof(config_path));
  FILE* f = fopen(config_path, "r");
  if (f) {
    int left = 0, top = 0, right = 0, bottom = 0, showCmd = 0;
    if (fscanf(f, "%d %d %d %d %d", &left, &top, &right, &bottom, &showCmd) == 5) {
      wp.showCmd = showCmd;
      wp.rcNormalPosition.left = left;
      wp.rcNormalPosition.top = top;
      wp.rcNormalPosition.right = right;
      wp.rcNormalPosition.bottom = bottom;
      loaded = 1;
    }
    fclose(f);
  }

  if (loaded) {
    SetWindowPlacement(main_hwnd, &wp);
  } else {
    ShowWindow(main_hwnd, SW_SHOWMAXIMIZED);
  }
  UpdateWindow(main_hwnd);

  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (g_windows[i] == NULL) {
      g_windows[i] = win_ctx;
      g_window_count++;
      break;
    }
  }

  RECT rect;
  GetClientRect(main_hwnd, &rect);
  int width = rect.right;
  int height = rect.bottom;

  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  int ui_height = GetUIHeightForWindow(main_hwnd);
  int content_y = ui_height + 1;
  int content_h = height - content_y - 1;

  // 1. Create UI child browser
  cef_window_info_t ui_window_info = {};
  ui_window_info.size = sizeof(cef_window_info_t);
  ui_window_info.style =
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
  ui_window_info.parent_window = main_hwnd;
  ui_window_info.bounds.x = 0;
  ui_window_info.bounds.y = 0;
  ui_window_info.bounds.width = width;
  ui_window_info.bounds.height = ui_height;
  ui_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t ui_url = {};
  ResolveUIPath(&ui_url);

  simple_handler_t *ui_handler = simple_handler_create(0);
  ui_handler->window_ctx = win_ctx;
  ui_handler->type = BROWSER_TYPE_UI;

  cef_browser_host_create_browser(&ui_window_info, &ui_handler->client,
                                  &ui_url, &browser_settings, NULL, NULL);
  cef_string_clear(&ui_url);

  // 2. Create Content child browser (Tab 1)
  cef_window_info_t content_window_info = {};
  content_window_info.size = sizeof(cef_window_info_t);
  content_window_info.style =
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  content_window_info.parent_window = main_hwnd;
  content_window_info.bounds.x = 1;
  content_window_info.bounds.y = content_y;
  content_window_info.bounds.width = width - 2;
  content_window_info.bounds.height = content_h;
  content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t content_url = {};
  cef_string_from_ascii(startup_url, strlen(startup_url), &content_url);

  simple_handler_t *content_handler = simple_handler_create(0);
  content_handler->window_ctx = win_ctx;
  content_handler->type = BROWSER_TYPE_CONTENT;

  win_ctx->tabs[0].tab_id = 1;
  win_ctx->tabs[0].browser = NULL;
  win_ctx->tabs[0].hwnd = NULL;
  strncpy(win_ctx->tabs[0].url, startup_url, sizeof(win_ctx->tabs[0].url) - 1);
  strcpy(win_ctx->tabs[0].title, "로딩 중...");
  win_ctx->tabs[0].is_loaded = 1;
  win_ctx->tabs[0].tab_handler = content_handler;
  win_ctx->active_tab_index = 0;
  win_ctx->tab_count = 1;

  cef_browser_host_create_browser(
      &content_window_info, &content_handler->client, &content_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&content_url);

  // 3. Create Sidepanel child browser (Initially hidden)
  int default_sp_w = (int)(380 * (ui_height / 72.0f));
  cef_window_info_t sidepanel_window_info = {};
  sidepanel_window_info.size = sizeof(cef_window_info_t);
  sidepanel_window_info.style =
      WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  sidepanel_window_info.parent_window = main_hwnd;
  sidepanel_window_info.bounds.x = width - default_sp_w;
  sidepanel_window_info.bounds.y = content_y;
  sidepanel_window_info.bounds.width = default_sp_w;
  sidepanel_window_info.bounds.height = content_h;
  sidepanel_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t sp_url = {};
  ResolveSidepanelPath(&sp_url);

  simple_handler_t *sidepanel_handler = simple_handler_create(0);
  sidepanel_handler->window_ctx = win_ctx;
  sidepanel_handler->type = BROWSER_TYPE_SIDEPANEL;

  win_ctx->sidepanel_browser = NULL;
  win_ctx->sidepanel_hwnd = NULL;
  win_ctx->show_sidepanel = 0;
  win_ctx->sidepanel_width = default_sp_w;
  win_ctx->sidepanel_handler = sidepanel_handler;

  cef_browser_host_create_browser(
      &sidepanel_window_info, &sidepanel_handler->client, &sp_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&sp_url);

  return win_ctx;
}

browser_window_t* create_browser_window_for_detached(cef_browser_t* detached_browser, HWND detached_hwnd, const char* url, const char* title, int x, int y) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  browser_window_t* win_ctx = (browser_window_t*)calloc(1, sizeof(browser_window_t));
  if (!win_ctx) return NULL;

  HWND main_hwnd = CreateWindowEx(
      0, L"LiteBrowserMainWindowClass", L"Lite Browser",
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      x, y, 1024, 768, NULL, NULL, hInstance, NULL);

  if (!main_hwnd) {
    free(win_ctx);
    return NULL;
  }

  // DWM Shadow Effect
  MARGINS margins = { 1, 1, 1, 1 };
  DwmExtendFrameIntoClientArea(main_hwnd, &margins);

  win_ctx->main_hwnd = main_hwnd;
  SetWindowLongPtr(main_hwnd, GWLP_USERDATA, (LONG_PTR)win_ctx);

  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (g_windows[i] == NULL) {
      g_windows[i] = win_ctx;
      g_window_count++;
      break;
    }
  }

  RECT rect;
  GetClientRect(main_hwnd, &rect);
  int width = rect.right;
  int height = rect.bottom;

  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  int ui_height = GetUIHeightForWindow(main_hwnd);
  int content_y = ui_height + 1;
  int content_h = height - content_y - 1;

  // 1. Create UI child browser
  cef_window_info_t ui_window_info = {};
  ui_window_info.size = sizeof(cef_window_info_t);
  ui_window_info.style =
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
  ui_window_info.parent_window = main_hwnd;
  ui_window_info.bounds.x = 0;
  ui_window_info.bounds.y = 0;
  ui_window_info.bounds.width = width;
  ui_window_info.bounds.height = ui_height;
  ui_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t ui_url = {};
  ResolveUIPath(&ui_url);

  simple_handler_t *ui_handler = simple_handler_create(0);
  ui_handler->window_ctx = win_ctx;
  ui_handler->type = BROWSER_TYPE_UI;

  cef_browser_host_create_browser(&ui_window_info, &ui_handler->client,
                                  &ui_url, &browser_settings, NULL, NULL);
  cef_string_clear(&ui_url);

  // 2. Attach the detached browser (SetParent)
  SetParent(detached_hwnd, main_hwnd);

  // Modify styles to act as child of the new window
  DWORD style = GetWindowLong(detached_hwnd, GWL_STYLE);
  style &= ~WS_POPUP;
  style |= WS_CHILD | WS_VISIBLE;
  SetWindowLong(detached_hwnd, GWL_STYLE, style);

  // Update bounds
  MoveWindow(detached_hwnd, 1, content_y, width - 2, content_h, TRUE);

  // Assign to tabs
  win_ctx->tabs[0].tab_id = 1;
  win_ctx->tabs[0].browser = detached_browser;
  win_ctx->tabs[0].hwnd = detached_hwnd;
  strncpy(win_ctx->tabs[0].url, url, sizeof(win_ctx->tabs[0].url) - 1);
  strncpy(win_ctx->tabs[0].title, title, sizeof(win_ctx->tabs[0].title) - 1);
  win_ctx->tabs[0].is_loaded = 1;
  win_ctx->active_tab_index = 0;
  win_ctx->tab_count = 1;

  // Let the browser host know it has been resized and focus it
  cef_browser_host_t* host = detached_browser->get_host(detached_browser);
  if (host) {
    host->was_resized(host);
    host->set_focus(host, 1);
    host->base.release(&host->base);
  }

  // 3. Create Sidepanel child browser (Initially hidden)
  int default_sp_w = (int)(380 * (ui_height / 72.0f));
  cef_window_info_t sidepanel_window_info = {};
  sidepanel_window_info.size = sizeof(cef_window_info_t);
  sidepanel_window_info.style =
      WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  sidepanel_window_info.parent_window = main_hwnd;
  sidepanel_window_info.bounds.x = width - default_sp_w;
  sidepanel_window_info.bounds.y = content_y;
  sidepanel_window_info.bounds.width = default_sp_w;
  sidepanel_window_info.bounds.height = content_h;
  sidepanel_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t sp_url = {};
  ResolveSidepanelPath(&sp_url);

  simple_handler_t *sidepanel_handler = simple_handler_create(0);
  sidepanel_handler->window_ctx = win_ctx;
  sidepanel_handler->type = BROWSER_TYPE_SIDEPANEL;

  win_ctx->sidepanel_browser = NULL;
  win_ctx->sidepanel_hwnd = NULL;
  win_ctx->show_sidepanel = 0;
  win_ctx->sidepanel_width = default_sp_w;
  win_ctx->sidepanel_handler = sidepanel_handler;

  cef_browser_host_create_browser(
      &sidepanel_window_info, &sidepanel_handler->client, &sp_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&sp_url);

  ShowWindow(main_hwnd, SW_SHOW);
  UpdateWindow(main_hwnd);

  return win_ctx;
}
#endif

// Called after CEF initialization to create the browser.
void CEF_CALLBACK browser_process_handler_on_context_initialized(
    cef_browser_process_handler_t *self)
{
  // Get the global command line.
  cef_command_line_t *command_line = cef_command_line_get_global();
  CHECK(command_line);

  // Specify CEF browser settings.
  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  // Get the URL from command line or use default.
  cef_string_t url_switch = {};
  cef_string_from_ascii("url", 3, &url_switch);
  cef_string_userfree_t url_value =
      command_line->get_switch_value(command_line, &url_switch);
  cef_string_clear(&url_switch);

  cef_string_t url = {};
  if (url_value && url_value->length > 0)
  {
    cef_string_copy(url_value->str, url_value->length, &url);
  }
  else
  {
    cef_string_from_ascii("lite://favorites", 16, &url);
  }

  cef_string_utf8_t url_utf8 = {};
  cef_string_to_utf8(url.str, url.length, &url_utf8);
  if (url_utf8.str && url_utf8.length > 0)
  {
    strncpy(g_startup_url, url_utf8.str, sizeof(g_startup_url) - 1);
    g_startup_url[sizeof(g_startup_url) - 1] = '\0';
  }
  cef_string_utf8_clear(&url_utf8);

  if (url_value)
  {
    cef_string_userfree_free(url_value);
  }

  // Suppress Chromium download bubble partial view and auto-open popups via global preferences
  cef_preference_manager_t* pref_mgr = cef_preference_manager_get_global();
  if (pref_mgr) {
    const char* prefs[] = {
      "download_bubble.partial_view_enabled",
      "download_bubble.auto_open",
      "download.prompt_for_download",
      NULL
    };
    for (int i = 0; prefs[i] != NULL; i++) {
      cef_string_t name = {};
      cef_string_from_ascii(prefs[i], strlen(prefs[i]), &name);
      if (pref_mgr->can_set_preference(pref_mgr, &name)) {
        cef_value_t* val = cef_value_create();
        val->set_bool(val, 0);
        cef_string_t err = {};
        pref_mgr->set_preference(pref_mgr, &name, val, &err);
        cef_string_clear(&err);
        val->base.release(&val->base);
      }
      cef_string_clear(&name);
    }
  }

#if defined(OS_WIN)
  create_browser_window(g_startup_url);
#else
  // Non-Windows fallback (views or generic single window)
  cef_window_info_t window_info = {};
  window_info.size = sizeof(cef_window_info_t);
  window_info.bounds.width = 800;
  window_info.bounds.height = 600;
  window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  simple_handler_t *client_handler = simple_handler_create(0);
  cef_browser_host_create_browser(&window_info, &client_handler->client, &url,
                                  &browser_settings, NULL, NULL);
#endif

  cef_string_clear(&url);
}

// Handles relaunch when another instance is already running with the same cache/profile directory.
int CEF_CALLBACK browser_process_handler_on_already_running_app_relaunch(
    cef_browser_process_handler_t *self,
    struct _cef_command_line_t *command_line,
    const cef_string_t *current_directory) {
  (void)self;
  (void)current_directory;

  char target_url[4096] = "";
  if (g_startup_url[0] != '\0') {
    strncpy(target_url, g_startup_url, sizeof(target_url) - 1);
  } else {
    strcpy(target_url, "lite://favorites");
  }

  if (command_line) {
    cef_string_t url_switch = {};
    cef_string_from_ascii("url", 3, &url_switch);
    cef_string_userfree_t url_value =
        command_line->get_switch_value(command_line, &url_switch);
    cef_string_clear(&url_switch);

    if (url_value && url_value->length > 0) {
      cef_string_utf8_t url_utf8 = {};
      cef_string_to_utf8(url_value->str, url_value->length, &url_utf8);
      if (url_utf8.str && url_utf8.length > 0) {
        strncpy(target_url, url_utf8.str, sizeof(target_url) - 1);
        target_url[sizeof(target_url) - 1] = '\0';
      }
      cef_string_utf8_clear(&url_utf8);
    }
    if (url_value) {
      cef_string_userfree_free(url_value);
    }
  }

#if defined(OS_WIN)
  browser_window_t* win_ctx = create_browser_window(target_url);
  if (win_ctx && win_ctx->main_hwnd) {
    if (IsIconic(win_ctx->main_hwnd)) {
      ShowWindow(win_ctx->main_hwnd, SW_RESTORE);
    }
    SetForegroundWindow(win_ctx->main_hwnd);
  }
#endif

  // Return 1 (true) to indicate relaunch was handled and suppress default Chrome window
  return 1;
}

// Returns the default client handler for Chrome style UI.
cef_client_t *CEF_CALLBACK browser_process_handler_get_default_client(
    cef_browser_process_handler_t *self) {
  // Return the global instance (matches C++ SimpleApp::GetDefaultClient).
  simple_handler_t *instance = simple_handler_get_instance();
  if (instance) {
    // Add reference before returning (CEF will release it).
    instance->client.base.add_ref(&instance->client.base);
    return &instance->client;
  }

  return NULL;
}

// Creates a browser process handler instance.
simple_browser_process_handler_t *browser_process_handler_create(void) {
  simple_browser_process_handler_t *handler =
      (simple_browser_process_handler_t *)calloc(
          1, sizeof(simple_browser_process_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base,
                           cef_browser_process_handler_t,
                           browser_process_handler);

  // Set callbacks.
  handler->handler.on_context_initialized =
      browser_process_handler_on_context_initialized;
  handler->handler.get_default_client =
      browser_process_handler_get_default_client;
  handler->handler.on_already_running_app_relaunch =
      browser_process_handler_on_already_running_app_relaunch;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}

// Creates the application instance.
simple_app_t *simple_app_create(void) {
  simple_app_t *app = (simple_app_t *)calloc(1, sizeof(simple_app_t));
  CHECK(app);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&app->app.base, cef_app_t, simple_app);

  // Set callbacks.
  app->app.get_browser_process_handler = simple_app_get_browser_process_handler;
  app->app.on_before_command_line_processing = simple_app_on_before_command_line_processing;

  // Create the browser process handler.
  app->browser_process_handler = browser_process_handler_create();
  CHECK(app->browser_process_handler);

  // Initialize with ref count of 1.
  atomic_store(&app->ref_count, 1);

  return app;
}
