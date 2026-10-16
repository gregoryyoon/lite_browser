// Copyright (c) 2026 Gregory Yoon. All rights reserved.
// Custom URL Protocol Handler for external desktop applications (ETX, Zoom, Slack, Mailto, etc.)

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PROTOCOL_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PROTOCOL_HANDLER_H_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _cef_browser_t;
struct _cef_frame_t;

// Inspects the URL to determine if it is an external custom scheme registered in Windows
// (e.g. "etx12://...", "mailto:...", "zoommtg://...", etc.).
// If registered:
//   - Checks if (origin, scheme) is already saved in the "always allow" list.
//   - If not saved, displays a TaskDialog asking the user for confirmation.
//   - If confirmed, executes the URL via ShellExecute and optionally persists "always allow".
// Returns 1 if the URL was handled (either launched or canceled by user).
// Returns 0 if it is a standard web URL (http, https, file, chrome, etc.) or not an external protocol.
int HandleExternalProtocolIfNeeded(HWND parent_hwnd, const char* url, const char* current_page_url);

// Convenience wrapper that extracts current page URL from browser/frame if available
int HandleExternalProtocolForBrowser(HWND parent_hwnd, const char* url, struct _cef_browser_t* browser, struct _cef_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_PROTOCOL_HANDLER_H_
