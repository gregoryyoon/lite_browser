#ifndef CEF_TESTS_CEFSIMPLE_CAPI_BROWSER_CONTEXT_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_BROWSER_CONTEXT_H_

#include <windows.h>
#include "include/capi/cef_browser_capi.h"

#define MAX_TABS 20

typedef struct _tab_info_t {
  int tab_id;
  cef_browser_t* browser;
  HWND hwnd;
  char title[256];
  char url[4096];
  int is_loaded;
  char favicon_url[2048];
  void* tab_handler;

  // Dual tab split screen fields
  int is_split;             // 0: single view, 1: dual split view
  cef_browser_t* right_browser;
  HWND right_hwnd;
  char right_title[256];
  char right_url[4096];
  int right_is_loaded;
  char right_favicon_url[2048];
  void* right_tab_handler;
  int active_split;         // 0: left pane, 1: right pane
  float split_ratio;        // ratio of left pane width (0.2 to 0.8, default 0.5)
} tab_info_t;

typedef struct _browser_window_t {
  HWND main_hwnd;
  cef_browser_t* ui_browser;
  HWND ui_hwnd;
  tab_info_t tabs[MAX_TABS];
  int active_tab_index;
  int tab_count;
  int is_ui_expanded;
  int ui_expanded_height;

  // Splitter resizer tracking
  int is_resizing_splitter;
  int drag_start_x;
  float drag_start_ratio;

  // Independent AI Side Panel
  cef_browser_t* sidepanel_browser;
  HWND sidepanel_hwnd;
  int show_sidepanel;
  int sidepanel_width;
  void* sidepanel_handler;
  int is_resizing_sidepanel;
  int sidepanel_drag_start_x;
  int sidepanel_drag_start_w;
} browser_window_t;

#if defined(OS_WIN)
browser_window_t* create_browser_window(const char* startup_url);
browser_window_t* create_browser_window_for_detached(cef_browser_t* detached_browser, HWND detached_hwnd, const char* url, const char* title, int x, int y);
void CreateSidepanelBrowser(browser_window_t* win_ctx);
extern int g_window_count;
#endif

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_BROWSER_CONTEXT_H_
