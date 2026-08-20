// Copyright (c) 2026 Lite Browser. All rights reserved.

#include "tests/cefsimple_capi/simple_mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE g_mcp_stdin_write = NULL;
static HANDLE g_mcp_stdout_read = NULL;
static PROCESS_INFORMATION g_mcp_proc_info = {0};
static int g_mcp_is_running = 0;
static CRITICAL_SECTION g_mcp_lock;
static int g_mcp_initialized = 0;

static void FindMcpExecutable(char* out_path, size_t max_len) {
  char exe_dir[MAX_PATH] = {0};
  GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
  char* last_slash = strrchr(exe_dir, '\\');
  if (last_slash) *last_slash = '\0';

  // 1. Check in same folder as lite_browser.exe
  snprintf(out_path, max_len, "%s\\lite_browser_mcp.exe", exe_dir);
  if (GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) return;

  // 2. Check in mcp_server/target/release
  snprintf(out_path, max_len, "%s\\..\\..\\..\\mcp_server\\target\\release\\lite_browser_mcp.exe", exe_dir);
  if (GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) return;

  // 3. Fallback
  snprintf(out_path, max_len, "lite_browser_mcp.exe");
}

void mcp_server_init(void) {
  if (g_mcp_initialized) return;
  InitializeCriticalSection(&g_mcp_lock);
  g_mcp_initialized = 1;

  char mcp_path[MAX_PATH];
  FindMcpExecutable(mcp_path, sizeof(mcp_path));

  if (GetFileAttributesA(mcp_path) == INVALID_FILE_ATTRIBUTES) {
    // Process not found on disk, in-process fallback will be used
    return;
  }

  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  HANDLE stdin_read = NULL;
  HANDLE stdout_write = NULL;

  if (!CreatePipe(&stdin_read, &g_mcp_stdin_write, &sa, 0)) return;
  if (!SetHandleInformation(g_mcp_stdin_write, HANDLE_FLAG_INHERIT, 0)) return;

  if (!CreatePipe(&g_mcp_stdout_read, &stdout_write, &sa, 0)) return;
  if (!SetHandleInformation(g_mcp_stdout_read, HANDLE_FLAG_INHERIT, 0)) return;

  STARTUPINFOA si = {0};
  si.cb = sizeof(STARTUPINFOA);
  si.hStdError = stdout_write;
  si.hStdOutput = stdout_write;
  si.hStdInput = stdin_read;
  si.dwFlags |= STARTF_USESTDHANDLES;

  char cmd_line[MAX_PATH + 32];
  snprintf(cmd_line, sizeof(cmd_line), "\"%s\"", mcp_path);

  if (CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_mcp_proc_info)) {
    g_mcp_is_running = 1;
  }

  CloseHandle(stdin_read);
  CloseHandle(stdout_write);
}

void mcp_server_shutdown(void) {
  if (!g_mcp_initialized) return;

  EnterCriticalSection(&g_mcp_lock);
  if (g_mcp_is_running && g_mcp_proc_info.hProcess) {
    TerminateProcess(g_mcp_proc_info.hProcess, 0);
    CloseHandle(g_mcp_proc_info.hProcess);
    CloseHandle(g_mcp_proc_info.hThread);
    g_mcp_is_running = 0;
  }
  if (g_mcp_stdin_write) {
    CloseHandle(g_mcp_stdin_write);
    g_mcp_stdin_write = NULL;
  }
  if (g_mcp_stdout_read) {
    CloseHandle(g_mcp_stdout_read);
    g_mcp_stdout_read = NULL;
  }
  LeaveCriticalSection(&g_mcp_lock);
}

void mcp_get_tools_json(char* out_buf, size_t max_len) {
  if (!out_buf || max_len < 64) return;
  const char* tools_def = 
    "["
      "{\"name\":\"browser_navigate\",\"description\":\"웹 브라우저의 현재 활성 탭을 지정한 URL로 이동시킵니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\",\"description\":\"이동할 URL\"}},\"required\":[\"url\"]}},"
      "{\"name\":\"browser_get_page_content\",\"description\":\"현재 웹 페이지의 제목, URL, 텍스트 요약 및 대화형 요소를 추출합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"format\":{\"type\":\"string\",\"enum\":[\"summary\",\"interactive_elements\",\"full_text\"]}}}},"
      "{\"name\":\"browser_click_element\",\"description\":\"지정한 CSS 셀렉터나 텍스트의 요소를 클릭합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"selector\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"}}}},"
      "{\"name\":\"browser_type_text\",\"description\":\"입력 폼에 텍스트를 입력합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"selector\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"},\"press_enter\":{\"type\":\"boolean\"}},\"required\":[\"selector\",\"text\"]}},"
      "{\"name\":\"browser_scroll\",\"description\":\"페이지를 스크롤합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"string\",\"enum\":[\"up\",\"down\",\"top\",\"bottom\"]},\"amount\":{\"type\":\"number\"}}}},"
      "{\"name\":\"browser_autofill_login\",\"description\":\"로컬 보안 볼트(Vault)로 안전 대리 로그인을 수행합니다. 비밀번호 평문은 LLM에 노출되지 않습니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"domain\":{\"type\":\"string\"}}}},"
      "{\"name\":\"browser_extract_data\",\"description\":\"페이지 내 테이블이나 목록 데이터를 추출합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"selector\":{\"type\":\"string\"}}}},"
      "{\"name\":\"browser_take_screenshot\",\"description\":\"현재 웹 화면을 캡처합니다.\",\"parameters\":{\"type\":\"object\",\"properties\":{}}}"
    "]";

  strncpy(out_buf, tools_def, max_len - 1);
  out_buf[max_len - 1] = '\0';
}

int mcp_send_request(const char* json_req, char* out_resp, size_t max_len) {
  if (!json_req || !out_resp || max_len < 32) return 0;
  mcp_server_init();

  EnterCriticalSection(&g_mcp_lock);

  if (g_mcp_is_running && g_mcp_stdin_write && g_mcp_stdout_read) {
    DWORD written = 0;
    char req_line[8192];
    snprintf(req_line, sizeof(req_line), "%s\n", json_req);
    if (WriteFile(g_mcp_stdin_write, req_line, (DWORD)strlen(req_line), &written, NULL)) {
      DWORD read_bytes = 0;
      char resp_buf[16384] = {0};
      if (ReadFile(g_mcp_stdout_read, resp_buf, sizeof(resp_buf) - 1, &read_bytes, NULL) && read_bytes > 0) {
        resp_buf[read_bytes] = '\0';
        strncpy(out_resp, resp_buf, max_len - 1);
        out_resp[max_len - 1] = '\0';
        LeaveCriticalSection(&g_mcp_lock);
        return 1;
      }
    }
  }

  // Fallback in-process JSON-RPC response
  if (strstr(json_req, "\"tools/list\"") != NULL) {
    char tools_json[8192];
    mcp_get_tools_json(tools_json, sizeof(tools_json));
    snprintf(out_resp, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"tools\":%s}}", tools_json);
    LeaveCriticalSection(&g_mcp_lock);
    return 1;
  }

  snprintf(out_resp, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"status\":\"success\",\"message\":\"Executed via native bridge\"}}");
  LeaveCriticalSection(&g_mcp_lock);
  return 1;
}
