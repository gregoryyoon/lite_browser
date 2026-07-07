#include "tests/cefsimple_capi/simple_app.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif
#include <stdarg.h>
#include <stdio.h>

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

// Resolves local path to ui/editor.html
static void ResolveEditorPath(cef_string_t *out_url) {
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
    snprintf(test_path, sizeof(test_path), "%s\\ui\\editor.html", path);
    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES &&
        !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
      char file_url[MAX_PATH + 16];
      snprintf(file_url, sizeof(file_url), "file:///%s/ui/editor.html", path);
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
    cef_string_from_ascii("file:///C:/projects/lite_browser/ui/editor.html", 47,
                          out_url);
  }
#else
  // Fallback for non-Windows (e.g. Linux/Mac)
  cef_string_from_ascii("file:///projects/lite_browser/ui/editor.html", 44,
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
  cef_string_t switch1 = {};
  cef_string_from_ascii("disable-web-security", 20, &switch1);
  command_line->append_switch(command_line, &switch1);
  cef_string_clear(&switch1);

  cef_string_t switch2 = {};
  cef_string_from_ascii("allow-file-access-from-files", 28, &switch2);
  command_line->append_switch(command_line, &switch2);
  cef_string_clear(&switch2);
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
  case WM_SIZE:
  {
    if (!win_ctx) return 0;
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);

    if (win_ctx->ui_browser)
    {
      cef_browser_host_t *host = win_ctx->ui_browser->get_host(win_ctx->ui_browser);
      if (host)
      {
        HWND ui_hwnd = host->get_window_handle(host);
        if (ui_hwnd)
        {
          MoveWindow(ui_hwnd, 0, 0, width, 100, TRUE);
        }
        host->base.release(&host->base);
      }
    }

    int content_w = width - 2;
    int editor_w = 0;

    if (win_ctx->show_editor && win_ctx->editor_hwnd)
    {
      content_w = (width - 3) / 2;
      editor_w = width - 3 - content_w;
      MoveWindow(win_ctx->editor_hwnd, content_w + 2, 101, editor_w, height - 102, TRUE);
    }
    else if (win_ctx->editor_hwnd)
    {
      MoveWindow(win_ctx->editor_hwnd, 0, 0, 0, 0, FALSE);
      ShowWindow(win_ctx->editor_hwnd, SW_HIDE);
    }

    if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count)
    {
      cef_browser_t* content_browser = win_ctx->tabs[win_ctx->active_tab_index].browser;
      if (content_browser)
      {
        cef_browser_host_t *host = content_browser->get_host(content_browser);
        if (host)
        {
          HWND content_hwnd = host->get_window_handle(host);
          if (content_hwnd)
          {
            MoveWindow(content_hwnd, 1, 101, content_w, height - 102, TRUE);
          }
          host->base.release(&host->base);
        }
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
      if (win_ctx->editor_browser)
      {
        cef_browser_host_t* host = win_ctx->editor_browser->get_host(win_ctx->editor_browser);
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
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
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

  // 1. Create UI child browser
  cef_window_info_t ui_window_info = {};
  ui_window_info.size = sizeof(cef_window_info_t);
  ui_window_info.style =
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  ui_window_info.parent_window = main_hwnd;
  ui_window_info.bounds.x = 0;
  ui_window_info.bounds.y = 0;
  ui_window_info.bounds.width = width;
  ui_window_info.bounds.height = 100;
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
  content_window_info.bounds.y = 101;
  content_window_info.bounds.width = width - 2;
  content_window_info.bounds.height = height - 102;
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
  win_ctx->active_tab_index = 0;
  win_ctx->tab_count = 1;

  cef_browser_host_create_browser(
      &content_window_info, &content_handler->client, &content_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&content_url);

  // 3. Create MD Editor child browser
  cef_window_info_t editor_window_info = {};
  editor_window_info.size = sizeof(cef_window_info_t);
  // Initially hidden (do not specify WS_VISIBLE)
  editor_window_info.style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  editor_window_info.parent_window = main_hwnd;
  editor_window_info.bounds.x = 0;
  editor_window_info.bounds.y = 0;
  editor_window_info.bounds.width = 0;
  editor_window_info.bounds.height = 0;
  editor_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t editor_url = {};
  ResolveEditorPath(&editor_url);

  simple_handler_t *editor_handler = simple_handler_create(0);
  editor_handler->window_ctx = win_ctx;
  editor_handler->type = BROWSER_TYPE_EDITOR;

  win_ctx->editor_browser = NULL;
  win_ctx->editor_hwnd = NULL;
  win_ctx->show_editor = 0;

  cef_browser_host_create_browser(&editor_window_info, &editor_handler->client,
                                  &editor_url, &browser_settings, NULL, NULL);
  cef_string_clear(&editor_url);

  return win_ctx;
}

browser_window_t* create_browser_window_for_detached(cef_browser_t* detached_browser, HWND detached_hwnd, const char* url, const char* title, int x, int y) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  browser_window_t* win_ctx = (browser_window_t*)calloc(1, sizeof(browser_window_t));
  if (!win_ctx) return NULL;

  HWND main_hwnd = CreateWindowEx(
      0, L"LiteBrowserMainWindowClass", L"Lite Browser",
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
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

  // 1. Create UI child browser
  cef_window_info_t ui_window_info = {};
  ui_window_info.size = sizeof(cef_window_info_t);
  ui_window_info.style =
      WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  ui_window_info.parent_window = main_hwnd;
  ui_window_info.bounds.x = 0;
  ui_window_info.bounds.y = 0;
  ui_window_info.bounds.width = width;
  ui_window_info.bounds.height = 100;
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
  MoveWindow(detached_hwnd, 1, 101, width - 2, height - 102, TRUE);

  // Assign to tabs
  win_ctx->tabs[0].tab_id = 1;
  win_ctx->tabs[0].browser = detached_browser;
  win_ctx->tabs[0].hwnd = detached_hwnd;
  strncpy(win_ctx->tabs[0].url, url, sizeof(win_ctx->tabs[0].url) - 1);
  strncpy(win_ctx->tabs[0].title, title, sizeof(win_ctx->tabs[0].title) - 1);
  win_ctx->active_tab_index = 0;
  win_ctx->tab_count = 1;

  // Let the browser host know it has been resized and focus it
  cef_browser_host_t* host = detached_browser->get_host(detached_browser);
  if (host) {
    host->was_resized(host);
    host->set_focus(host, 1);
    host->base.release(&host->base);
  }

  // 3. Create MD Editor child browser
  cef_window_info_t editor_window_info = {};
  editor_window_info.size = sizeof(cef_window_info_t);
  // Initially hidden (do not specify WS_VISIBLE)
  editor_window_info.style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  editor_window_info.parent_window = main_hwnd;
  editor_window_info.bounds.x = 0;
  editor_window_info.bounds.y = 0;
  editor_window_info.bounds.width = 0;
  editor_window_info.bounds.height = 0;
  editor_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t editor_url = {};
  ResolveEditorPath(&editor_url);

  simple_handler_t *editor_handler = simple_handler_create(0);
  editor_handler->window_ctx = win_ctx;
  editor_handler->type = BROWSER_TYPE_EDITOR;

  win_ctx->editor_browser = NULL;
  win_ctx->editor_hwnd = NULL;
  win_ctx->show_editor = 0;

  cef_browser_host_create_browser(&editor_window_info, &editor_handler->client,
                                  &editor_url, &browser_settings, NULL, NULL);
  cef_string_clear(&editor_url);

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
    cef_string_from_ascii("https://gemini.google.com", 25, &url);
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
