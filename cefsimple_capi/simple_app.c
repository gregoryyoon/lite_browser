#include "tests/cefsimple_capi/simple_app.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <stdio.h>
#include <stdarg.h>

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_window_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"
#include "tests/cefsimple_capi/simple_views.h"

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

// Resolves local path to ui/index.html
static void ResolveUIPath(cef_string_t* out_url) {
#if defined(OS_WIN)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  char path[MAX_PATH];
  strcpy(path, exe_path);

  int found = 0;
  for (int i = 0; i < 8; i++) {
    char* last_backslash = strrchr(path, '\\');
    if (!last_backslash) break;
    *last_backslash = '\0';

    char test_path[MAX_PATH];
    snprintf(test_path, sizeof(test_path), "%s\\ui\\index.html", path);
    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
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
    cef_string_from_ascii("file:///C:/projects/lite_browser/ui/index.html", 46, out_url);
  }
#else
  // Fallback for non-Windows (e.g. Linux/Mac)
  cef_string_from_ascii("file:///projects/lite_browser/ui/index.html", 43, out_url);
#endif
}

// Implement reference counting functions for simple_app_t.
IMPLEMENT_REFCOUNTING_MANUAL(simple_app_t, simple_app, ref_count)

// Release function for simple_app_t with custom cleanup logic.
int CEF_CALLBACK simple_app_release(cef_base_ref_counted_t* self) {
  simple_app_t* app = (simple_app_t*)self;
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

// Returns the browser process handler.
// Adds a reference before returning (CEF will release it when done).
cef_browser_process_handler_t* CEF_CALLBACK
simple_app_get_browser_process_handler(cef_app_t* self) {
  simple_app_t* app = (simple_app_t*)self;
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
    cef_browser_process_handler_t* self);
cef_client_t* CEF_CALLBACK
browser_process_handler_get_default_client(cef_browser_process_handler_t* self);

// Implement reference counting functions for browser process handler.
IMPLEMENT_REFCOUNTING_SIMPLE(simple_browser_process_handler_t,
                             browser_process_handler,
                             ref_count)

#if defined(OS_WIN)
HWND g_main_hwnd = NULL;

LRESULT CALLBACK LiteBrowserMainWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_SIZE: {
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);
      if (g_ui_browser) {
        cef_browser_host_t* host = g_ui_browser->get_host(g_ui_browser);
        if (host) {
          HWND ui_hwnd = host->get_window_handle(host);
          if (ui_hwnd) {
            MoveWindow(ui_hwnd, 0, 0, width, 80, TRUE);
          }
          host->base.release(&host->base);
        }
      }
      if (g_content_browser) {
        cef_browser_host_t* host = g_content_browser->get_host(g_content_browser);
        if (host) {
          HWND content_hwnd = host->get_window_handle(host);
          if (content_hwnd) {
            MoveWindow(content_hwnd, 0, 80, width, height - 80, TRUE);
          }
          host->base.release(&host->base);
        }
      }
      return 0;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      cef_quit_message_loop();
      return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}
#endif

// Called after CEF initialization to create the browser.
void CEF_CALLBACK browser_process_handler_on_context_initialized(
    cef_browser_process_handler_t* self) {
  // Get the global command line.
  cef_command_line_t* command_line = cef_command_line_get_global();
  CHECK(command_line);

  // Check if Alloy style will be used.
  cef_string_t alloy_switch = {};
  cef_string_from_ascii("use-alloy-style", 15, &alloy_switch);
  int use_alloy_style = command_line->has_switch(command_line, &alloy_switch);
  cef_string_clear(&alloy_switch);

  // Create the client handler.
  simple_handler_t* client_handler = simple_handler_create(use_alloy_style);
  CHECK(client_handler);

  // The client_handler is stored globally via simple_handler_get_instance().
  // GetDefaultClient() will retrieve it from there when needed.

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
  if (url_value && url_value->length > 0) {
    cef_string_copy(url_value->str, url_value->length, &url);
  } else {
    cef_string_from_ascii("https://www.google.com", 22, &url);
  }
  
  cef_string_utf8_t url_utf8 = {};
  cef_string_to_utf8(url.str, url.length, &url_utf8);
  if (url_utf8.str && url_utf8.length > 0) {
    strncpy(g_startup_url, url_utf8.str, sizeof(g_startup_url) - 1);
    g_startup_url[sizeof(g_startup_url) - 1] = '\0';
  }
  cef_string_utf8_clear(&url_utf8);

  if (url_value) {
    cef_string_userfree_free(url_value);
  }

  // Check if Views framework should be used.
  // Views is enabled by default (add `--use-native` to disable).
  cef_string_t native_switch = {};
  cef_string_from_ascii("use-native", 10, &native_switch);
  (void)command_line->has_switch(command_line, &native_switch);
  cef_string_clear(&native_switch);

  int use_views = 0; // Force native Win32 window mode!

  // Determine runtime style.
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
  if (use_alloy_style) {
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  if (use_views) {
    // Create the BrowserView using Views framework.
    simple_browser_view_delegate_t* browser_view_delegate =
        browser_view_delegate_create(runtime_style, 0, 0);
    CHECK(browser_view_delegate);

    // Create the browser view.
    // We transfer our client_handler and browser_view_delegate references to
    // CEF. CEF will release them when the browser view is destroyed.
    cef_browser_view_t* browser_view = cef_browser_view_create(
        &client_handler->client, &url, &browser_settings, NULL, NULL,
        &browser_view_delegate->delegate);

    // Note: We DON'T release browser_view_delegate here - we transferred
    // ownership to CEF.

    if (browser_view) {
      // Optionally configure the initial show state.
      cef_show_state_t initial_show_state = CEF_SHOW_STATE_NORMAL;
      cef_string_t show_state_switch = {};
      cef_string_from_ascii("initial-show-state", 18, &show_state_switch);
      cef_string_userfree_t show_state_value =
          command_line->get_switch_value(command_line, &show_state_switch);
      cef_string_clear(&show_state_switch);

      if (show_state_value && show_state_value->length > 0) {
        // Check for "minimized"
        cef_string_t minimized = {};
        cef_string_from_ascii("minimized", 9, &minimized);
        if (cef_string_utf16_cmp(show_state_value, &minimized) == 0) {
          initial_show_state = CEF_SHOW_STATE_MINIMIZED;
        }
        cef_string_clear(&minimized);

        // Check for "maximized"
        cef_string_t maximized = {};
        cef_string_from_ascii("maximized", 9, &maximized);
        if (cef_string_utf16_cmp(show_state_value, &maximized) == 0) {
          initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
        }
        cef_string_clear(&maximized);

#if defined(OS_MAC)
        // Hidden show state is only supported on macOS.
        cef_string_t hidden = {};
        cef_string_from_ascii("hidden", 6, &hidden);
        if (cef_string_utf16_cmp(show_state_value, &hidden) == 0) {
          initial_show_state = CEF_SHOW_STATE_HIDDEN;
        }
        cef_string_clear(&hidden);
#endif
      }

      if (show_state_value) {
        cef_string_userfree_free(show_state_value);
      }

      // Create the Window. It will show itself after creation.
      // We transfer our browser_view reference to the window delegate.
      simple_window_delegate_t* window_delegate = window_delegate_create(
          browser_view, NULL, runtime_style, initial_show_state);
      CHECK(window_delegate);

      // Create the window.
      // We transfer our window_delegate reference to CEF.
      cef_window_create_top_level(&window_delegate->delegate);
    }
  } else {
#if defined(OS_WIN)
    // Register window class
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = LiteBrowserMainWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"LiteBrowserMainWindowClass";
    RegisterClassEx(&wcex);

    // Create main window
    g_main_hwnd = CreateWindowEx(
        0, L"LiteBrowserMainWindowClass", L"Lite Browser",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (g_main_hwnd) {
      RECT rect;
      GetClientRect(g_main_hwnd, &rect);
      int width = rect.right;
      int height = rect.bottom;

      // 1. Create UI child browser
      cef_window_info_t ui_window_info = {};
      ui_window_info.size = sizeof(cef_window_info_t);
      ui_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      ui_window_info.parent_window = g_main_hwnd;
      ui_window_info.bounds.x = 0;
      ui_window_info.bounds.y = 0;
      ui_window_info.bounds.width = width;
      ui_window_info.bounds.height = 80;
      ui_window_info.runtime_style = runtime_style;

      // Resolve UI path to file URL
      cef_string_t ui_url = {};
      ResolveUIPath(&ui_url);

      cef_browser_host_create_browser(&ui_window_info, &client_handler->client, &ui_url,
                                      &browser_settings, NULL, NULL);
      cef_string_clear(&ui_url);

      // 2. Create Content child browser
      cef_window_info_t content_window_info = {};
      content_window_info.size = sizeof(cef_window_info_t);
      content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      content_window_info.parent_window = g_main_hwnd;
      content_window_info.bounds.x = 0;
      content_window_info.bounds.y = 80;
      content_window_info.bounds.width = width;
      content_window_info.bounds.height = height - 80;
      content_window_info.runtime_style = runtime_style;

      cef_string_t content_url = {};
      cef_string_from_ascii(g_startup_url, strlen(g_startup_url), &content_url);

      simple_handler_t* content_client_handler = simple_handler_create(use_alloy_style);

      cef_browser_host_create_browser(&content_window_info, &content_client_handler->client, &content_url,
                                      &browser_settings, NULL, NULL);
      cef_string_clear(&content_url);
    }
#else
    // Information used when creating the native window.
    cef_window_info_t window_info = {};
    window_info.size = sizeof(cef_window_info_t);
    window_info.bounds.width = 800;
    window_info.bounds.height = 600;
    window_info.runtime_style = runtime_style;

    cef_browser_host_create_browser(&window_info, &client_handler->client, &url,
                                    &browser_settings, NULL, NULL);
#endif
  }

  cef_string_clear(&url);
  // Note: We DON'T release the client_handler here.
  // We transferred our creation reference to CEF via
  // cef_browser_host_create_browser. CEF will release it when the browser
  // closes.
}

// Returns the default client handler for Chrome style UI.
cef_client_t* CEF_CALLBACK browser_process_handler_get_default_client(
    cef_browser_process_handler_t* self) {
  // Return the global instance (matches C++ SimpleApp::GetDefaultClient).
  simple_handler_t* instance = simple_handler_get_instance();
  if (instance) {
    // Add reference before returning (CEF will release it).
    instance->client.base.add_ref(&instance->client.base);
    return &instance->client;
  }

  return NULL;
}

// Creates a browser process handler instance.
simple_browser_process_handler_t* browser_process_handler_create(void) {
  simple_browser_process_handler_t* handler =
      (simple_browser_process_handler_t*)calloc(
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
simple_app_t* simple_app_create(void) {
  simple_app_t* app = (simple_app_t*)calloc(1, sizeof(simple_app_t));
  CHECK(app);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&app->app.base, cef_app_t, simple_app);

  // Set callbacks.
  app->app.get_browser_process_handler = simple_app_get_browser_process_handler;

  // Create the browser process handler.
  app->browser_process_handler = browser_process_handler_create();
  CHECK(app->browser_process_handler);

  // Initialize with ref count of 1.
  atomic_store(&app->ref_count, 1);

  return app;
}
