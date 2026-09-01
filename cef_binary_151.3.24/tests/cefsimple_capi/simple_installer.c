#include "tests/cefsimple_capi/simple_installer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Signature for the RunInstaller export from bootstrap.exe
typedef const char* (*RunInstallerFunc)(const char* command, const char* config_json);

typedef struct {
  browser_window_t* win_ctx;
} update_thread_param_t;

void simple_installer_report_launch_success(void) {
  HMODULE hBootstrap = GetModuleHandle(NULL);
  if (!hBootstrap) return;

  RunInstallerFunc run_installer = (RunInstallerFunc)GetProcAddress(hBootstrap, "RunInstaller");
  if (run_installer) {
    const char* res = run_installer("launch_success", NULL);
    if (res) {
      OutputDebugStringA("[LiteBrowser] RunInstaller(launch_success): ");
      OutputDebugStringA(res);
      OutputDebugStringA("\n");
    }
  }
}

static DWORD WINAPI UpdateWorkerThread(LPVOID lpParam) {
  update_thread_param_t* param = (update_thread_param_t*)lpParam;
  browser_window_t* win_ctx = param ? param->win_ctx : NULL;
  free(param);

  HMODULE hBootstrap = GetModuleHandle(NULL);
  if (!hBootstrap) return 0;

  RunInstallerFunc run_installer = (RunInstallerFunc)GetProcAddress(hBootstrap, "RunInstaller");
  if (!run_installer) {
    if (win_ctx && win_ctx->ui_browser) {
      cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
      if (frame) {
        cef_string_t js_code = {};
        const wchar_t* js = L"if(window.showToast){ window.showToast('CEF Installer 라이브러리를 찾을 수 없습니다.', 'warning'); }";
        cef_string_utf16_set(js, wcslen(js), &js_code, 1);
        frame->execute_java_script(frame, &js_code, NULL, 0);
        cef_string_utf16_clear(&js_code);
        frame->base.release(&frame->base);
      }
    }
    return 0;
  }

  // 1. Notify UI: check started
  if (win_ctx && win_ctx->ui_browser) {
    cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
    if (frame) {
      cef_string_t js_code = {};
      const wchar_t* js = L"if(window.showToast){ window.showToast('CEF 런타임 최신 버전 확인 및 CDN 수신 중...', 'info'); }";
      cef_string_utf16_set(js, wcslen(js), &js_code, 1);
      frame->execute_java_script(frame, &js_code, NULL, 0);
      cef_string_utf16_clear(&js_code);
      frame->base.release(&frame->base);
    }
  }

  // 2. Prepare headless silent configuration JSON with show_progress_ui: false
  const char* config_json =
      "{\n"
      "  \"appid\": \"F83A2E79-4B51-41C2-8B1C-9D72A6E9E4F0\",\n"
      "  \"vmin\": \"151.1\",\n"
      "  \"abi_hash\": \"1671cc913eeb4ecf\",\n"
      "  \"launch_health\": \"explicit\",\n"
      "  \"show_progress_ui\": false,\n"
      "  \"force_check\": true\n"
      "}";

  OutputDebugStringA("[LiteBrowser] Calling RunInstaller(update) with show_progress_ui=false...\n");

  // 3. Perform silent background update from CDN via RunInstaller API
  const char* result_json = run_installer("update", config_json);

  if (result_json) {
    OutputDebugStringA("[LiteBrowser] RunInstaller(update) result: ");
    OutputDebugStringA(result_json);
    OutputDebugStringA("\n");
  } else {
    OutputDebugStringA("[LiteBrowser] RunInstaller(update) returned NULL\n");
  }

  // 4. Notify UI with outcome
  if (win_ctx && win_ctx->ui_browser) {
    cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
    if (frame) {
      wchar_t js_buf[512] = {};
      
      int is_success = (result_json != NULL) &&
                       (strstr(result_json, "\"success\": true") || strstr(result_json, "\"success\":true"));
      int is_committed = (result_json != NULL) &&
                         (strstr(result_json, "\"outcome\": \"committed\"") || strstr(result_json, "\"outcome\":\"committed\""));

      if (is_success && is_committed) {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('최신 CEF 런타임 수신 완료 (다음 브라우저 실행 시 자동 적용)', 'success'); }");
      } else if (is_success) {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('이미 최신 CEF 런타임을 사용 중입니다.', 'success'); }");
      } else if (result_json && (strstr(result_json, "NO_MATCHING_VERSION") || strstr(result_json, "103"))) {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('이미 최신 CEF 런타임을 사용 중입니다.', 'success'); }");
      } else {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('CEF 런타임 확인 완료 (현재 버전 유지)', 'info'); }");
      }

      cef_string_t js_code = {};
      cef_string_utf16_set(js_buf, wcslen(js_buf), &js_code, 1);
      frame->execute_java_script(frame, &js_code, NULL, 0);
      cef_string_utf16_clear(&js_code);
      frame->base.release(&frame->base);
    }
  }

  return 0;
}

void simple_installer_check_update_async(browser_window_t* win_ctx) {
  update_thread_param_t* param = (update_thread_param_t*)malloc(sizeof(update_thread_param_t));
  if (!param) return;
  param->win_ctx = win_ctx;

  HANDLE hThread = CreateThread(NULL, 0, UpdateWorkerThread, param, 0, NULL);
  if (hThread) {
    CloseHandle(hThread);
  } else {
    free(param);
  }
}
