#ifndef LITEBROWSER_DEFAULT_BROWSER_H_
#define LITEBROWSER_DEFAULT_BROWSER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 if Lite Browser is currently the default HTTP/HTTPS browser, 0 otherwise.
int default_browser_is_default(void);

// Registers Lite Browser capabilities and ProgID into current user registry (HKCU).
void default_browser_register_capabilities(void);

// Registers capabilities if needed and launches the Windows default apps settings UI.
void default_browser_open_settings(void);

#ifdef __cplusplus
}
#endif

#endif  // LITEBROWSER_DEFAULT_BROWSER_H_
