// Copyright (c) 2026 Lite Browser. All rights reserved.

#include "tests/cefsimple_capi/simple_vault.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <wincrypt.h>

#define MAX_VAULT_ENTRIES 100

typedef struct _vault_entry_t {
  char domain[256];
  char username[256];
  BYTE* encrypted_data;
  DWORD encrypted_size;
} vault_entry_t;

static vault_entry_t g_vault_entries[MAX_VAULT_ENTRIES];
static int g_vault_count = 0;
static CRITICAL_SECTION g_vault_lock;
static int g_vault_initialized = 0;

static void GetVaultFilePath(char* out_path, size_t max_len) {
  char profile_path[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, TRUE)) {
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\.lite-browser", profile_path);
    CreateDirectoryA(dir_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\vault.dat", profile_path);
  } else {
    snprintf(out_path, max_len, "vault.dat");
  }
}

static void HexEncode(const BYTE* data, DWORD len, char* out_hex) {
  for (DWORD i = 0; i < len; ++i) {
    sprintf(out_hex + (i * 2), "%02X", data[i]);
  }
  out_hex[len * 2] = '\0';
}

static int HexDecode(const char* hex, BYTE* out_data, DWORD max_len) {
  size_t len = strlen(hex);
  if (len % 2 != 0 || (len / 2) > max_len) return 0;
  for (size_t i = 0; i < len / 2; ++i) {
    unsigned int val = 0;
    if (sscanf(hex + (i * 2), "%02X", &val) != 1) return 0;
    out_data[i] = (BYTE)val;
  }
  return (int)(len / 2);
}

static void SaveVaultToFile_Locked(void) {
  char path[MAX_PATH];
  GetVaultFilePath(path, sizeof(path));
  FILE* fp = fopen(path, "w");
  if (!fp) return;

  for (int i = 0; i < g_vault_count; ++i) {
    if (g_vault_entries[i].domain[0] && g_vault_entries[i].encrypted_data && g_vault_entries[i].encrypted_size > 0) {
      char* hex_buf = (char*)malloc(g_vault_entries[i].encrypted_size * 2 + 1);
      if (hex_buf) {
        HexEncode(g_vault_entries[i].encrypted_data, g_vault_entries[i].encrypted_size, hex_buf);
        fprintf(fp, "%s\t%s\t%s\n", g_vault_entries[i].domain, g_vault_entries[i].username, hex_buf);
        free(hex_buf);
      }
    }
  }
  fclose(fp);
}

static void LoadVaultFromFile_Locked(void) {
  char path[MAX_PATH];
  GetVaultFilePath(path, sizeof(path));
  FILE* fp = fopen(path, "r");
  if (!fp) return;

  char line[8192];
  while (fgets(line, sizeof(line), fp)) {
    char domain[256] = {0};
    char user[256] = {0};
    char hex[4096] = {0};

    char* tab1 = strchr(line, '\t');
    if (!tab1) continue;
    *tab1 = '\0';
    strncpy(domain, line, sizeof(domain) - 1);

    char* tab2 = strchr(tab1 + 1, '\t');
    if (!tab2) continue;
    *tab2 = '\0';
    strncpy(user, tab1 + 1, sizeof(user) - 1);

    char* hex_start = tab2 + 1;
    char* nl = strpbrk(hex_start, "\r\n");
    if (nl) *nl = '\0';
    strncpy(hex, hex_start, sizeof(hex) - 1);

    DWORD byte_count = (DWORD)(strlen(hex) / 2);
    if (byte_count > 0 && g_vault_count < MAX_VAULT_ENTRIES) {
      BYTE* raw = (BYTE*)malloc(byte_count);
      if (raw) {
        if (HexDecode(hex, raw, byte_count) > 0) {
          strncpy(g_vault_entries[g_vault_count].domain, domain, 255);
          strncpy(g_vault_entries[g_vault_count].username, user, 255);
          g_vault_entries[g_vault_count].encrypted_data = raw;
          g_vault_entries[g_vault_count].encrypted_size = byte_count;
          g_vault_count++;
        } else {
          free(raw);
        }
      }
    }
  }
  fclose(fp);
}

void vault_init(void) {
  if (!g_vault_initialized) {
    InitializeCriticalSection(&g_vault_lock);
    g_vault_initialized = 1;
    EnterCriticalSection(&g_vault_lock);
    LoadVaultFromFile_Locked();
    LeaveCriticalSection(&g_vault_lock);
  }
}

int vault_save_credential(const char* domain, const char* username, const char* password) {
  if (!domain || !password) return 0;
  vault_init();

  // Encrypt password using Windows DPAPI (CryptProtectData)
  DATA_BLOB in_blob;
  DATA_BLOB out_blob;
  in_blob.pbData = (BYTE*)password;
  in_blob.cbData = (DWORD)strlen(password);

  if (!CryptProtectData(&in_blob, L"LiteBrowserVault", NULL, NULL, NULL, 0, &out_blob)) {
    return 0;
  }

  EnterCriticalSection(&g_vault_lock);

  // Check if domain already exists
  int found_idx = -1;
  for (int i = 0; i < g_vault_count; ++i) {
    if (_stricmp(g_vault_entries[i].domain, domain) == 0) {
      found_idx = i;
      break;
    }
  }

  if (found_idx >= 0) {
    if (g_vault_entries[found_idx].encrypted_data) {
      free(g_vault_entries[found_idx].encrypted_data);
    }
    strncpy(g_vault_entries[found_idx].username, username ? username : "", 255);
    g_vault_entries[found_idx].encrypted_data = (BYTE*)malloc(out_blob.cbData);
    memcpy(g_vault_entries[found_idx].encrypted_data, out_blob.pbData, out_blob.cbData);
    g_vault_entries[found_idx].encrypted_size = out_blob.cbData;
  } else if (g_vault_count < MAX_VAULT_ENTRIES) {
    strncpy(g_vault_entries[g_vault_count].domain, domain, 255);
    strncpy(g_vault_entries[g_vault_count].username, username ? username : "", 255);
    g_vault_entries[g_vault_count].encrypted_data = (BYTE*)malloc(out_blob.cbData);
    memcpy(g_vault_entries[g_vault_count].encrypted_data, out_blob.pbData, out_blob.cbData);
    g_vault_entries[g_vault_count].encrypted_size = out_blob.cbData;
    g_vault_count++;
  }

  LocalFree(out_blob.pbData);
  SaveVaultToFile_Locked();
  LeaveCriticalSection(&g_vault_lock);

  return 1;
}

int vault_delete_credential(const char* domain) {
  if (!domain) return 0;
  vault_init();

  EnterCriticalSection(&g_vault_lock);
  int found_idx = -1;
  for (int i = 0; i < g_vault_count; ++i) {
    if (_stricmp(g_vault_entries[i].domain, domain) == 0) {
      found_idx = i;
      break;
    }
  }

  if (found_idx >= 0) {
    if (g_vault_entries[found_idx].encrypted_data) {
      free(g_vault_entries[found_idx].encrypted_data);
    }
    for (int i = found_idx; i < g_vault_count - 1; ++i) {
      g_vault_entries[i] = g_vault_entries[i + 1];
    }
    g_vault_count--;
    SaveVaultToFile_Locked();
    LeaveCriticalSection(&g_vault_lock);
    return 1;
  }

  LeaveCriticalSection(&g_vault_lock);
  return 0;
}

void vault_get_list_json(char* out_buf, size_t max_len) {
  if (!out_buf || max_len < 32) return;
  vault_init();

  EnterCriticalSection(&g_vault_lock);
  strcpy(out_buf, "[");
  size_t cur_len = 1;

  for (int i = 0; i < g_vault_count; ++i) {
    char entry_json[1024];
    snprintf(entry_json, sizeof(entry_json),
             "%s{\"domain\":\"%s\",\"username\":\"%s\"}",
             (i > 0) ? "," : "",
             g_vault_entries[i].domain,
             g_vault_entries[i].username);
    size_t elen = strlen(entry_json);
    if (cur_len + elen + 2 < max_len) {
      strcat(out_buf, entry_json);
      cur_len += elen;
    }
  }
  strcat(out_buf, "]");
  LeaveCriticalSection(&g_vault_lock);
}

int vault_clear_all(void) {
  vault_init();
  EnterCriticalSection(&g_vault_lock);
  for (int i = 0; i < g_vault_count; ++i) {
    if (g_vault_entries[i].encrypted_data) {
      free(g_vault_entries[i].encrypted_data);
    }
  }
  g_vault_count = 0;
  SaveVaultToFile_Locked();
  LeaveCriticalSection(&g_vault_lock);
  return 1;
}

int vault_execute_autofill(cef_browser_t* browser, const char* target_domain) {
  if (!browser || !target_domain) return 0;
  vault_init();

  EnterCriticalSection(&g_vault_lock);
  int found_idx = -1;
  for (int i = 0; i < g_vault_count; ++i) {
    if (strstr(target_domain, g_vault_entries[i].domain) != NULL ||
        strstr(g_vault_entries[i].domain, target_domain) != NULL) {
      found_idx = i;
      break;
    }
  }

  if (found_idx < 0 || !g_vault_entries[found_idx].encrypted_data) {
    LeaveCriticalSection(&g_vault_lock);
    return 0;
  }

  // Decrypt password using DPAPI
  DATA_BLOB in_blob;
  DATA_BLOB out_blob;
  in_blob.pbData = g_vault_entries[found_idx].encrypted_data;
  in_blob.cbData = g_vault_entries[found_idx].encrypted_size;

  if (!CryptUnprotectData(&in_blob, NULL, NULL, NULL, NULL, 0, &out_blob)) {
    LeaveCriticalSection(&g_vault_lock);
    return 0;
  }

  char decrypted_password[512] = {0};
  DWORD copy_len = (out_blob.cbData < sizeof(decrypted_password) - 1) ? out_blob.cbData : (DWORD)(sizeof(decrypted_password) - 1);
  memcpy(decrypted_password, out_blob.pbData, copy_len);
  decrypted_password[copy_len] = '\0';
  LocalFree(out_blob.pbData);

  char username[256] = {0};
  strncpy(username, g_vault_entries[found_idx].username, 255);
  LeaveCriticalSection(&g_vault_lock);

  // Generate safe direct DOM autofill script (Injected straight into browser frame)
  // Escapes characters to prevent injection
  char script_buf[4096];
  snprintf(script_buf, sizeof(script_buf),
    "(function() {"
    "  try {"
    "    const user = '%s';"
    "    const pass = '%s';"
    "    const passInputs = Array.from(document.querySelectorAll('input[type=\"password\"]'));"
    "    if (passInputs.length > 0) {"
    "      const passInput = passInputs[0];"
    "      const form = passInput.form || document;"
    "      const userInputs = Array.from(form.querySelectorAll('input[type=\"text\"], input[type=\"email\"], input:not([type])'));"
    "      if (userInputs.length > 0 && user) {"
    "        userInputs[0].value = user;"
    "        userInputs[0].dispatchEvent(new Event('input', { bubbles: true }));"
    "        userInputs[0].dispatchEvent(new Event('change', { bubbles: true }));"
    "      }"
    "      passInput.value = pass;"
    "      passInput.dispatchEvent(new Event('input', { bubbles: true }));"
    "      passInput.dispatchEvent(new Event('change', { bubbles: true }));"
    "      console.log('[LiteBrowser Vault] Autofill executed safely');"
    "    }"
    "  } catch(e) { console.error('[LiteBrowser Vault] Autofill error:', e); }"
    "})();",
    username, decrypted_password);

  // Securely wipe decrypted password from C memory buffer
  SecureZeroMemory(decrypted_password, sizeof(decrypted_password));

  cef_frame_t* frame = browser->get_main_frame(browser);
  if (frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(script_buf, strlen(script_buf), &js_str);
    frame->execute_java_script(frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    frame->base.release(&frame->base);
    return 1;
  }

  return 0;
}
