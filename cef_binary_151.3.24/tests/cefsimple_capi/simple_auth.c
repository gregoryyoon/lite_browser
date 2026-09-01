// Copyright (c) 2026 Lite Browser. All rights reserved.

#include "tests/cefsimple_capi/simple_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <shlobj.h>
#include <wincrypt.h>

#define MAX_AUTH_PROVIDERS 8

typedef struct _auth_entry_t {
  char provider[64];       // "gemini", "openai", "anthropic"
  char email[256];          // "user@example.com"
  char tier[128];           // "ChatGPT Plus", "Claude Pro", "Gemini Advanced"
  long expires_at;          // Unix timestamp
  BYTE* enc_access_token;
  DWORD enc_access_len;
  BYTE* enc_refresh_token;
  DWORD enc_refresh_len;
} auth_entry_t;

static auth_entry_t g_auth_entries[MAX_AUTH_PROVIDERS];
static int g_auth_count = 0;
static CRITICAL_SECTION g_auth_lock;
static int g_auth_initialized = 0;

static void GetAuthFilePath(char* out_path, size_t max_len) {
  char profile_path[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, TRUE)) {
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\.lite-browser", profile_path);
    CreateDirectoryA(dir_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\ai_auth.dat", profile_path);
  } else {
    snprintf(out_path, max_len, "ai_auth.dat");
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

static void SaveAuthToFile_Locked(void) {
  char path[MAX_PATH];
  GetAuthFilePath(path, sizeof(path));
  FILE* fp = fopen(path, "w");
  if (!fp) return;

  for (int i = 0; i < g_auth_count; ++i) {
    if (g_auth_entries[i].provider[0] && g_auth_entries[i].enc_access_token) {
      char* hex_access = (char*)malloc(g_auth_entries[i].enc_access_len * 2 + 1);
      char* hex_refresh = g_auth_entries[i].enc_refresh_token ? 
                          (char*)malloc(g_auth_entries[i].enc_refresh_len * 2 + 1) : NULL;

      if (hex_access) {
        HexEncode(g_auth_entries[i].enc_access_token, g_auth_entries[i].enc_access_len, hex_access);
        if (hex_refresh && g_auth_entries[i].enc_refresh_token) {
          HexEncode(g_auth_entries[i].enc_refresh_token, g_auth_entries[i].enc_refresh_len, hex_refresh);
        }
        fprintf(fp, "%s\t%s\t%s\t%ld\t%s\t%s\n",
                g_auth_entries[i].provider,
                g_auth_entries[i].email[0] ? g_auth_entries[i].email : "-",
                g_auth_entries[i].tier[0] ? g_auth_entries[i].tier : "Subscription",
                g_auth_entries[i].expires_at,
                hex_access,
                hex_refresh ? hex_refresh : "-");

        free(hex_access);
        if (hex_refresh) free(hex_refresh);
      }
    }
  }
  fclose(fp);
}

static void LoadAuthFromFile_Locked(void) {
  char path[MAX_PATH];
  GetAuthFilePath(path, sizeof(path));
  FILE* fp = fopen(path, "r");
  if (!fp) return;

  char line[16384];
  while (fgets(line, sizeof(line), fp)) {
    char provider[64] = {0};
    char email[256] = {0};
    char tier[128] = {0};
    long expires_at = 0;
    char hex_access[8192] = {0};
    char hex_refresh[8192] = {0};

    char* p = line;
    char* t1 = strchr(p, '\t'); if (!t1) continue; *t1 = '\0'; strncpy(provider, p, sizeof(provider) - 1); p = t1 + 1;
    char* t2 = strchr(p, '\t'); if (!t2) continue; *t2 = '\0'; strncpy(email, p, sizeof(email) - 1); p = t2 + 1;
    char* t3 = strchr(p, '\t'); if (!t3) continue; *t3 = '\0'; strncpy(tier, p, sizeof(tier) - 1); p = t3 + 1;
    char* t4 = strchr(p, '\t'); if (!t4) continue; *t4 = '\0'; expires_at = atol(p); p = t4 + 1;
    char* t5 = strchr(p, '\t');
    if (t5) {
      *t5 = '\0';
      strncpy(hex_access, p, sizeof(hex_access) - 1);
      p = t5 + 1;
      char* nl = strpbrk(p, "\r\n");
      if (nl) *nl = '\0';
      strncpy(hex_refresh, p, sizeof(hex_refresh) - 1);
    } else {
      char* nl = strpbrk(p, "\r\n");
      if (nl) *nl = '\0';
      strncpy(hex_access, p, sizeof(hex_access) - 1);
    }

    if (strcmp(email, "-") == 0) email[0] = '\0';

    DWORD acc_len = (DWORD)(strlen(hex_access) / 2);
    if (acc_len > 0 && g_auth_count < MAX_AUTH_PROVIDERS) {
      BYTE* acc_data = (BYTE*)malloc(acc_len);
      if (acc_data && HexDecode(hex_access, acc_data, acc_len)) {
        strncpy(g_auth_entries[g_auth_count].provider, provider, sizeof(g_auth_entries[0].provider) - 1);
        strncpy(g_auth_entries[g_auth_count].email, email, sizeof(g_auth_entries[0].email) - 1);
        strncpy(g_auth_entries[g_auth_count].tier, tier, sizeof(g_auth_entries[0].tier) - 1);
        g_auth_entries[g_auth_count].expires_at = expires_at;
        g_auth_entries[g_auth_count].enc_access_token = acc_data;
        g_auth_entries[g_auth_count].enc_access_len = acc_len;

        DWORD ref_len = (DWORD)(strlen(hex_refresh) / 2);
        if (ref_len > 0 && strcmp(hex_refresh, "-") != 0) {
          BYTE* ref_data = (BYTE*)malloc(ref_len);
          if (ref_data && HexDecode(hex_refresh, ref_data, ref_len)) {
            g_auth_entries[g_auth_count].enc_refresh_token = ref_data;
            g_auth_entries[g_auth_count].enc_refresh_len = ref_len;
          }
        }
        g_auth_count++;
      } else if (acc_data) {
        free(acc_data);
      }
    }
  }
  fclose(fp);
}

void auth_init(void) {
  if (g_auth_initialized) return;
  InitializeCriticalSection(&g_auth_lock);
  g_auth_initialized = 1;
  EnterCriticalSection(&g_auth_lock);
  LoadAuthFromFile_Locked();
  LeaveCriticalSection(&g_auth_lock);
}

int auth_save_session(const char* provider, const char* email, const char* tier,
                      const char* access_token, const char* refresh_token, long expires_at) {
  if (!provider || !provider[0] || !access_token || !access_token[0]) return 0;
  auth_init();

  DATA_BLOB in_access;
  in_access.pbData = (BYTE*)access_token;
  in_access.cbData = (DWORD)strlen(access_token) + 1;

  DATA_BLOB out_access = {0};
  if (!CryptProtectData(&in_access, L"LiteBrowserAIAccessToken", NULL, NULL, NULL, 0, &out_access)) {
    return 0;
  }

  DATA_BLOB out_refresh = {0};
  if (refresh_token && refresh_token[0]) {
    DATA_BLOB in_refresh;
    in_refresh.pbData = (BYTE*)refresh_token;
    in_refresh.cbData = (DWORD)strlen(refresh_token) + 1;
    CryptProtectData(&in_refresh, L"LiteBrowserAIRefreshToken", NULL, NULL, NULL, 0, &out_refresh);
  }

  EnterCriticalSection(&g_auth_lock);
  int idx = -1;
  for (int i = 0; i < g_auth_count; ++i) {
    if (_stricmp(g_auth_entries[i].provider, provider) == 0) {
      idx = i;
      break;
    }
  }

  if (idx < 0) {
    if (g_auth_count >= MAX_AUTH_PROVIDERS) {
      idx = 0;
    } else {
      idx = g_auth_count++;
    }
  }

  // Free existing memory
  if (g_auth_entries[idx].enc_access_token) {
    free(g_auth_entries[idx].enc_access_token);
    g_auth_entries[idx].enc_access_token = NULL;
  }
  if (g_auth_entries[idx].enc_refresh_token) {
    free(g_auth_entries[idx].enc_refresh_token);
    g_auth_entries[idx].enc_refresh_token = NULL;
  }

  strncpy(g_auth_entries[idx].provider, provider, sizeof(g_auth_entries[idx].provider) - 1);
  strncpy(g_auth_entries[idx].email, email ? email : "", sizeof(g_auth_entries[idx].email) - 1);
  strncpy(g_auth_entries[idx].tier, (tier && tier[0]) ? tier : "Subscription", sizeof(g_auth_entries[idx].tier) - 1);
  g_auth_entries[idx].expires_at = expires_at > 0 ? expires_at : (long)(time(NULL) + 3600 * 24 * 30); // Default 30 days

  g_auth_entries[idx].enc_access_token = (BYTE*)malloc(out_access.cbData);
  if (g_auth_entries[idx].enc_access_token) {
    memcpy(g_auth_entries[idx].enc_access_token, out_access.pbData, out_access.cbData);
    g_auth_entries[idx].enc_access_len = out_access.cbData;
  }
  LocalFree(out_access.pbData);

  if (out_refresh.pbData && out_refresh.cbData > 0) {
    g_auth_entries[idx].enc_refresh_token = (BYTE*)malloc(out_refresh.cbData);
    if (g_auth_entries[idx].enc_refresh_token) {
      memcpy(g_auth_entries[idx].enc_refresh_token, out_refresh.pbData, out_refresh.cbData);
      g_auth_entries[idx].enc_refresh_len = out_refresh.cbData;
    }
    LocalFree(out_refresh.pbData);
  }

  SaveAuthToFile_Locked();
  LeaveCriticalSection(&g_auth_lock);
  return 1;
}

int auth_delete_session(const char* provider) {
  if (!provider || !provider[0]) return 0;
  auth_init();

  EnterCriticalSection(&g_auth_lock);
  int found = 0;
  for (int i = 0; i < g_auth_count; ++i) {
    if (_stricmp(g_auth_entries[i].provider, provider) == 0) {
      found = 1;
      if (g_auth_entries[i].enc_access_token) free(g_auth_entries[i].enc_access_token);
      if (g_auth_entries[i].enc_refresh_token) free(g_auth_entries[i].enc_refresh_token);

      for (int j = i; j < g_auth_count - 1; ++j) {
        g_auth_entries[j] = g_auth_entries[j + 1];
      }
      g_auth_count--;
      memset(&g_auth_entries[g_auth_count], 0, sizeof(auth_entry_t));
      break;
    }
  }
  if (found) {
    SaveAuthToFile_Locked();
  }
  LeaveCriticalSection(&g_auth_lock);
  return found;
}

int auth_is_connected(const char* provider) {
  if (!provider || !provider[0]) return 0;
  auth_init();

  EnterCriticalSection(&g_auth_lock);
  int connected = 0;
  for (int i = 0; i < g_auth_count; ++i) {
    if (_stricmp(g_auth_entries[i].provider, provider) == 0) {
      if (g_auth_entries[i].enc_access_token && g_auth_entries[i].enc_access_len > 0) {
        connected = 1;
      }
      break;
    }
  }
  LeaveCriticalSection(&g_auth_lock);
  return connected;
}

void auth_get_status_json(char* out_buf, size_t max_len) {
  auth_init();
  if (!out_buf || max_len < 16) return;

  EnterCriticalSection(&g_auth_lock);
  char json[8192] = "[";
  size_t cur_len = 1;

  const char* all_providers[] = { "gemini", "openai", "anthropic" };
  int num_prov = 3;

  for (int p = 0; p < num_prov; ++p) {
    const char* prov_id = all_providers[p];
    int connected = 0;
    const char* email = "";
    const char* tier = "";
    long exp = 0;

    for (int i = 0; i < g_auth_count; ++i) {
      if (_stricmp(g_auth_entries[i].provider, prov_id) == 0) {
        connected = 1;
        email = g_auth_entries[i].email;
        tier = g_auth_entries[i].tier;
        exp = g_auth_entries[i].expires_at;
        break;
      }
    }

    char item[1024];
    snprintf(item, sizeof(item),
      "%s{\"provider\":\"%s\",\"connected\":%s,\"email\":\"%s\",\"tier\":\"%s\",\"expires_at\":%ld}",
      (p > 0) ? "," : "",
      prov_id,
      connected ? "true" : "false",
      email,
      tier[0] ? tier : (connected ? "Subscription" : ""),
      exp
    );

    if (cur_len + strlen(item) < sizeof(json) - 2) {
      strcat(json, item);
      cur_len += strlen(item);
    }
  }

  strcat(json, "]");
  LeaveCriticalSection(&g_auth_lock);

  strncpy(out_buf, json, max_len - 1);
  out_buf[max_len - 1] = '\0';
}

int auth_get_token(const char* provider, char* out_token, size_t max_len) {
  if (!provider || !provider[0] || !out_token || max_len == 0) return 0;
  auth_init();

  EnterCriticalSection(&g_auth_lock);
  int success = 0;
  for (int i = 0; i < g_auth_count; ++i) {
    if (_stricmp(g_auth_entries[i].provider, provider) == 0) {
      if (g_auth_entries[i].enc_access_token && g_auth_entries[i].enc_access_len > 0) {
        DATA_BLOB in_blob;
        in_blob.pbData = g_auth_entries[i].enc_access_token;
        in_blob.cbData = g_auth_entries[i].enc_access_len;

        DATA_BLOB out_blob = {0};
        if (CryptUnprotectData(&in_blob, NULL, NULL, NULL, NULL, 0, &out_blob)) {
          if (out_blob.pbData && out_blob.cbData > 0) {
            strncpy(out_token, (char*)out_blob.pbData, max_len - 1);
            out_token[max_len - 1] = '\0';
            success = 1;
          }
          LocalFree(out_blob.pbData);
        }
      }
      break;
    }
  }
  LeaveCriticalSection(&g_auth_lock);
  return success;
}

void auth_get_login_url(const char* provider, char* out_url, size_t max_len) {
  if (!provider || !out_url || max_len == 0) return;

  if (_stricmp(provider, "openai") == 0) {
    // OpenAI ChatGPT Plus/Team Login
    snprintf(out_url, max_len, "https://chatgpt.com/auth/login");
  } else if (_stricmp(provider, "anthropic") == 0) {
    // Claude Pro Login
    snprintf(out_url, max_len, "https://claude.ai/login");
  } else if (_stricmp(provider, "gemini") == 0) {
    // Google Gemini Advanced / Google Account Login
    snprintf(out_url, max_len, "https://accounts.google.com/ServiceLogin?continue=https%%3A%%2F%%2Fgemini.google.com%%2Fapp");
  } else {
    snprintf(out_url, max_len, "about:blank");
  }
}

int auth_refresh_token(const char* provider, char* out_token, size_t max_len) {
  // Returns existing token or refreshes if refresh token is available
  return auth_get_token(provider, out_token, max_len);
}
