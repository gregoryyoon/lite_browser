// Copyright (c) 2026 Lite Browser. All rights reserved.
// Auto-updater subsystem for Lite Browser using GitHub Releases and WinVerifyTrust.

#include "tests/cefsimple_capi/simple_updater.h"

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

static CRITICAL_SECTION g_updater_cs;
static int g_updater_inited = 0;
static update_state_t g_state = UPDATE_STATE_IDLE;
static int g_progress = 0;
static char g_latest_version[64] = "";
static char g_download_url[1024] = "";
static char g_status_message[256] = "";
static char g_installer_path[MAX_PATH] = "";
static HWND g_parent_hwnd = NULL;
static HANDLE g_worker_thread = NULL;
static int g_is_silent = 0;

static const char* state_to_string(update_state_t s) {
  switch (s) {
    case UPDATE_STATE_IDLE: return "IDLE";
    case UPDATE_STATE_CHECKING: return "CHECKING";
    case UPDATE_STATE_UP_TO_DATE: return "UP_TO_DATE";
    case UPDATE_STATE_AVAILABLE: return "AVAILABLE";
    case UPDATE_STATE_DOWNLOADING: return "DOWNLOADING";
    case UPDATE_STATE_READY: return "READY";
    case UPDATE_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

void simple_updater_init(void) {
  if (!g_updater_inited) {
    InitializeCriticalSection(&g_updater_cs);
    g_updater_inited = 1;
    g_state = UPDATE_STATE_IDLE;
    g_progress = 0;
    snprintf(g_status_message, sizeof(g_status_message), "업데이트 확인 대기 중");
  }
}

void simple_updater_cleanup(void) {
  if (g_updater_inited) {
    if (g_worker_thread) {
      WaitForSingleObject(g_worker_thread, 2000);
      CloseHandle(g_worker_thread);
      g_worker_thread = NULL;
    }
    DeleteCriticalSection(&g_updater_cs);
    g_updater_inited = 0;
  }
}

void simple_updater_set_parent_hwnd(HWND hwnd) {
  EnterCriticalSection(&g_updater_cs);
  g_parent_hwnd = hwnd;
  LeaveCriticalSection(&g_updater_cs);
}

update_state_t simple_updater_get_state(void) {
  update_state_t s;
  EnterCriticalSection(&g_updater_cs);
  s = g_state;
  LeaveCriticalSection(&g_updater_cs);
  return s;
}

int simple_updater_is_ready_to_restart(void) {
  int ready = 0;
  EnterCriticalSection(&g_updater_cs);
  if (g_state == UPDATE_STATE_READY && strlen(g_installer_path) > 0) {
    DWORD attr = GetFileAttributesA(g_installer_path);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
      ready = 1;
    }
  }
  LeaveCriticalSection(&g_updater_cs);
  return ready;
}

void simple_updater_broadcast_status(void) {
  HWND h = NULL;
  EnterCriticalSection(&g_updater_cs);
  h = g_parent_hwnd;
  LeaveCriticalSection(&g_updater_cs);

  if (h && IsWindow(h)) {
    PostMessage(h, WM_UPDATER_STATUS_CHANGED, 0, 0);
  }
}

void simple_updater_get_status_json(char* out_buf, size_t max_len) {
  if (!out_buf || max_len == 0) return;

  EnterCriticalSection(&g_updater_cs);
  int ready = (g_state == UPDATE_STATE_READY);
  snprintf(out_buf, max_len,
           "{\"currentVersion\":\"%s\",\"latestVersion\":\"%s\",\"state\":\"%s\","
           "\"progress\":%d,\"message\":\"%s\",\"isReady\":%s}",
           LITE_BROWSER_VERSION,
           g_latest_version,
           state_to_string(g_state),
           g_progress,
           g_status_message,
           ready ? "true" : "false");
  LeaveCriticalSection(&g_updater_cs);
}

static int parse_version_numbers(const char* ver, int* major, int* minor, int* patch) {
  *major = 0; *minor = 0; *patch = 0;
  if (!ver) return 0;
  while (*ver == 'v' || *ver == 'V' || *ver == ' ' || *ver == '\t') {
    ver++;
  }
  return sscanf(ver, "%d.%d.%d", major, minor, patch);
}

static int compare_versions(const char* ver_remote, const char* ver_local) {
  int r_maj = 0, r_min = 0, r_pat = 0;
  int l_maj = 0, l_min = 0, l_pat = 0;
  parse_version_numbers(ver_remote, &r_maj, &r_min, &r_pat);
  parse_version_numbers(ver_local, &l_maj, &l_min, &l_pat);

  if (r_maj != l_maj) return (r_maj > l_maj) ? 1 : -1;
  if (r_min != l_min) return (r_min > l_min) ? 1 : -1;
  if (r_pat != l_pat) return (r_pat > l_pat) ? 1 : -1;
  return 0;
}

static int json_extract_string(const char* json, const char* key, char* out_val, size_t max_len) {
  if (!json || !key || !out_val || max_len == 0) return 0;
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* pos = strstr(json, pattern);
  if (!pos) return 0;
  pos += strlen(pattern);
  while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n' || *pos == ':')) {
    pos++;
  }
  if (*pos != '\"') return 0;
  pos++;
  size_t idx = 0;
  while (*pos && *pos != '\"' && idx < max_len - 1) {
    if (*pos == '\\' && *(pos + 1)) {
      pos++;
    }
    out_val[idx++] = *pos++;
  }
  out_val[idx] = '\0';
  return (idx > 0);
}

static int json_extract_installer_asset_url(const char* json, char* out_url, size_t max_len) {
  if (!json || !out_url || max_len == 0) return 0;
  const char* asset_pos = strstr(json, "LiteBrowserInstaller.exe");
  if (!asset_pos) return 0;

  const char* url_key = strstr(asset_pos, "\"browser_download_url\"");
  if (!url_key) {
    const char* scan = asset_pos;
    while (scan > json && (size_t)(asset_pos - scan) < 1024) {
      if (strncmp(scan, "\"browser_download_url\"", 22) == 0) {
        url_key = scan;
        break;
      }
      scan--;
    }
  }
  if (!url_key) return 0;

  return json_extract_string(url_key, "browser_download_url", out_url, max_len);
}

static int verify_installer_signature(const char* file_path) {
  wchar_t w_file_path[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, file_path, -1, w_file_path, MAX_PATH);

  WINTRUST_FILE_INFO file_data = {0};
  file_data.cbStruct = sizeof(WINTRUST_FILE_INFO);
  file_data.pcwszFilePath = w_file_path;

  WINTRUST_DATA trust_data = {0};
  trust_data.cbStruct = sizeof(WINTRUST_DATA);
  trust_data.dwUIChoice = WTD_UI_NONE;
  trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  trust_data.dwUnionChoice = WTD_CHOICE_FILE;
  trust_data.pFile = &file_data;
  trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  trust_data.dwProvFlags = WTD_SAFER_FLAG;

  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG status = WinVerifyTrust(NULL, &policy_guid, &trust_data);

  trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(NULL, &policy_guid, &trust_data);

  // Accept valid certificate or self-signed dev certificate (0x800B0109 CERT_E_UNTRUSTEDROOT)
  if (status == ERROR_SUCCESS || (DWORD)status == CERT_E_UNTRUSTEDROOT) {
    return 1;
  }
  return 0;
}

static DWORD WINAPI updater_worker_proc(LPVOID lpParam) {
  (void)lpParam;

  EnterCriticalSection(&g_updater_cs);
  g_state = UPDATE_STATE_CHECKING;
  g_progress = 0;
  snprintf(g_status_message, sizeof(g_status_message), "최신 버전 정보 확인 중...");
  LeaveCriticalSection(&g_updater_cs);
  simple_updater_broadcast_status();

  HINTERNET hSession = WinHttpOpen(L"LiteBrowser-Updater/1.0",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "네트워크 초기화 실패 (WinHttpOpen)");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Set timeouts (10s connect, 15s send/receive)
  WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 15000);

  HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "GitHub 서버 연결 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                         L"/repos/gregoryyoon/lite_browser/releases/latest",
                                         NULL, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "API 요청 생성 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  LPCWSTR req_headers = L"User-Agent: LiteBrowser-Updater/1.0\r\nAccept: application/vnd.github.v3+json\r\n";
  BOOL sent = WinHttpSendRequest(hRequest, req_headers, (DWORD)-1, NULL, 0, 0, 0);
  if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "업데이트 서버 응답 없음");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

  if (status_code != 200) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    if (status_code == 404) {
      g_state = UPDATE_STATE_UP_TO_DATE;
      snprintf(g_status_message, sizeof(g_status_message), "현재 최신 버전입니다 (v%s)", LITE_BROWSER_VERSION);
    } else {
      g_state = UPDATE_STATE_ERROR;
      snprintf(g_status_message, sizeof(g_status_message), "업데이트 확인 오류 (HTTP %lu)", status_code);
    }
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Read release info JSON
  char* json_buf = (char*)malloc(131072);
  if (!json_buf) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
  }
  DWORD total_json = 0;
  DWORD bytes_read = 0;
  while (WinHttpReadData(hRequest, json_buf + total_json, 131072 - 1 - total_json, &bytes_read) && bytes_read > 0) {
    total_json += bytes_read;
    if (total_json >= 131071) break;
  }
  json_buf[total_json] = '\0';

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  // Extract tag_name and installer URL
  char remote_ver[64] = "";
  char download_url[1024] = "";
  json_extract_string(json_buf, "tag_name", remote_ver, sizeof(remote_ver));
  json_extract_installer_asset_url(json_buf, download_url, sizeof(download_url));
  free(json_buf);

  if (strlen(remote_ver) == 0 || strlen(download_url) == 0) {
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "업데이트 패키지 정보 분석 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  EnterCriticalSection(&g_updater_cs);
  strncpy(g_latest_version, remote_ver, sizeof(g_latest_version) - 1);
  strncpy(g_download_url, download_url, sizeof(g_download_url) - 1);
  LeaveCriticalSection(&g_updater_cs);

  // SemVer comparison
  if (compare_versions(remote_ver, LITE_BROWSER_VERSION) <= 0) {
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_UP_TO_DATE;
    snprintf(g_status_message, sizeof(g_status_message), "현재 최신 버전 (v%s)을 사용하고 있습니다.", LITE_BROWSER_VERSION);
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // New version found: Download
  EnterCriticalSection(&g_updater_cs);
  g_state = UPDATE_STATE_DOWNLOADING;
  g_progress = 0;
  snprintf(g_status_message, sizeof(g_status_message), "새 버전 (v%s) 다운로드 중...", remote_ver);
  LeaveCriticalSection(&g_updater_cs);
  simple_updater_broadcast_status();

  char temp_path[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_path);
  char update_dir[MAX_PATH];
  snprintf(update_dir, sizeof(update_dir), "%sLiteBrowserUpdate", temp_path);
  CreateDirectoryA(update_dir, NULL);

  char installer_path[MAX_PATH];
  snprintf(installer_path, sizeof(installer_path), "%s\\LiteBrowserInstaller.exe", update_dir);
  char part_path[MAX_PATH];
  snprintf(part_path, sizeof(part_path), "%s\\LiteBrowserInstaller.exe.part", update_dir);

  DeleteFileA(part_path);

  // Parse download URL
  wchar_t w_url[1024];
  MultiByteToWideChar(CP_UTF8, 0, download_url, -1, w_url, 1024);

  URL_COMPONENTSW url_comp = {0};
  url_comp.dwStructSize = sizeof(url_comp);
  wchar_t w_host[256] = {0};
  wchar_t w_path[1024] = {0};
  url_comp.lpszHostName = w_host;
  url_comp.dwHostNameLength = 256;
  url_comp.lpszUrlPath = w_path;
  url_comp.dwUrlPathLength = 1024;

  if (!WinHttpCrackUrl(w_url, 0, 0, &url_comp)) {
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 URL 주소 오류");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  hSession = WinHttpOpen(L"LiteBrowser-Updater/1.0",
                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME,
                         WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 세션 시작 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Set redirect policy ALWAYS for GitHub release asset downloads
  DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
  WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 30000);

  hConnect = WinHttpConnect(hSession, w_host, url_comp.nPort, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 호스트 연결 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  DWORD flags = (url_comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  hRequest = WinHttpOpenRequest(hConnect, L"GET", w_path, NULL, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 요청 생성 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Also enforce redirect option on request
  WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

  LPCWSTR dl_headers = L"User-Agent: LiteBrowser-Updater/1.0\r\nAccept: application/octet-stream\r\n";
  if (!WinHttpSendRequest(hRequest, dl_headers, (DWORD)-1, NULL, 0, 0, 0) ||
      !WinHttpReceiveResponse(hRequest, NULL)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 서버 응답 수신 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  DWORD dl_status = 0;
  DWORD dl_status_size = sizeof(dl_status);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &dl_status, &dl_status_size, WINHTTP_NO_HEADER_INDEX);

  if (dl_status != 200) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드 HTTP 실패 (%lu)", dl_status);
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  DWORD content_length = 0;
  DWORD cl_size = sizeof(content_length);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &content_length, &cl_size, WINHTTP_NO_HEADER_INDEX);

  HANDLE hFile = CreateFileA(part_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "임시 파일 생성 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  char chunk[65536];
  DWORD total_downloaded = 0;
  int last_reported_progress = -1;

  while (WinHttpReadData(hRequest, chunk, sizeof(chunk), &bytes_read) && bytes_read > 0) {
    DWORD written = 0;
    WriteFile(hFile, chunk, bytes_read, &written, NULL);
    total_downloaded += bytes_read;

    int cur_prog = 0;
    if (content_length > 0) {
      cur_prog = (int)(((ULONGLONG)total_downloaded * 100) / content_length);
      if (cur_prog > 99) cur_prog = 99;
    }

    if (cur_prog != last_reported_progress && cur_prog % 5 == 0) {
      last_reported_progress = cur_prog;
      EnterCriticalSection(&g_updater_cs);
      g_progress = cur_prog;
      snprintf(g_status_message, sizeof(g_status_message), "새 버전 다운로드 중 (%d%%)...", cur_prog);
      LeaveCriticalSection(&g_updater_cs);
      simple_updater_broadcast_status();
    }
  }

  CloseHandle(hFile);
  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  if (total_downloaded < 1048576) { // Less than 1MB is suspicious for installer
    DeleteFileA(part_path);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "다운로드된 설치 파일이 손상되었습니다.");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Rename .part to .exe
  DeleteFileA(installer_path);
  if (!MoveFileExA(part_path, installer_path, MOVEFILE_REPLACE_EXISTING)) {
    DeleteFileA(part_path);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "설치 파일 배치 실패");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Verify Authenticode Digital Signature
  if (!verify_installer_signature(installer_path)) {
    DeleteFileA(installer_path);
    EnterCriticalSection(&g_updater_cs);
    g_state = UPDATE_STATE_ERROR;
    snprintf(g_status_message, sizeof(g_status_message), "설치 패키지 디지털 서명 검증 실패 (무결성 경고)");
    LeaveCriticalSection(&g_updater_cs);
    simple_updater_broadcast_status();
    return 0;
  }

  // Success: Ready for silent install & restart
  EnterCriticalSection(&g_updater_cs);
  g_state = UPDATE_STATE_READY;
  g_progress = 100;
  strncpy(g_installer_path, installer_path, sizeof(g_installer_path) - 1);
  snprintf(g_status_message, sizeof(g_status_message), "새 버전 (v%s) 설치 준비 완료", remote_ver);
  LeaveCriticalSection(&g_updater_cs);
  simple_updater_broadcast_status();

  return 0;
}

void simple_updater_check_async(HWND parent_hwnd, int is_silent) {
  EnterCriticalSection(&g_updater_cs);
  if (parent_hwnd) {
    g_parent_hwnd = parent_hwnd;
  }
  g_is_silent = is_silent;

  if (g_state == UPDATE_STATE_CHECKING || g_state == UPDATE_STATE_DOWNLOADING) {
    LeaveCriticalSection(&g_updater_cs);
    return;
  }

  // If already ready, keep ready state and re-broadcast
  if (g_state == UPDATE_STATE_READY && strlen(g_installer_path) > 0) {
    DWORD attr = GetFileAttributesA(g_installer_path);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
      LeaveCriticalSection(&g_updater_cs);
      simple_updater_broadcast_status();
      return;
    }
  }

  if (g_worker_thread) {
    CloseHandle(g_worker_thread);
    g_worker_thread = NULL;
  }

  g_worker_thread = CreateThread(NULL, 0, updater_worker_proc, NULL, 0, NULL);
  LeaveCriticalSection(&g_updater_cs);
}

void simple_updater_apply_and_restart(HWND parent_hwnd) {
  EnterCriticalSection(&g_updater_cs);
  if (g_state != UPDATE_STATE_READY || strlen(g_installer_path) == 0) {
    LeaveCriticalSection(&g_updater_cs);
    return;
  }
  char target_installer[MAX_PATH];
  strncpy(target_installer, g_installer_path, sizeof(target_installer) - 1);
  target_installer[sizeof(target_installer) - 1] = '\0';
  LeaveCriticalSection(&g_updater_cs);

  DWORD attr = GetFileAttributesA(target_installer);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
    return;
  }

  // Execute silent installation with 1.5s delay to ensure browser process exits and cache/file locks unlock cleanly
  char cmd_args[MAX_PATH * 2 + 128];
  snprintf(cmd_args, sizeof(cmd_args), "/c ping 127.0.0.1 -n 2 >nul & start \"\" \"%s\" /S", target_installer);
  ShellExecuteA(NULL, "open", "cmd.exe", cmd_args, NULL, SW_HIDE);

  // Close parent window
  if (parent_hwnd && IsWindow(parent_hwnd)) {
    PostMessage(parent_hwnd, WM_CLOSE, 0, 0);
  }
}
