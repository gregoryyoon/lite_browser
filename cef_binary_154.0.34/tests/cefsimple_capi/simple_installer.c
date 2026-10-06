#include "tests/cefsimple_capi/simple_installer.h"
#include "include/capi/cef_task_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

// Signature for the RunInstaller export from bootstrap.exe
typedef const char* (*RunInstallerFunc)(const char* command, const char* config_json);

typedef struct {
  cef_browser_t* ui_browser;
} update_thread_param_t;

typedef struct {
  cef_task_t task;
  atomic_int ref_count;
  cef_browser_t* browser;
  wchar_t message[256];
  wchar_t type[32];
} show_toast_task_t;

static void CEF_CALLBACK show_toast_task_add_ref(cef_base_ref_counted_t* self) {
  show_toast_task_t* task = (show_toast_task_t*)self;
  atomic_fetch_add(&task->ref_count, 1);
}

static int CEF_CALLBACK show_toast_task_release(cef_base_ref_counted_t* self) {
  show_toast_task_t* task = (show_toast_task_t*)self;
  if (atomic_fetch_sub(&task->ref_count, 1) == 1) {
    if (task->browser) {
      task->browser->base.release(&task->browser->base);
    }
    free(task);
    return 1;
  }
  return 0;
}

static int CEF_CALLBACK show_toast_task_has_one_ref(cef_base_ref_counted_t* self) {
  show_toast_task_t* task = (show_toast_task_t*)self;
  return atomic_load(&task->ref_count) == 1;
}

static int CEF_CALLBACK show_toast_task_has_at_least_one_ref(cef_base_ref_counted_t* self) {
  show_toast_task_t* task = (show_toast_task_t*)self;
  return atomic_load(&task->ref_count) >= 1;
}

static void CEF_CALLBACK show_toast_task_execute(cef_task_t* self) {
  show_toast_task_t* task = (show_toast_task_t*)self;
  if (!task->browser) return;

  cef_frame_t* frame = task->browser->get_main_frame(task->browser);
  if (frame) {
    wchar_t js[1024];
    swprintf_s(js, 1024, L"if(window.showToast){ window.showToast('%ls', '%ls'); }", task->message, task->type);
    cef_string_t js_code = {};
    cef_string_utf16_set(js, wcslen(js), &js_code, 1);
    frame->execute_java_script(frame, &js_code, NULL, 0);
    cef_string_utf16_clear(&js_code);
    frame->base.release(&frame->base);
  }
}

static void PostToast(cef_browser_t* ui_browser, const wchar_t* msg, const wchar_t* type) {
  if (!ui_browser) return;

  show_toast_task_t* task = (show_toast_task_t*)calloc(1, sizeof(show_toast_task_t));
  if (!task) return;

  task->task.base.size = sizeof(cef_task_t);
  task->task.base.add_ref = show_toast_task_add_ref;
  task->task.base.release = show_toast_task_release;
  task->task.base.has_one_ref = show_toast_task_has_one_ref;
  task->task.base.has_at_least_one_ref = show_toast_task_has_at_least_one_ref;
  task->task.execute = show_toast_task_execute;
  atomic_store(&task->ref_count, 1);

  task->browser = ui_browser;
  ui_browser->base.add_ref(&ui_browser->base);

  wcsncpy_s(task->message, 256, msg, _TRUNCATE);
  wcsncpy_s(task->type, 32, type, _TRUNCATE);

  if (cef_currently_on(TID_UI)) {
    show_toast_task_execute(&task->task);
    show_toast_task_release(&task->task.base);
  } else {
    cef_post_task(TID_UI, &task->task);
  }
}

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
  cef_browser_t* ui_browser = param ? param->ui_browser : NULL;
  free(param);

  if (!ui_browser) return 0;

  // 1. Initial Toast via UI Thread Task
  PostToast(ui_browser, L"CEF 런타임 최신 버전 확인 및 CDN 수신 중...", L"info");

  HMODULE hBootstrap = GetModuleHandle(NULL);
  RunInstallerFunc run_installer = hBootstrap ? (RunInstallerFunc)GetProcAddress(hBootstrap, "RunInstaller") : NULL;

  if (!run_installer) {
    PostToast(ui_browser, L"CEF Installer 라이브러리를 찾을 수 없습니다.", L"warning");
    ui_browser->base.release(&ui_browser->base);
    return 0;
  }

  // 2. Headless Configuration
  const char* config_json =
      "{\n"
      "  \"appid\": \"F83A2E79-4B51-41C2-8B1C-9D72A6E9E4F0\",\n"
      "  \"vmin\": \"151.1\",\n"
      "  \"abi_hash\": \"1671cc913eeb4ecf\",\n"
      "  \"launch_health\": \"explicit\",\n"
      "  \"show_progress_ui\": false,\n"
      "  \"force_check\": true\n"
      "}";

  OutputDebugStringA("[LiteBrowser] Calling RunInstaller(update)...\n");

  // 3. Call RunInstaller API
  const char* result_json = run_installer("update", config_json);

  if (result_json) {
    OutputDebugStringA("[LiteBrowser] RunInstaller result: ");
    OutputDebugStringA(result_json);
    OutputDebugStringA("\n");
  } else {
    OutputDebugStringA("[LiteBrowser] RunInstaller returned NULL\n");
  }

  // 4. Parse outcome and show completion toast via UI Thread Task
  if (result_json && (strstr(result_json, "\"outcome\": \"committed\"") || strstr(result_json, "\"outcome\":\"committed\""))) {
    PostToast(ui_browser, L"최신 CEF 런타임 수신 완료 (다음 브라우저 실행 시 자동 적용)", L"success");
  } else if (result_json && (strstr(result_json, "\"success\": true") || strstr(result_json, "\"success\":true"))) {
    PostToast(ui_browser, L"이미 최신 CEF 런타임을 사용 중입니다.", L"success");
  } else if (result_json && (strstr(result_json, "NO_MATCHING_VERSION") || strstr(result_json, "103") || strstr(result_json, "current") || strstr(result_json, "up_to_date"))) {
    PostToast(ui_browser, L"이미 최신 CEF 런타임을 사용 중입니다.", L"success");
  } else {
    PostToast(ui_browser, L"CEF 런타임 확인 완료 (현재 버전 유지)", L"info");
  }

  ui_browser->base.release(&ui_browser->base);
  return 0;
}

void simple_installer_check_update_async(browser_window_t* win_ctx) {
  if (!win_ctx || !win_ctx->ui_browser) return;

  update_thread_param_t* param = (update_thread_param_t*)malloc(sizeof(update_thread_param_t));
  if (!param) return;

  param->ui_browser = win_ctx->ui_browser;
  param->ui_browser->base.add_ref(&param->ui_browser->base);

  HANDLE hThread = CreateThread(NULL, 0, UpdateWorkerThread, param, 0, NULL);
  if (hThread) {
    CloseHandle(hThread);
  } else {
    param->ui_browser->base.release(&param->ui_browser->base);
    free(param);
  }
}
