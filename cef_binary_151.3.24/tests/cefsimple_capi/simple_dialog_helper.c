#include "tests/cefsimple_capi/simple_dialog_helper.h"
#include "tests/cefsimple_capi/browser_context.h"
#include "tests/cefsimple_capi/simple_download_handler.h"

#if defined(OS_WIN) || defined(_WIN32)
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")

#define SUBCLASS_ID_MODAL_DIALOG 9002

static HHOOK g_cbt_hook = NULL;

// Forward declaration of the window subclass procedure
static LRESULT CALLBACK ModalDialogSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

static int is_download_bubble_window(HWND dialog_hwnd, int dialog_w, HWND root_owner, browser_window_t* win_ctx) {
  if (!dialog_hwnd || !IsWindow(dialog_hwnd)) return 0;

  // 1. Check cached property tag for quick and consistent lookup
  HANDLE prop = GetPropA(dialog_hwnd, "LiteBrowser_IsBubble");
  if (prop != NULL) {
    return (prop == (HANDLE)1) ? 1 : 0;
  }

  DWORD ex_style = GetWindowLong(dialog_hwnd, GWL_EXSTYLE);

  // 2. Web modal dialogs (alert, confirm, prompt, auth, beforeunload) ALWAYS have WS_EX_DLGMODALFRAME
  if (ex_style & WS_EX_DLGMODALFRAME) {
    SetPropA(dialog_hwnd, "LiteBrowser_IsBubble", (HANDLE)2);  // 2 = marked as web modal
    return 0;
  }

  // 3. Download bubble in Chromium ALWAYS has WS_EX_TOOLWINDOW
  if (ex_style & WS_EX_TOOLWINDOW) {
    UINT dpi = root_owner ? GetDpiForWindow(root_owner) : 96;
    int expected_bubble_w = (int)(418.0 * ((double)dpi / 96.0));
    int is_w_match = (dialog_w > 0 && abs(dialog_w - expected_bubble_w) <= 8);
    int bubble_exp = simple_download_is_bubble_expected();

    if (bubble_exp || is_w_match) {
      SetPropA(dialog_hwnd, "LiteBrowser_IsBubble", (HANDLE)1);
      return 1;
    }
  }

  // 4. Fallback: Check window title keyword if present
  WCHAR wtitle[128] = {0};
  if (GetWindowTextW(dialog_hwnd, wtitle, sizeof(wtitle)/sizeof(wtitle[0])) > 0) {
    if (wcsstr(wtitle, L"다운로드") || wcsstr(wtitle, L"Download")) {
      SetPropA(dialog_hwnd, "LiteBrowser_IsBubble", (HANDLE)1);
      return 1;
    }
  }

  // Default: Web Modal Dialog (centered) to eliminate false positives
  SetPropA(dialog_hwnd, "LiteBrowser_IsBubble", (HANDLE)2);
  return 0;
}

static int calculate_dialog_target_pos(HWND dialog_hwnd, int dialog_w, int dialog_h, int* out_x, int* out_y) {
  if (!dialog_hwnd || !IsWindow(dialog_hwnd)) return 0;

  HWND root_owner = GetAncestor(dialog_hwnd, GA_ROOTOWNER);
  if (!root_owner) {
    root_owner = GetWindow(dialog_hwnd, GW_OWNER);
    if (root_owner) root_owner = GetAncestor(root_owner, GA_ROOTOWNER);
  }
  if (!root_owner || !IsWindow(root_owner)) return 0;

  browser_window_t* win_ctx = (browser_window_t*)GetWindowLongPtr(root_owner, GWLP_USERDATA);
  if (!win_ctx || win_ctx->main_hwnd != root_owner) return 0;

  // 1. Download Bubble positioning (Edge style: aligned to right edge, 2px below toolbar)
  if (is_download_bubble_window(dialog_hwnd, dialog_w, root_owner, win_ctx)) {
    RECT rc_client;
    GetClientRect(root_owner, &rc_client);
    POINT pt_tr = { rc_client.right, 0 };
    ClientToScreen(root_owner, &pt_tr);
    POINT pt_tl = { 0, 0 };
    ClientToScreen(root_owner, &pt_tl);

    UINT dpi = GetDpiForWindow(root_owner);
    int ui_h = (int)(72.0 * ((double)dpi / 96.0));
    int margin_x = (int)(1.0 * ((double)dpi / 96.0));
    if (margin_x < 1) margin_x = 1;
    int margin_y = (int)(2.0 * ((double)dpi / 96.0));

    int target_x = pt_tr.x - dialog_w - margin_x;
    int target_y = pt_tl.y + ui_h + margin_y;

    HMONITOR hMon = MonitorFromWindow(root_owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};
    if (GetMonitorInfo(hMon, &mi)) {
      if (target_x < mi.rcWork.left) target_x = mi.rcWork.left;
      if (target_x + dialog_w > mi.rcWork.right) target_x = mi.rcWork.right - dialog_w;
      if (target_y < mi.rcWork.top) target_y = mi.rcWork.top;
      if (target_y + dialog_h > mi.rcWork.bottom) target_y = mi.rcWork.bottom - dialog_h;
    }

    *out_x = target_x;
    *out_y = target_y;
    return 1;
  }

  // 2. Web Modal Dialog positioning (alert, confirm, beforeunload, http auth - centered over tab content)
  HWND target_content_hwnd = NULL;
  if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
    tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
    if (active_tab->is_split && active_tab->right_hwnd && IsWindow(active_tab->right_hwnd)) {
      HWND owner = GetWindow(dialog_hwnd, GW_OWNER);
      int is_right = 0;
      HWND cur = owner;
      while (cur && cur != root_owner) {
        if (cur == active_tab->right_hwnd) {
          is_right = 1;
          break;
        }
        cur = GetParent(cur);
      }
      if (!is_right && active_tab->active_split == 1) {
        is_right = 1;
      }
      target_content_hwnd = is_right ? active_tab->right_hwnd : active_tab->hwnd;
    } else {
      target_content_hwnd = active_tab->hwnd;
    }
  }

  RECT target_rect = {0};
  if (target_content_hwnd && IsWindow(target_content_hwnd)) {
    GetWindowRect(target_content_hwnd, &target_rect);
  } else {
    GetWindowRect(root_owner, &target_rect);
    UINT dpi = GetDpiForWindow(root_owner);
    int ui_h = (int)(72.0 * ((double)dpi / 96.0));
    target_rect.top += ui_h;
  }

  int content_w = target_rect.right - target_rect.left;
  int target_x = target_rect.left + (content_w - dialog_w) / 2;
  int target_y = target_rect.top;  // 0px offset directly below toolbar (Edge style)

  // Clamp to current monitor work area so dialog never falls off screen
  HMONITOR hMon = MonitorFromRect(&target_rect, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(MONITORINFO)};
  if (GetMonitorInfo(hMon, &mi)) {
    if (target_x < mi.rcWork.left) target_x = mi.rcWork.left;
    if (target_x + dialog_w > mi.rcWork.right) target_x = mi.rcWork.right - dialog_w;
    if (target_y < mi.rcWork.top) target_y = mi.rcWork.top;
    if (target_y + dialog_h > mi.rcWork.bottom) target_y = mi.rcWork.bottom - dialog_h;
  }

  *out_x = target_x;
  *out_y = target_y;
  return 1;
}

static BOOL is_candidate_dialog_window(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) return FALSE;

  char cls[128] = {0};
  if (GetClassNameA(hwnd, cls, sizeof(cls)) <= 0) return FALSE;
  if (strncmp(cls, "Chrome_WidgetWin_", 17) != 0) return FALSE;

  DWORD style = GetWindowLong(hwnd, GWL_STYLE);
  if (!(style & WS_POPUP) || (style & WS_CHILD)) return FALSE;

  HWND root_owner = GetAncestor(hwnd, GA_ROOTOWNER);
  if (!root_owner || root_owner == hwnd) {
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner) root_owner = GetAncestor(owner, GA_ROOTOWNER);
  }
  if (!root_owner || root_owner == hwnd) return FALSE;

  browser_window_t* win_ctx = (browser_window_t*)GetWindowLongPtr(root_owner, GWLP_USERDATA);
  if (!win_ctx || win_ctx->main_hwnd != root_owner) return FALSE;

  return TRUE;
}

static LRESULT CALLBACK ModalDialogSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  switch (uMsg) {
    case WM_WINDOWPOSCHANGING: {
      WINDOWPOS* pos = (WINDOWPOS*)lParam;
      if (pos) {
        int w = pos->cx;
        int h = pos->cy;
        if ((pos->flags & SWP_NOSIZE) || w <= 0 || h <= 0) {
          RECT wr;
          GetWindowRect(hWnd, &wr);
          w = wr.right - wr.left;
          h = wr.bottom - wr.top;
        }

        // Modal dialog and download bubble size filter (excludes ~28px link hover tooltips)
        if (w >= 240 && w <= 950 && h >= 80 && h <= 650) {
          int tx = 0, ty = 0;
          if (calculate_dialog_target_pos(hWnd, w, h, &tx, &ty)) {
            pos->x = tx;
            pos->y = ty;
            pos->flags &= ~SWP_NOMOVE;
          }
        }
      }
      break;
    }

    case WM_SHOWWINDOW: {
      if (wParam) {
        RECT wr;
        GetWindowRect(hWnd, &wr);
        int w = wr.right - wr.left;
        int h = wr.bottom - wr.top;
        if (w >= 240 && w <= 950 && h >= 80 && h <= 650) {
          int tx = 0, ty = 0;
          if (calculate_dialog_target_pos(hWnd, w, h, &tx, &ty)) {
            SetWindowPos(hWnd, NULL, tx, ty, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
          }
        }
      }
      break;
    }

    case WM_NCDESTROY:
      RemovePropA(hWnd, "LiteBrowser_IsBubble");
      RemoveWindowSubclass(hWnd, ModalDialogSubclassProc, uIdSubclass);
      break;
  }

  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HCBT_CREATEWND) {
    HWND hwnd = (HWND)wParam;
    CBT_CREATEWND* cw = (CBT_CREATEWND*)lParam;
    if (cw && cw->lpcs) {
      DWORD style = cw->lpcs->style;
      if ((style & WS_POPUP) && !(style & WS_CHILD)) {
        char cls[128] = {0};
        GetClassNameA(hwnd, cls, sizeof(cls));
        int is_chrome = (strncmp(cls, "Chrome_WidgetWin_", 17) == 0);
        if (!is_chrome && cw->lpcs->lpszClass && !IS_INTRESOURCE(cw->lpcs->lpszClass)) {
          if (_wcsnicmp(cw->lpcs->lpszClass, L"Chrome_WidgetWin_", 17) == 0) {
            is_chrome = 1;
          }
        }
        if (is_chrome) {
          SetWindowSubclass(hwnd, ModalDialogSubclassProc, SUBCLASS_ID_MODAL_DIALOG, 0);
        }
      }
    }
  } else if (nCode == HCBT_ACTIVATE) {
    HWND hwnd = (HWND)wParam;
    if (is_candidate_dialog_window(hwnd)) {
      SetWindowSubclass(hwnd, ModalDialogSubclassProc, SUBCLASS_ID_MODAL_DIALOG, 0);

      RECT wr;
      GetWindowRect(hwnd, &wr);
      int w = wr.right - wr.left;
      int h = wr.bottom - wr.top;
      if (w >= 240 && w <= 950 && h >= 80 && h <= 650) {
        int tx = 0, ty = 0;
        if (calculate_dialog_target_pos(hwnd, w, h, &tx, &ty)) {
          SetWindowPos(hwnd, NULL, tx, ty, 0, 0,
                       SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
      }
    }
  }

  return CallNextHookEx(g_cbt_hook, nCode, wParam, lParam);
}

void simple_dialog_helper_init(void) {
  if (!g_cbt_hook) {
    g_cbt_hook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());
  }
}

void simple_dialog_helper_cleanup(void) {
  if (g_cbt_hook) {
    UnhookWindowsHookEx(g_cbt_hook);
    g_cbt_hook = NULL;
  }
}

static BOOL CALLBACK EnumRepositionChildDialogs(HWND child, LPARAM lParam) {
  HWND main_hwnd = (HWND)lParam;
  if (is_candidate_dialog_window(child)) {
    HWND root_owner = GetAncestor(child, GA_ROOTOWNER);
    if (!root_owner) {
      HWND owner = GetWindow(child, GW_OWNER);
      if (owner) root_owner = GetAncestor(owner, GA_ROOTOWNER);
    }
    if (root_owner == main_hwnd && IsWindowVisible(child)) {
      RECT wr;
      GetWindowRect(child, &wr);
      int w = wr.right - wr.left;
      int h = wr.bottom - wr.top;
      if (w >= 240 && w <= 950 && h >= 80 && h <= 650) {
        int tx, ty;
        if (calculate_dialog_target_pos(child, w, h, &tx, &ty)) {
          SetWindowPos(child, NULL, tx, ty, 0, 0,
                       SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
      }
    }
  }
  return TRUE;
}

void simple_dialog_helper_reposition_open_dialogs(HWND main_hwnd) {
  if (!main_hwnd || !IsWindow(main_hwnd)) return;
  EnumThreadWindows(GetCurrentThreadId(), EnumRepositionChildDialogs, (LPARAM)main_hwnd);
}

#endif  // defined(OS_WIN) || defined(_WIN32)
