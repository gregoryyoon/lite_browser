#include "default_browser.h"

#if defined(_WIN32) || defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>

static LONG reg_set_str(HKEY root, const wchar_t* subkey, const wchar_t* val_name, const wchar_t* str_data) {
  HKEY hKey = NULL;
  LONG res = RegCreateKeyExW(root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
  if (res == ERROR_SUCCESS && hKey) {
    DWORD byte_count = (DWORD)((wcslen(str_data) + 1) * sizeof(wchar_t));
    res = RegSetValueExW(hKey, val_name, 0, REG_SZ, (const BYTE*)str_data, byte_count);
    RegCloseKey(hKey);
  }
  return res;
}

int default_browser_is_default(void) {
  HKEY hKey = NULL;
  LONG res = RegOpenKeyExW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice",
      0,
      KEY_READ,
      &hKey);
  if (res != ERROR_SUCCESS) {
    return 0;
  }

  wchar_t prog_id[256] = {0};
  DWORD type = 0;
  DWORD byte_size = sizeof(prog_id);
  res = RegQueryValueExW(hKey, L"ProgId", NULL, &type, (BYTE*)prog_id, &byte_size);
  RegCloseKey(hKey);

  if (res == ERROR_SUCCESS && type == REG_SZ) {
    if (_wcsicmp(prog_id, L"LiteBrowserHTML") == 0) {
      return 1;
    }
  }
  return 0;
}

void default_browser_register_capabilities(void) {
  wchar_t exe_path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exe_path, MAX_PATH);
  if (wcslen(exe_path) == 0) {
    return;
  }

  wchar_t open_cmd[MAX_PATH + 32] = {0};
  swprintf_s(open_cmd, MAX_PATH + 32, L"\"%s\" \"%%1\"", exe_path);

  wchar_t icon_path[MAX_PATH + 16] = {0};
  swprintf_s(icon_path, MAX_PATH + 16, L"\"%s\",0", exe_path);

  // 1. ProgID: LiteBrowserHTML
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\LiteBrowserHTML", NULL, L"Lite Browser Document");
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\LiteBrowserHTML", L"FriendlyTypeName", L"Lite Browser Document");
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\LiteBrowserHTML\\DefaultIcon", NULL, icon_path);
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\LiteBrowserHTML\\shell\\open\\command", NULL, open_cmd);

  // 2. OpenWithProgids for .htm and .html
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\.htm\\OpenWithProgids", L"LiteBrowserHTML", L"");
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Classes\\.html\\OpenWithProgids", L"LiteBrowserHTML", L"");

  // 3. StartMenuInternet\LiteBrowser
  const wchar_t* smi_base = L"Software\\Clients\\StartMenuInternet\\LiteBrowser";
  reg_set_str(HKEY_CURRENT_USER, smi_base, NULL, L"Lite Browser");
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\DefaultIcon", NULL, icon_path);

  wchar_t smi_cmd[MAX_PATH + 16] = {0};
  swprintf_s(smi_cmd, MAX_PATH + 16, L"\"%s\"", exe_path);
  reg_set_str(HKEY_CURRENT_USER, L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\shell\\open\\command", NULL, smi_cmd);

  // Capabilities
  const wchar_t* cap_base = L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\Capabilities";
  reg_set_str(HKEY_CURRENT_USER, cap_base, L"ApplicationName", L"Lite Browser");
  reg_set_str(HKEY_CURRENT_USER, cap_base, L"ApplicationIcon", icon_path);
  reg_set_str(HKEY_CURRENT_USER, cap_base, L"ApplicationDescription", L"Lite Browser - Fast, lightweight, AI-integrated modern browser.");

  // URLAssociations
  const wchar_t* url_base = L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\Capabilities\\URLAssociations";
  reg_set_str(HKEY_CURRENT_USER, url_base, L"http", L"LiteBrowserHTML");
  reg_set_str(HKEY_CURRENT_USER, url_base, L"https", L"LiteBrowserHTML");

  // FileAssociations
  const wchar_t* file_base = L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\Capabilities\\FileAssociations";
  reg_set_str(HKEY_CURRENT_USER, file_base, L".htm", L"LiteBrowserHTML");
  reg_set_str(HKEY_CURRENT_USER, file_base, L".html", L"LiteBrowserHTML");
  reg_set_str(HKEY_CURRENT_USER, file_base, L".pdf", L"LiteBrowserHTML");
  reg_set_str(HKEY_CURRENT_USER, file_base, L".svg", L"LiteBrowserHTML");

  // 4. RegisteredApplications
  reg_set_str(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"LiteBrowser", L"Software\\Clients\\StartMenuInternet\\LiteBrowser\\Capabilities");
}

void default_browser_open_settings(void) {
  default_browser_register_capabilities();

  // Try direct Windows 11/10 Default Apps page with registered app parameter first
  HINSTANCE hInst = ShellExecuteW(
      NULL,
      L"open",
      L"ms-settings:defaultapps?registeredAppMachine=LiteBrowser",
      NULL,
      NULL,
      SW_SHOWNORMAL);

  if ((INT_PTR)hInst <= 32) {
    // Fallback to standard Default Apps page
    ShellExecuteW(NULL, L"open", L"ms-settings:defaultapps", NULL, NULL, SW_SHOWNORMAL);
  }
}

#else

int default_browser_is_default(void) {
  return 0;
}

void default_browser_register_capabilities(void) {}

void default_browser_open_settings(void) {}

#endif
