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
  char url[1024];
} tab_info_t;

typedef struct _browser_window_t {
  HWND main_hwnd;
  cef_browser_t* ui_browser;
  HWND ui_hwnd;
  tab_info_t tabs[MAX_TABS];
  int active_tab_index;
  int tab_count;
} browser_window_t;

#if defined(OS_WIN)
browser_window_t* create_browser_window(const char* startup_url);
browser_window_t* create_browser_window_for_detached(cef_browser_t* detached_browser, HWND detached_hwnd, const char* url, const char* title, int x, int y);
extern int g_window_count;
#endif

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_BROWSER_CONTEXT_H_
