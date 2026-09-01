#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_INSTALLER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_INSTALLER_H_

#include <windows.h>
#include "tests/cefsimple_capi/browser_context.h"

#ifdef __cplusplus
extern "C" {
#endif

// Reports launch success to CEF Installer launch health tracker.
void simple_installer_report_launch_success(void);

// Triggers an asynchronous CEF update check in a background worker thread and notifies UI.
void simple_installer_check_update_async(browser_window_t* win_ctx);

#ifdef __cplusplus
}
#endif

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_INSTALLER_H_
