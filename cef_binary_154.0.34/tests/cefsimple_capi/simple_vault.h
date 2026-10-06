// Copyright (c) 2026 Lite Browser. All rights reserved.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VAULT_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VAULT_H_

#include <windows.h>
#include <stddef.h>
#include "include/capi/cef_browser_capi.h"

// Initialize local vault subsystem
void vault_init(void);

// Save or update credentials for a domain (encrypted via Windows DPAPI)
int vault_save_credential(const char* domain, const char* username, const char* password);

// Delete credentials for a domain
int vault_delete_credential(const char* domain);

// List saved credentials as JSON (Domain & Username ONLY - Plaintext password is NEVER included)
void vault_get_list_json(char* out_buf, size_t max_len);

// Clear all credentials
int vault_clear_all(void);

// Secure Autofill Execution: Injects password directly into DOM and returns status (0: failed, 1: success)
// Plaintext password is NEVER exposed to LLM context or caller
int vault_execute_autofill(cef_browser_t* browser, const char* target_domain);

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VAULT_H_
