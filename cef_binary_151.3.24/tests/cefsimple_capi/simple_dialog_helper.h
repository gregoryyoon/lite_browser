#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DIALOG_HELPER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DIALOG_HELPER_H_
#pragma once

#if defined(OS_WIN) || defined(_WIN32)
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Win32 CBT hook for positioning CEF core modal dialogs at top-center.
void simple_dialog_helper_init(void);

// Clean up the Win32 CBT hook.
void simple_dialog_helper_cleanup(void);

// Reposition any currently open modal dialogs relative to their parent browser window.
void simple_dialog_helper_reposition_open_dialogs(HWND main_hwnd);

#ifdef __cplusplus
}
#endif

#endif  // defined(OS_WIN) || defined(_WIN32)
#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DIALOG_HELPER_H_
