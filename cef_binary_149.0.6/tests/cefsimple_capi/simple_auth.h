// Copyright (c) 2026 Lite Browser. All rights reserved.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AUTH_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AUTH_H_

#include <windows.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize AI Auth subsystem
void auth_init(void);

// Save or update subscription auth session for a provider (DPAPI encrypted)
int auth_save_session(const char* provider, const char* email, const char* tier,
                      const char* access_token, const char* refresh_token, long expires_at);

// Delete session for a provider (Logout)
int auth_delete_session(const char* provider);

// Check if a provider has an active subscription session (returns 1 if connected, 0 otherwise)
int auth_is_connected(const char* provider);

// Retrieve all provider connection statuses as JSON for Sidepanel UI
// Format: [{"provider":"gemini","connected":true,"email":"user@gmail.com","tier":"Gemini Advanced","expires_at":1725000000}, ...]
void auth_get_status_json(char* out_buf, size_t max_len);

// Retrieve decrypted Bearer Access Token for a provider (returns 1 on success, 0 on failure)
int auth_get_token(const char* provider, char* out_token, size_t max_len);

// Get OAuth Authorization Login URL for a provider
void auth_get_login_url(const char* provider, char* out_url, size_t max_len);

// Refresh OAuth access token if expired or near expiry (returns 1 on success, 0 on failure)
int auth_refresh_token(const char* provider, char* out_token, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_AUTH_H_
