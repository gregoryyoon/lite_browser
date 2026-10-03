// Copyright (c) 2026 LiteBrowser Authors. All rights reserved.
// Universal WinHTTP AI Streaming Proxy - bypasses browser CORS & handles SSE.

#include "simple_ai_proxy.h"

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ai_proxy_ctx_s {
  char req_id[64];
  cef_frame_t* frame;
  WCHAR* w_url;
  WCHAR* w_headers;
  char* body;
  size_t body_len;
  HINTERNET hRequest;
  volatile LONG cancelled;
  HANDLE hThread;
  struct ai_proxy_ctx_s* next;
} ai_proxy_ctx_t;

static CRITICAL_SECTION g_proxy_cs;
static ai_proxy_ctx_t* g_proxy_list = NULL;
static LONG g_proxy_inited = 0;

void ai_proxy_init(void) {
  if (InterlockedCompareExchange(&g_proxy_inited, 1, 0) == 0) {
    InitializeCriticalSection(&g_proxy_cs);
  }
}

void ai_proxy_shutdown(void) {
  if (!g_proxy_inited) return;

  EnterCriticalSection(&g_proxy_cs);
  ai_proxy_ctx_t* curr = g_proxy_list;
  while (curr) {
    InterlockedExchange(&curr->cancelled, 1);
    if (curr->hRequest) {
      WinHttpCloseHandle(curr->hRequest);
      curr->hRequest = NULL;
    }
    curr = curr->next;
  }
  LeaveCriticalSection(&g_proxy_cs);
}

static WCHAR* Utf8ToWide(const char* utf8_str) {
  if (!utf8_str) return NULL;
  int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
  if (wlen <= 0) return NULL;
  WCHAR* wstr = (WCHAR*)malloc(wlen * sizeof(WCHAR));
  if (wstr) {
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);
  }
  return wstr;
}

static char* Base64Encode(const unsigned char* src, size_t len) {
  static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (!src || len == 0) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  size_t out_len = 4 * ((len + 2) / 3);
  char* out = (char*)malloc(out_len + 1);
  if (!out) return NULL;
  size_t i, j = 0;
  for (i = 0; i < len; i += 3) {
    uint32_t a = src[i];
    uint32_t b = (i + 1 < len) ? src[i + 1] : 0;
    uint32_t c = (i + 2 < len) ? src[i + 2] : 0;
    uint32_t triple = (a << 16) | (b << 8) | c;
    out[j++] = b64_table[(triple >> 18) & 0x3F];
    out[j++] = b64_table[(triple >> 12) & 0x3F];
    out[j++] = (i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
    out[j++] = (i + 2 < len) ? b64_table[triple & 0x3F] : '=';
  }
  out[out_len] = '\0';
  return out;
}

static void ExecuteJs(cef_frame_t* frame, const char* js_code) {
  if (!frame || !js_code) return;
  cef_string_t js_str = {};
  cef_string_from_utf8(js_code, strlen(js_code), &js_str);
  frame->execute_java_script(frame, &js_str, NULL, 0);
  cef_string_clear(&js_str);
}

static void NotifyError(cef_frame_t* frame, const char* req_id, int status, const char* err_msg) {
  if (!frame || !req_id) return;
  char* b64_err = Base64Encode((const unsigned char*)(err_msg ? err_msg : "Unknown Error"),
                               err_msg ? strlen(err_msg) : 13);
  if (b64_err) {
    char js[1024];
    snprintf(js, sizeof(js),
             "if (window.onAIProxyError) { window.onAIProxyError('%s', %d, '%s'); }",
             req_id, status, b64_err);
    ExecuteJs(frame, js);
    free(b64_err);
  }
}

static DWORD WINAPI AIProxyWorkerThread(LPVOID param) {
  ai_proxy_ctx_t* ctx = (ai_proxy_ctx_t*)param;
  if (!ctx) return 0;

  URL_COMPONENTSW urlComp = {0};
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.dwHostNameLength = (DWORD)-1;
  urlComp.dwUrlPathLength = (DWORD)-1;
  urlComp.dwExtraInfoLength = (DWORD)-1;

  if (!WinHttpCrackUrl(ctx->w_url, 0, 0, &urlComp)) {
    NotifyError(ctx->frame, ctx->req_id, 0, "Invalid Target URL in Proxy");
    goto thread_cleanup;
  }

  WCHAR hostW[512] = {0};
  if (urlComp.dwHostNameLength > 0 && urlComp.dwHostNameLength < 511) {
    wcsncpy(hostW, urlComp.lpszHostName, urlComp.dwHostNameLength);
  } else {
    NotifyError(ctx->frame, ctx->req_id, 0, "Host name too long or missing");
    goto thread_cleanup;
  }

  WCHAR pathW[4096] = {0};
  if (urlComp.lpszUrlPath && urlComp.dwUrlPathLength > 0) {
    size_t total_path = urlComp.dwUrlPathLength + (urlComp.dwExtraInfoLength > 0 ? urlComp.dwExtraInfoLength : 0);
    if (total_path < 4095) {
      wcsncpy(pathW, urlComp.lpszUrlPath, total_path);
    } else {
      wcsncpy(pathW, urlComp.lpszUrlPath, 4095);
    }
  } else {
    wcscpy(pathW, L"/");
  }

  INTERNET_PORT port = urlComp.nPort;
  BOOL is_https = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

  HINTERNET hSession = WinHttpOpen(L"codex-cli/1.0",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS,
                                   0);
  if (!hSession) {
    NotifyError(ctx->frame, ctx->req_id, 0, "Failed to initialize WinHTTP session");
    goto thread_cleanup;
  }

  // Timeouts: resolve=15s, connect=15s, send=30s, receive=120s (for LLM reasoning)
  WinHttpSetTimeouts(hSession, 15000, 15000, 30000, 120000);

  HINTERNET hConnect = WinHttpConnect(hSession, hostW, port, 0);
  if (!hConnect) {
    NotifyError(ctx->frame, ctx->req_id, 0, "Failed to connect to host via WinHTTP");
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  DWORD open_flags = is_https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                         L"POST",
                                         pathW,
                                         NULL,
                                         WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         open_flags);
  if (!hRequest) {
    NotifyError(ctx->frame, ctx->req_id, 0, "Failed to open WinHTTP request");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  // Store hRequest for cancellation support
  EnterCriticalSection(&g_proxy_cs);
  ctx->hRequest = hRequest;
  BOOL is_cancelled = ctx->cancelled;
  LeaveCriticalSection(&g_proxy_cs);

  if (is_cancelled) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  // Add custom headers if provided
  if (ctx->w_headers && wcslen(ctx->w_headers) > 0) {
    WinHttpAddRequestHeaders(hRequest,
                             ctx->w_headers,
                             (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
  }

  // Send request with body
  BOOL bSend = WinHttpSendRequest(hRequest,
                                  WINHTTP_NO_ADDITIONAL_HEADERS,
                                  0,
                                  (LPVOID)ctx->body,
                                  (DWORD)ctx->body_len,
                                  (DWORD)ctx->body_len,
                                  0);
  if (!bSend) {
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "WinHttpSendRequest failed with error: %lu", GetLastError());
    NotifyError(ctx->frame, ctx->req_id, 0, err_msg);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  BOOL bRecv = WinHttpReceiveResponse(hRequest, NULL);
  if (!bRecv) {
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "WinHttpReceiveResponse failed with error: %lu", GetLastError());
    NotifyError(ctx->frame, ctx->req_id, 0, err_msg);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  DWORD statusCode = 0;
  DWORD dwSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX,
                      &statusCode,
                      &dwSize,
                      WINHTTP_NO_HEADER_INDEX);

  if (statusCode < 200 || statusCode >= 300) {
    char err_buf[4096] = {0};
    DWORD err_read = 0;
    WinHttpReadData(hRequest, err_buf, sizeof(err_buf) - 1, &err_read);
    err_buf[err_read] = '\0';
    NotifyError(ctx->frame, ctx->req_id, (int)statusCode, err_buf[0] ? err_buf : "HTTP Error Response");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    goto thread_cleanup;
  }

  // Streaming read loop
  char chunk_buf[8192];
  DWORD bytes_read = 0;
  while (!ctx->cancelled) {
    BOOL bRead = WinHttpReadData(hRequest, chunk_buf, sizeof(chunk_buf), &bytes_read);
    if (!bRead || bytes_read == 0) {
      break;
    }
    if (ctx->cancelled) break;

    char* b64_chunk = Base64Encode((const unsigned char*)chunk_buf, bytes_read);
    if (b64_chunk) {
      size_t js_len = strlen(b64_chunk) + 256;
      char* js = (char*)malloc(js_len);
      if (js) {
        snprintf(js, js_len,
                 "if (window.onAIProxyChunk) { window.onAIProxyChunk('%s', '%s'); }",
                 ctx->req_id, b64_chunk);
        ExecuteJs(ctx->frame, js);
        free(js);
      }
      free(b64_chunk);
    }
  }

  if (!ctx->cancelled) {
    char js[256];
    snprintf(js, sizeof(js),
             "if (window.onAIProxyDone) { window.onAIProxyDone('%s'); }",
             ctx->req_id);
    ExecuteJs(ctx->frame, js);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

thread_cleanup:
  // Detach and clean up from active list
  EnterCriticalSection(&g_proxy_cs);
  ai_proxy_ctx_t** curr = &g_proxy_list;
  while (*curr) {
    if (*curr == ctx) {
      *curr = ctx->next;
      break;
    }
    curr = &(*curr)->next;
  }
  LeaveCriticalSection(&g_proxy_cs);

  if (ctx->frame) {
    ctx->frame->base.release(&ctx->frame->base);
  }
  if (ctx->w_url) free(ctx->w_url);
  if (ctx->w_headers) free(ctx->w_headers);
  if (ctx->body) free(ctx->body);
  if (ctx->hThread) CloseHandle(ctx->hThread);
  free(ctx);

  return 0;
}

int ai_proxy_start_stream(cef_frame_t* target_frame,
                          const char* req_id,
                          const char* url,
                          const char* headers,
                          const char* body,
                          size_t body_len) {
  if (!target_frame || !req_id || !url) return 0;

  ai_proxy_init();

  ai_proxy_ctx_t* ctx = (ai_proxy_ctx_t*)calloc(1, sizeof(ai_proxy_ctx_t));
  if (!ctx) return 0;

  strncpy(ctx->req_id, req_id, sizeof(ctx->req_id) - 1);
  ctx->w_url = Utf8ToWide(url);
  ctx->w_headers = Utf8ToWide(headers);

  if (body && body_len > 0) {
    ctx->body = (char*)malloc(body_len);
    if (ctx->body) {
      memcpy(ctx->body, body, body_len);
      ctx->body_len = body_len;
    }
  }

  ctx->frame = target_frame;
  target_frame->base.add_ref(&target_frame->base);

  EnterCriticalSection(&g_proxy_cs);
  ctx->next = g_proxy_list;
  g_proxy_list = ctx;
  LeaveCriticalSection(&g_proxy_cs);

  HANDLE hThread = CreateThread(NULL, 0, AIProxyWorkerThread, ctx, 0, NULL);
  if (!hThread) {
    EnterCriticalSection(&g_proxy_cs);
    ai_proxy_ctx_t** curr = &g_proxy_list;
    while (*curr) {
      if (*curr == ctx) {
        *curr = ctx->next;
        break;
      }
      curr = &(*curr)->next;
    }
    LeaveCriticalSection(&g_proxy_cs);

    target_frame->base.release(&target_frame->base);
    if (ctx->w_url) free(ctx->w_url);
    if (ctx->w_headers) free(ctx->w_headers);
    if (ctx->body) free(ctx->body);
    free(ctx);
    return 0;
  }

  ctx->hThread = hThread;
  return 1;
}

void ai_proxy_cancel_stream(const char* req_id) {
  if (!g_proxy_inited || !req_id) return;

  EnterCriticalSection(&g_proxy_cs);
  ai_proxy_ctx_t* curr = g_proxy_list;
  while (curr) {
    if (strcmp(curr->req_id, req_id) == 0) {
      InterlockedExchange(&curr->cancelled, 1);
      if (curr->hRequest) {
        WinHttpCloseHandle(curr->hRequest);
        curr->hRequest = NULL;
      }
      break;
    }
    curr = curr->next;
  }
  LeaveCriticalSection(&g_proxy_cs);
}
