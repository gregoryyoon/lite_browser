// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include "include/capi/cef_app_capi.h"
#include "include/cef_api_hash.h"
#include "include/cef_sandbox_win.h"
#include "tests/cefsimple_capi/simple_app.h"
#include "tests/cefsimple_capi/simple_utils.h"

static int RunMain(HINSTANCE hInstance,
                   LPTSTR lpCmdLine,
                   int nCmdShow,
                   void* sandbox_info) {
  int exit_code;

  // Configure the CEF API version. This must be called before any other CEF
  // API functions.
  cef_api_hash(CEF_API_VERSION, 0);

  // Provide CEF with command-line arguments.
  cef_main_args_t main_args = {};
  main_args.instance = hInstance;

  // Create the application instance (with 1 reference).
  simple_app_t* app = simple_app_create();
  CHECK(app);

  // Add reference before cef_execute_process. Both cef_execute_process and
  // cef_initialize will take ownership of a reference, so we need 2 total.
  app->app.base.add_ref(&app->app.base);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  exit_code = cef_execute_process(&main_args, &app->app, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    // cef_execute_process took ownership of one reference.
    // Release only the additional reference we added.
    app->app.base.release(&app->app.base);
    return exit_code;
  }

  // Specify CEF global settings here.
  cef_settings_t settings = {};
  settings.size = sizeof(cef_settings_t);

  if (!sandbox_info) {
    settings.no_sandbox = 1;
  }

  // Set dedicated root_cache_path to prevent process singleton lock collisions
  wchar_t local_app_data[MAX_PATH];
  if (GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH) > 0) {
    wchar_t root_cache[MAX_PATH];
    swprintf(root_cache, MAX_PATH, L"%s\\LiteBrowser\\User Data", local_app_data);
    cef_string_from_wide(root_cache, wcslen(root_cache), &settings.root_cache_path);
  }

  // Automatically detect Windows OS default UI language and configure CEF locale / accept_language_list
  wchar_t wlocale[LOCALE_NAME_MAX_LENGTH] = {0};
  char locale_utf8[64] = "ko-KR";
  if (GetUserDefaultLocaleName(wlocale, LOCALE_NAME_MAX_LENGTH) > 0) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wlocale, -1, locale_utf8, sizeof(locale_utf8), NULL, NULL);
    if (len <= 0) {
      strcpy(locale_utf8, "ko-KR");
    }
  }

  // 1. CEF internal UI language (Password Manager, Context menus, DevTools, Alert dialogs, etc.)
  cef_string_from_utf8(locale_utf8, strlen(locale_utf8), &settings.locale);

  // 2. HTTP Accept-Language header & navigator.language / navigator.languages JS attributes
  char accept_languages[128] = {0};
  char lang_prefix[16] = {0};
  const char* hyphen = strchr(locale_utf8, '-');
  if (hyphen && (size_t)(hyphen - locale_utf8) < sizeof(lang_prefix)) {
    strncpy(lang_prefix, locale_utf8, hyphen - locale_utf8);
    lang_prefix[hyphen - locale_utf8] = '\0';
  } else {
    strncpy(lang_prefix, locale_utf8, sizeof(lang_prefix) - 1);
    lang_prefix[sizeof(lang_prefix) - 1] = '\0';
  }

  if (_stricmp(lang_prefix, "en") == 0) {
    snprintf(accept_languages, sizeof(accept_languages), "%s,en", locale_utf8);
  } else {
    snprintf(accept_languages, sizeof(accept_languages), "%s,%s,en-US,en", locale_utf8, lang_prefix);
  }
  cef_string_from_utf8(accept_languages, strlen(accept_languages), &settings.accept_language_list);

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  BOOL init_success = cef_initialize(&main_args, &settings, &app->app, sandbox_info);

  // Clear CEF strings allocated for settings
  cef_string_clear(&settings.locale);
  cef_string_clear(&settings.accept_language_list);
  cef_string_clear(&settings.root_cache_path);

  if (!init_success) {
    // cef_initialize took ownership of the remaining app reference so we don't
    // need to release any.
    return cef_get_exit_code();
  }

  // Run the CEF message loop. This will block until cef_quit_message_loop() is
  // called.
  cef_run_message_loop();

  // Shut down CEF.
  cef_shutdown();

  // Note: We DON'T release the app here. The 2 total references have been
  // given to cef_execute_process and cef_initialize. If cef_initialize
  // succeeded the final reference will be released during cef_shutdown.

  return 0;
}

#if defined(CEF_USE_BOOTSTRAP)

// Entry point called by bootstrap.exe when built as a DLL.
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE hInstance,
                                    LPTSTR lpCmdLine,
                                    int nCmdShow,
                                    void* sandbox_info,
                                    cef_version_info_t* version_info) {
  (void)version_info;  // Unused parameter
  return RunMain(hInstance, lpCmdLine, nCmdShow, sandbox_info);
}

#else  // !defined(CEF_USE_BOOTSTRAP)

// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
  int exit_code = cef_run_winmain_with_preferred_stack_size(
      wWinMain, hInstance, lpCmdLine, nCmdShow);
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  void* sandbox_info = NULL;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  sandbox_info = cef_sandbox_info_create();
#endif

  int result = RunMain(hInstance, lpCmdLine, nCmdShow, sandbox_info);

#if defined(CEF_USE_SANDBOX)
  if (sandbox_info) {
    cef_sandbox_info_destroy(sandbox_info);
  }
#endif

  return result;
}

#endif  // !defined(CEF_USE_BOOTSTRAP)
