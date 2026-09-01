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
    (void)res;
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

  // 1. Notify UI: update check started
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

  // 2. Perform background update from CDN via RunInstaller API
  const char* result_json = run_installer("update", NULL);

  // 3. Notify UI with result
  if (win_ctx && win_ctx->ui_browser) {
    cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
    if (frame) {
      wchar_t js_buf[512] = {};
      if (result_json && (strstr(result_json, "success") || strstr(result_json, "installed") || strstr(result_json, "updated"))) {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('최신 CEF 런타임 수신 완료 (다음 브라우저 실행 시 자동 적용)', 'success'); }");
      } else if (result_json && (strstr(result_json, "current") || strstr(result_json, "no_update") || strstr(result_json, "up_to_date"))) {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('이미 최신 CEF 런타임을 사용 중입니다.', 'success'); }");
      } else {
        swprintf_s(js_buf, 512, L"if(window.showToast){ window.showToast('CEF 최신 버전 확인 완료 (현재 런타임 유지)', 'info'); }");
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
