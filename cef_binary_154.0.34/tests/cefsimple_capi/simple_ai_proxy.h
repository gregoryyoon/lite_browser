// Copyright (c) 2026 LiteBrowser Authors. All rights reserved.
// Universal WinHTTP AI Streaming Proxy - bypasses browser CORS & handles SSE.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AI_PROXY_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AI_PROXY_H_

#include "include/capi/cef_frame_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes proxy subsystem and critical sections
void ai_proxy_init(void);

// Cleans up active proxy requests on shutdown
void ai_proxy_shutdown(void);

// Starts an asynchronous HTTP/HTTPS streaming request via native WinHTTP
// target_frame: The CEF frame where callbacks will be executed
// req_id: Unique request ID string (e.g. "req_1234567")
// url: Full target URL (https://...)
// headers: Custom HTTP headers (CRLF separated) or NULL
// body: Request payload (e.g. JSON string) or NULL
// body_len: Length of body in bytes
int ai_proxy_start_stream(cef_frame_t* target_frame,
                          const char* req_id,
                          const char* url,
                          const char* headers,
                          const char* body,
                          size_t body_len);

// Cancels an in-flight request by req_id
void ai_proxy_cancel_stream(const char* req_id);

#ifdef __cplusplus
}
#endif

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AI_PROXY_H_
