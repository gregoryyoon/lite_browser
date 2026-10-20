// Copyright (c) 2026 Gregory Yoon. All rights reserved.
// Permission Handler for Web Permissions (Local Network Access, Loopback, etc.)

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PERMISSION_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PERMISSION_HANDLER_H_

#include <windows.h>
#include "include/capi/cef_permission_handler_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _simple_handler_t simple_handler_t;

// Creates a new permission handler for the client
cef_permission_handler_t* simple_permission_handler_create(simple_handler_t* parent_handler);

// Cleans up any session-temporary allowed origins (resets them in CefRequestContext)
void simple_permission_handler_cleanup_session(struct _cef_request_context_t* context);

#ifdef __cplusplus
}
#endif

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PERMISSION_HANDLER_H_
