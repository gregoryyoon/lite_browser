// Copyright (c) 2026 Lite Browser. All rights reserved.
// Auto-updater subsystem for Lite Browser using GitHub Releases and WinVerifyTrust.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UPDATER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UPDATER_H_

#include <windows.h>
#include <stddef.h>
#include "tests/cefsimple_capi/version.h"

#define WM_UPDATER_STATUS_CHANGED (WM_APP + 55)

typedef enum {
  UPDATE_STATE_IDLE = 0,
  UPDATE_STATE_CHECKING,
  UPDATE_STATE_UP_TO_DATE,
  UPDATE_STATE_AVAILABLE,
  UPDATE_STATE_DOWNLOADING,
  UPDATE_STATE_READY,
  UPDATE_STATE_ERROR
} update_state_t;

// Initialize updater critical sections and state
void simple_updater_init(void);

// Clean up updater resources
void simple_updater_cleanup(void);

// Set parent window handle for UI thread notifications
void simple_updater_set_parent_hwnd(HWND hwnd);

// Start check for updates asynchronously
// is_silent: 1 for background startup check, 0 for user-initiated manual check
void simple_updater_check_async(HWND parent_hwnd, int is_silent);

// Returns current state
update_state_t simple_updater_get_state(void);

// Returns 1 if a verified installer package is ready to install & restart
int simple_updater_is_ready_to_restart(void);

// Format current status as JSON
void simple_updater_get_status_json(char* out_buf, size_t max_len);

// Spawns LiteBrowserInstaller.exe with /S (silent mode) and closes Lite Browser
void simple_updater_apply_and_restart(HWND parent_hwnd);

// Broadcast update status to parent window
void simple_updater_broadcast_status(void);

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UPDATER_H_
