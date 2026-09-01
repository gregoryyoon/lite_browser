// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_download_handler.h"
#include "tests/cefsimple_capi/simple_vault.h"
#include "tests/cefsimple_capi/simple_auth.h"
#include "tests/cefsimple_capi/simple_installer.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_task_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_utils.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
extern int GetUIHeightForWindow(HWND hwnd);


static void GetBookmarksFilePath(char* out_path, size_t max_len) {
  char user_profile[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, user_profile, CSIDL_PROFILE, FALSE)) {
    snprintf(out_path, max_len, "%s\\.lite-browser", user_profile);
    CreateDirectoryA(out_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\bookmarks_v2.json", user_profile);
  } else {
    snprintf(out_path, max_len, "C:\\projects\\lite_browser\\bookmarks_v2.json");
  }
}

static void GetHistoryFilePath(char* out_path, size_t max_len) {
  char user_profile[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, user_profile, CSIDL_PROFILE, FALSE)) {
    snprintf(out_path, max_len, "%s\\.lite-browser", user_profile);
    CreateDirectoryA(out_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\history.json", user_profile);
  } else {
    snprintf(out_path, max_len, "C:\\projects\\lite_browser\\history.json");
  }
}

static void ExecuteJsOnBrowser(cef_browser_t* target_b, const char* js_call) {
  if (!target_b || !js_call) return;
  cef_frame_t* target_frame = target_b->get_main_frame(target_b);
  if (target_frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(js_call, strlen(js_call), &js_str);
    target_frame->execute_java_script(target_frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    target_frame->base.release(&target_frame->base);
  }
}

static void ResolveUIFilePath(const char* relative_path, char* out_file_path, size_t max_path_len, char* out_file_url, size_t max_url_len) {
#if defined(OS_WIN)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);

  char path[MAX_PATH];
  strcpy(path, exe_path);

  int found = 0;
  for (int i = 0; i < 8; i++) {
    char *last_backslash = strrchr(path, '\\');
    if (!last_backslash) break;
    *last_backslash = '\0';

    char test_path[MAX_PATH];
    snprintf(test_path, sizeof(test_path), "%s\\%s", path, relative_path);
    DWORD attrib = GetFileAttributesA(test_path);
    if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
      if (out_file_path && max_path_len > 0) {
        strncpy(out_file_path, test_path, max_path_len - 1);
        out_file_path[max_path_len - 1] = '\0';
      }
      if (out_file_url && max_url_len > 0) {
        snprintf(out_file_url, max_url_len, "file:///%s/%s", path, relative_path);
        for (size_t j = 8; out_file_url[j]; j++) {
          if (out_file_url[j] == '\\') {
            out_file_url[j] = '/';
          }
        }
      }
      found = 1;
      break;
    }
  }

  if (!found) {
    if (out_file_path && max_path_len > 0) {
      snprintf(out_file_path, max_path_len, "C:\\projects\\lite_browser\\%s", relative_path);
    }
    if (out_file_url && max_url_len > 0) {
      snprintf(out_file_url, max_url_len, "file:///C:/projects/lite_browser/%s", relative_path);
    }
  }
#else
  if (out_file_path && max_path_len > 0) {
    snprintf(out_file_path, max_path_len, "/projects/lite_browser/%s", relative_path);
  }
  if (out_file_url && max_url_len > 0) {
    snprintf(out_file_url, max_url_len, "file:///projects/lite_browser/%s", relative_path);
  }
#endif
}

static void scan_directory_recursive(const char* dir_path, char** json_ptr, size_t* len_ptr, size_t* cap_ptr, int is_first_in_level) {
  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
  
  WIN32_FIND_DATAA find_data;
  HANDLE hFind = FindFirstFileA(search_path, &find_data);
  if (hFind == INVALID_HANDLE_VALUE) return;
  
  int first_item = 1;
  do {
    if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
      continue;
    }
    
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
    
    int is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    
    if (!is_dir) {
      size_t name_len = strlen(find_data.cFileName);
      if (name_len < 3 || _stricmp(find_data.cFileName + name_len - 3, ".md") != 0) {
        continue;
      }
    }
    
    size_t needed = strlen(find_data.cFileName) + strlen(full_path) + 256;
    if (*len_ptr + needed >= *cap_ptr) {
      *cap_ptr *= 2;
      char* temp = (char*)realloc(*json_ptr, *cap_ptr);
      if (!temp) {
        FindClose(hFind);
        return;
      }
      *json_ptr = temp;
    }
    
    if (!is_first_in_level || !first_item) {
      strcat(*json_ptr, ",");
      *len_ptr = strlen(*json_ptr);
    }
    first_item = 0;
    
    char clean_path[MAX_PATH];
    strcpy(clean_path, full_path);
    for (int i = 0; clean_path[i]; i++) {
      if (clean_path[i] == '\\') {
        clean_path[i] = '/';
      }
    }
    
    char clean_name[MAX_PATH];
    strcpy(clean_name, find_data.cFileName);
    for (int i = 0; clean_name[i]; i++) {
      if (clean_name[i] == '"') clean_name[i] = '\'';
      if (clean_name[i] == '\\') clean_name[i] = '/';
    }
    
    if (is_dir) {
      snprintf(*json_ptr + *len_ptr, *cap_ptr - *len_ptr,
               "{\"name\":\"%s\",\"type\":\"directory\",\"path\":\"%s\",\"children\":[",
               clean_name, clean_path);
      *len_ptr = strlen(*json_ptr);
      
      scan_directory_recursive(full_path, json_ptr, len_ptr, cap_ptr, 1);
      
      strcat(*json_ptr, "]}");
      *len_ptr = strlen(*json_ptr);
    } else {
      snprintf(*json_ptr + *len_ptr, *cap_ptr - *len_ptr,
               "{\"name\":\"%s\",\"type\":\"file\",\"path\":\"%s\"}",
               clean_name, clean_path);
      *len_ptr = strlen(*json_ptr);
    }
  } while (FindNextFileA(hFind, &find_data));
  
  FindClose(hFind);
}
#endif

static void LogMsg(const char *format, ...) {
  FILE *f = fopen("C:\\projects\\lite_browser\\debug_c.txt", "a");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fclose(f);
  }
}

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const unsigned char* data, size_t input_length) {
  size_t output_length = 4 * ((input_length + 2) / 3);
  char* encoded_data = (char*)malloc(output_length + 1);
  if (encoded_data == NULL) return NULL;

  for (size_t i = 0, j = 0; i < input_length;) {
    size_t remaining = input_length - i;
    uint32_t octet_a = (unsigned char)data[i++];
    uint32_t octet_b = (remaining > 1) ? (unsigned char)data[i++] : 0;
    uint32_t octet_c = (remaining > 2) ? (unsigned char)data[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
    encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
    encoded_data[j++] = (remaining > 1) ? base64_table[(triple >> 6) & 0x3F] : '=';
    encoded_data[j++] = (remaining > 2) ? base64_table[triple & 0x3F] : '=';
  }
  encoded_data[output_length] = '\0';
  return encoded_data;
}

static int get_base64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static unsigned char* base64_decode(const char* src, size_t* out_len) {
  size_t len = strlen(src);
  if (len % 4 != 0) return NULL;

  size_t padding = 0;
  if (len > 0 && src[len - 1] == '=') padding++;
  if (len > 1 && src[len - 2] == '=') padding++;

  size_t decoded_len = (len / 4) * 3 - padding;
  unsigned char* out = (unsigned char*)malloc(decoded_len + 1);
  if (!out) return NULL;

  size_t i = 0, j = 0;
  while (i < len) {
    int a = get_base64_value(src[i++]);
    int b = get_base64_value(src[i++]);
    int c = get_base64_value(src[i++]);
    int d = get_base64_value(src[i++]);

    if (a < 0 || b < 0 || (c < 0 && src[i - 2] != '=') || (d < 0 && src[i - 1] != '=')) {
      free(out);
      return NULL;
    }

    uint32_t triple = (a << 18) + (b << 12) + ((c >= 0 ? c : 0) << 6) + (d >= 0 ? d : 0);

    if (j < decoded_len) out[j++] = (triple >> 16) & 0xFF;
    if (j < decoded_len) out[j++] = (triple >> 8) & 0xFF;
    if (j < decoded_len) out[j++] = triple & 0xFF;
  }

  out[decoded_len] = '\0';
  *out_len = decoded_len;
  return out;
}

static char* get_query_param(const char* query, const char* param_name) {
  size_t name_len = strlen(param_name);
  const char* p = query;
  while (p) {
    if (strncmp(p, param_name, name_len) == 0 && p[name_len] == '=') {
      const char* val_start = p + name_len + 1;
      const char* val_end = strchr(val_start, '&');
      size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

      char* decoded = (char*)malloc(val_len + 1);
      if (!decoded) return NULL;

      size_t i = 0, j = 0;
      while (i < val_len) {
        if (val_start[i] == '%' && i + 2 < val_len) {
          char hex[3] = {val_start[i + 1], val_start[i + 2], '\0'};
          decoded[j++] = (char)strtol(hex, NULL, 16);
          i += 3;
        } else if (val_start[i] == '+') {
          decoded[j++] = ' ';
          i++;
        } else {
          decoded[j++] = val_start[i];
          i++;
        }
      }
      decoded[j] = '\0';
      return decoded;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return NULL;
}

// Global instance pointer.
static simple_handler_t *g_instance = NULL;

// cef_browser_t *g_ui_browser = NULL;
// cef_browser_t *g_content_browser = NULL;
char g_startup_url[4096] = "lite://favorites";

// Forward declarations for handler create functions.
simple_display_handler_t *display_handler_create(simple_handler_t *parent);
simple_life_span_handler_t *life_span_handler_create(simple_handler_t *parent);
simple_load_handler_t *load_handler_create(simple_handler_t *parent);
simple_request_handler_t *request_handler_create(simple_handler_t *parent);
simple_context_menu_handler_t *context_menu_handler_create(simple_handler_t *parent);
simple_download_handler_t *download_handler_create(simple_handler_t *parent);

typedef struct _simple_focus_handler_t {
  cef_focus_handler_t handler;
  atomic_int ref_count;
  simple_handler_t *parent;
} simple_focus_handler_t;

simple_focus_handler_t *focus_handler_create(simple_handler_t *parent);
cef_focus_handler_t *CEF_CALLBACK simple_handler_get_focus_handler(cef_client_t *self);

//
// Client handler implementation.
//

IMPLEMENT_REFCOUNTING_MANUAL(simple_handler_t, simple_handler, ref_count)

int CEF_CALLBACK simple_handler_release(cef_base_ref_counted_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    // Release all handlers.
    if (handler->display_handler) {
      handler->display_handler->handler.base.release(
          &handler->display_handler->handler.base);
    }
    if (handler->life_span_handler) {
      handler->life_span_handler->handler.base.release(
          &handler->life_span_handler->handler.base);
    }
    if (handler->load_handler) {
      handler->load_handler->handler.base.release(
          &handler->load_handler->handler.base);
    }
    if (handler->request_handler) {
      handler->request_handler->handler.base.release(
          &handler->request_handler->handler.base);
    }
    if (handler->context_menu_handler) {
      handler->context_menu_handler->handler.base.release(
          &handler->context_menu_handler->handler.base);
    }
    if (handler->download_handler) {
      handler->download_handler->handler.base.release(
          &handler->download_handler->handler.base);
    }
    if (handler->focus_handler) {
      handler->focus_handler->handler.base.release(
          &handler->focus_handler->handler.base);
    }

    // Destroy the browser list.
    browser_list_destroy(&handler->browser_list);

    // Clear global instance if this is it.
    if (g_instance == handler) {
      g_instance = NULL;
    }

    free(handler);
    return 1;
  }
  return 0;
}

//
// Client handler getter implementations.
//

cef_download_handler_t *CEF_CALLBACK
simple_handler_get_download_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->download_handler) {
    // Add reference before returning.
    handler->download_handler->handler.base.add_ref(
        &handler->download_handler->handler.base);
    return &handler->download_handler->handler;
  }
  return NULL;
}

cef_context_menu_handler_t *CEF_CALLBACK
simple_handler_get_context_menu_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->context_menu_handler) {
    // Add reference before returning.
    handler->context_menu_handler->handler.base.add_ref(
        &handler->context_menu_handler->handler.base);
    return &handler->context_menu_handler->handler;
  }
  return NULL;
}

cef_display_handler_t *CEF_CALLBACK
simple_handler_get_display_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->display_handler) {
    // Add reference before returning.
    handler->display_handler->handler.base.add_ref(
        &handler->display_handler->handler.base);
    return &handler->display_handler->handler;
  }
  return NULL;
}

cef_life_span_handler_t *CEF_CALLBACK
simple_handler_get_life_span_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->life_span_handler) {
    // Add reference before returning.
    handler->life_span_handler->handler.base.add_ref(
        &handler->life_span_handler->handler.base);
    return &handler->life_span_handler->handler;
  }
  return NULL;
}

cef_load_handler_t *CEF_CALLBACK
simple_handler_get_load_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->load_handler) {
    // Add reference before returning.
    handler->load_handler->handler.base.add_ref(
        &handler->load_handler->handler.base);
    return &handler->load_handler->handler;
  }
  return NULL;
}

cef_request_handler_t *CEF_CALLBACK
simple_handler_get_request_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->request_handler) {
    // Add reference before returning.
    handler->request_handler->handler.base.add_ref(
        &handler->request_handler->handler.base);
    return &handler->request_handler->handler;
  }
  return NULL;
}

cef_focus_handler_t *CEF_CALLBACK
simple_handler_get_focus_handler(cef_client_t *self) {
  simple_handler_t *handler = (simple_handler_t *)self;
  if (handler->focus_handler) {
    handler->focus_handler->handler.base.add_ref(
        &handler->focus_handler->handler.base);
    return &handler->focus_handler->handler;
  }
  return NULL;
}

//
// Public API implementation.
//

simple_handler_t *simple_handler_create(int is_alloy_style) {
  simple_handler_t *handler =
      (simple_handler_t *)calloc(1, sizeof(simple_handler_t));
  CHECK(handler);

  // Initialize download manager state from disk once
  download_manager_init();

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->client.base, cef_client_t, simple_handler);

  // Set callbacks.
  handler->client.get_display_handler = simple_handler_get_display_handler;
  handler->client.get_life_span_handler = simple_handler_get_life_span_handler;
  handler->client.get_load_handler = simple_handler_get_load_handler;
  handler->client.get_request_handler = simple_handler_get_request_handler;
  handler->client.get_context_menu_handler = simple_handler_get_context_menu_handler;
  handler->client.get_download_handler = simple_handler_get_download_handler;
  handler->client.get_focus_handler = simple_handler_get_focus_handler;

  // Create sub-handlers.
  handler->display_handler = display_handler_create(handler);
  CHECK(handler->display_handler);
  handler->life_span_handler = life_span_handler_create(handler);
  CHECK(handler->life_span_handler);
  handler->load_handler = load_handler_create(handler);
  CHECK(handler->load_handler);
  handler->request_handler = request_handler_create(handler);
  CHECK(handler->request_handler);
  handler->context_menu_handler = context_menu_handler_create(handler);
  CHECK(handler->context_menu_handler);
  handler->download_handler = download_handler_create(handler);
  CHECK(handler->download_handler);
  handler->focus_handler = focus_handler_create(handler);
  CHECK(handler->focus_handler);

  // Initialize other fields.
  handler->is_alloy_style = is_alloy_style;
  browser_list_init(&handler->browser_list);
  handler->is_closing = 0;
  handler->type = BROWSER_TYPE_CONTENT;
 

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  // Set global instance.
  if (!g_instance) {
    g_instance = handler;
  }

  return handler;
}

simple_handler_t *simple_handler_get_instance(void) { return g_instance; }

// Task for closing browsers on the UI thread.
typedef struct _close_browsers_task_t {
  cef_task_t task;
  atomic_int ref_count;
  simple_handler_t *handler;
  int force_close;
} close_browsers_task_t;

IMPLEMENT_REFCOUNTING_MANUAL(close_browsers_task_t, close_browsers_task,
                             ref_count)

int CEF_CALLBACK close_browsers_task_release(cef_base_ref_counted_t *self) {
  close_browsers_task_t *task = (close_browsers_task_t *)self;
  int count = atomic_fetch_sub(&task->ref_count, 1) - 1;
  if (count == 0) {
    // Don't release handler reference - we don't own it.
    free(task);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK close_browsers_task_execute(cef_task_t *self) {
  close_browsers_task_t *task = (close_browsers_task_t *)self;

  size_t count = browser_list_count(&task->handler->browser_list);
  if (count == 0) {
    return;
  }

  // Close all browsers.
  for (size_t i = 0; i < count; ++i) {
    cef_browser_t *browser = browser_list_get(&task->handler->browser_list, i);
    cef_browser_host_t *host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, task->force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_close_all_browsers(simple_handler_t *handler,
                                       int force_close) {
  CHECK(handler);

  if (!cef_currently_on(TID_UI)) {
    // Execute on the UI thread.
    close_browsers_task_t *task =
        (close_browsers_task_t *)calloc(1, sizeof(close_browsers_task_t));
    CHECK(task);

    INIT_CEF_BASE_REFCOUNTED(&task->task.base, cef_task_t, close_browsers_task);
    task->task.execute = close_browsers_task_execute;
    task->handler = handler;
    task->force_close = force_close;
    atomic_store(&task->ref_count, 1);

    cef_post_task(TID_UI, &task->task);
    return;
  }

  // Already on UI thread, execute directly.
  size_t count = browser_list_count(&handler->browser_list);
  if (count == 0) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    cef_browser_t *browser = browser_list_get(&handler->browser_list, i);
    cef_browser_host_t *host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_show_main_window(simple_handler_t *handler) {
  CHECK(handler);
  if (browser_list_count(&handler->browser_list) == 0) {
    return;
  }

  cef_browser_t *main_browser = browser_list_get(&handler->browser_list, 0);
  simple_handler_platform_show_window(handler, main_browser);
}

// Default platform implementations (can be overridden in platform-specific
// files).
#if !defined(OS_MAC)
void simple_handler_platform_show_window(simple_handler_t *handler,
                                         cef_browser_t *browser) {
  // Not implemented on this platform.
}
#endif

static void EscapeJsonString(const char* src, char* dest, size_t dest_len) {
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j < dest_len - 3; i++) {
    if (src[i] == '"') {
      dest[j++] = '\\';
      dest[j++] = '"';
    } else if (src[i] == '\\') {
      dest[j++] = '\\';
      dest[j++] = '\\';
    } else if (src[i] == '\n') {
      dest[j++] = '\\';
      dest[j++] = 'n';
    } else if (src[i] == '\r') {
      dest[j++] = '\\';
      dest[j++] = 'r';
    } else if (src[i] == '\t') {
      dest[j++] = '\\';
      dest[j++] = 't';
    } else {
      dest[j++] = src[i];
    }
  }
  dest[j] = '\0';
}

void update_ui_tabs(browser_window_t* win_ctx) {
  if (!win_ctx || !win_ctx->ui_browser) return;

  char json[65536] = "[";
  for (int i = 0; i < win_ctx->tab_count; i++) {
    char escaped_title[1024] = {0};
    char escaped_url[4096] = {0};
    char escaped_favicon[2048] = {0};

    if (win_ctx->tabs[i].is_split) {
      const char* t1 = win_ctx->tabs[i].title[0] ? win_ctx->tabs[i].title : "새 탭";
      const char* t2 = win_ctx->tabs[i].right_title[0] ? win_ctx->tabs[i].right_title : "북마크 관리자";
      char combo_title[1024];
      snprintf(combo_title, sizeof(combo_title), "%s | %s", t1, t2);
      EscapeJsonString(combo_title, escaped_title, sizeof(escaped_title));
    } else {
      EscapeJsonString(win_ctx->tabs[i].title, escaped_title, sizeof(escaped_title));
    }
    EscapeJsonString(win_ctx->tabs[i].url, escaped_url, sizeof(escaped_url));
    EscapeJsonString(win_ctx->tabs[i].favicon_url, escaped_favicon, sizeof(escaped_favicon));

    int is_loading = 0;
    if (win_ctx->tabs[i].browser) {
      is_loading = win_ctx->tabs[i].browser->is_loading(win_ctx->tabs[i].browser);
    }

    char tab_str[6000];
    snprintf(tab_str, sizeof(tab_str), 
             "{\"id\":%d,\"title\":\"%s\",\"url\":\"%s\",\"favicon\":\"%s\",\"is_loading\":%d,\"is_split\":%d}%s", 
             win_ctx->tabs[i].tab_id, 
             escaped_title, 
             escaped_url,
             escaped_favicon,
             is_loading,
             win_ctx->tabs[i].is_split,
             (i == win_ctx->tab_count - 1) ? "" : ",");
    
    if (strlen(json) + strlen(tab_str) < sizeof(json) - 5) {
      strcat(json, tab_str);
    }
  }
  strcat(json, "]");

  char js_code[70000];
  snprintf(js_code, sizeof(js_code), "if (window.updateTabsList) { window.updateTabsList(%s, %d); }", 
           json, 
           (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) ? 
           win_ctx->tabs[win_ctx->active_tab_index].tab_id : -1);

  cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
  if (frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
    frame->execute_java_script(frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    frame->base.release(&frame->base);
  }
}

void update_ui_nav_state(browser_window_t* win_ctx) {
  if (!win_ctx || win_ctx->active_tab_index < 0 || win_ctx->active_tab_index >= win_ctx->tab_count) return;

  tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
  cef_browser_t* cb = NULL;
  const char* active_url = active_tab->url;

  if (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_browser) {
    cb = active_tab->right_browser;
    active_url = active_tab->right_url;
  } else {
    cb = active_tab->browser;
    active_url = active_tab->url;
  }

  if (!cb || !win_ctx->ui_browser) return;

  int can_go_back = cb->can_go_back(cb);
  int can_go_forward = cb->can_go_forward(cb);
  int is_loading = cb->is_loading(cb);

  char escaped_url[4096] = {0};
  EscapeJsonString(active_url, escaped_url, sizeof(escaped_url));

  char js_code[5000];
  snprintf(js_code, sizeof(js_code), 
           "if (window.updateNavState) { window.updateNavState(%d, %d, %d); } "
           "if (window.updateAddress) { window.updateAddress(\"%s\"); } "
           "if (window.updateDualSplitState) { window.updateDualSplitState(%d, %d); } "
           "if (window.updateSidepanelState) { window.updateSidepanelState(%d); }", 
           can_go_back, can_go_forward, is_loading, escaped_url,
           active_tab->is_split, active_tab->active_split,
           win_ctx->show_sidepanel);

  cef_frame_t* frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
  if (frame) {
    cef_string_t js_str = {};
    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
    frame->execute_java_script(frame, &js_str, NULL, 0);
    cef_string_clear(&js_str);
    frame->base.release(&frame->base);
  }
}

static void RemoveTabAt(browser_window_t* win_ctx, int remove_idx, int close_cef_browser) {
  if (!win_ctx || remove_idx < 0 || remove_idx >= win_ctx->tab_count) return;

  if (win_ctx->tab_count <= 1) {
    if (close_cef_browser) {
      PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
    }
    return;
  }

  if (close_cef_browser) {
    cef_browser_t* target_browser = win_ctx->tabs[remove_idx].browser;
    if (target_browser) {
      cef_browser_host_t* host = target_browser->get_host(target_browser);
      if (host) {
        host->close_browser(host, 1);
        host->base.release(&host->base);
      }
    }
  }

  int old_active = win_ctx->active_tab_index;
  int new_active = old_active;

  if (old_active == remove_idx) {
    new_active = (remove_idx > 0) ? (remove_idx - 1) : 0;
  } else if (old_active > remove_idx) {
    new_active = old_active - 1;
  }

  for (int i = remove_idx; i < win_ctx->tab_count - 1; i++) {
    win_ctx->tabs[i] = win_ctx->tabs[i + 1];
  }
  win_ctx->tab_count--;
  win_ctx->active_tab_index = new_active;

  for (int k = 0; k < win_ctx->tab_count; k++) {
    if (k != new_active) {
      if (win_ctx->tabs[k].hwnd) ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
      if (win_ctx->tabs[k].right_hwnd) ShowWindow(win_ctx->tabs[k].right_hwnd, SW_HIDE);
    }
  }

  win_ctx->tabs[new_active].is_loaded = 1;
  if (win_ctx->tabs[new_active].hwnd) {
    ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
    RECT rect;
    GetClientRect(win_ctx->main_hwnd, &rect);
    int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
    int content_y = ui_height + 1;
    int content_h = rect.bottom - content_y - 1;
    MoveWindow(win_ctx->tabs[new_active].hwnd, 1, content_y, rect.right - 2, content_h, TRUE);
    PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
    if (win_ctx->tabs[new_active].browser) {
      cef_browser_host_t* host = win_ctx->tabs[new_active].browser->get_host(win_ctx->tabs[new_active].browser);
      if (host) {
        host->was_resized(host);
        host->set_focus(host, 1);
        host->base.release(&host->base);
      }
    }
  }

  update_ui_tabs(win_ctx);
  update_ui_nav_state(win_ctx);
}

static void CloseTab(browser_window_t* win_ctx, int target_id) {
  int found_idx = -1;
  for (int i = 0; i < win_ctx->tab_count; i++) {
    if (win_ctx->tabs[i].tab_id == target_id) {
      found_idx = i;
      break;
    }
  }
  if (found_idx != -1) {
    RemoveTabAt(win_ctx, found_idx, 1);
  }
}

static void CloseOtherTabs(browser_window_t* win_ctx, int keep_id) {
  int keep_idx = -1;
  for (int i = 0; i < win_ctx->tab_count; i++) {
    if (win_ctx->tabs[i].tab_id == keep_id) {
      keep_idx = i;
      break;
    }
  }
  if (keep_idx == -1) return;

  for (int i = 0; i < win_ctx->tab_count; i++) {
    if (i != keep_idx) {
      cef_browser_t* target_browser = win_ctx->tabs[i].browser;
      if (target_browser) {
        cef_browser_host_t* host = target_browser->get_host(target_browser);
        if (host) {
          host->close_browser(host, 1);
          host->base.release(&host->base);
        }
      }
    }
  }

  tab_info_t keep_tab = win_ctx->tabs[keep_idx];
  win_ctx->tabs[0] = keep_tab;
  win_ctx->tab_count = 1;
  win_ctx->active_tab_index = 0;

  if (win_ctx->tabs[0].hwnd) {
    ShowWindow(win_ctx->tabs[0].hwnd, SW_SHOW);
    RECT rect;
    GetClientRect(win_ctx->main_hwnd, &rect);
    PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
    cef_browser_host_t* host = win_ctx->tabs[0].browser->get_host(win_ctx->tabs[0].browser);
    if (host) {
      host->was_resized(host);
      host->set_focus(host, 1);
      host->base.release(&host->base);
    }
  }

  update_ui_tabs(win_ctx);
  update_ui_nav_state(win_ctx);
}

static void CloseTabsToRight(browser_window_t* win_ctx, int target_id) {
  int target_idx = -1;
  for (int i = 0; i < win_ctx->tab_count; i++) {
    if (win_ctx->tabs[i].tab_id == target_id) {
      target_idx = i;
      break;
    }
  }
  if (target_idx == -1 || target_idx >= win_ctx->tab_count - 1) return;

  for (int i = target_idx + 1; i < win_ctx->tab_count; i++) {
    cef_browser_t* target_browser = win_ctx->tabs[i].browser;
    if (target_browser) {
      cef_browser_host_t* host = target_browser->get_host(target_browser);
      if (host) {
        host->close_browser(host, 1);
        host->base.release(&host->base);
      }
    }
  }

  int old_active = win_ctx->active_tab_index;
  int new_active = old_active;
  if (old_active > target_idx) {
    new_active = target_idx;
    if (win_ctx->tabs[new_active].hwnd) {
      ShowWindow(win_ctx->tabs[new_active].hwnd, SW_SHOW);
      RECT rect;
      GetClientRect(win_ctx->main_hwnd, &rect);
      PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));
      cef_browser_host_t* host = win_ctx->tabs[new_active].browser->get_host(win_ctx->tabs[new_active].browser);
      if (host) {
        host->was_resized(host);
        host->set_focus(host, 1);
        host->base.release(&host->base);
      }
    }
  }

  win_ctx->tab_count = target_idx + 1;
  win_ctx->active_tab_index = new_active;

  update_ui_tabs(win_ctx);
  update_ui_nav_state(win_ctx);
}

//
// Request handler implementation.
//
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_request_handler_t, request_handler,
                             ref_count)

int CEF_CALLBACK request_handler_on_before_browse(
    cef_request_handler_t *self, cef_browser_t *browser, cef_frame_t *frame,
    cef_request_t *request, int user_gesture, int is_redirect) {

  cef_string_userfree_t url_userfree = request->get_url(request);
  if (url_userfree) {
    cef_string_utf8_t url_utf8 = {};
    cef_string_to_utf8(url_userfree->str, url_userfree->length, &url_utf8);

    if (url_utf8.str) {
      if (strncmp(url_utf8.str, "lite://favorites", 16) == 0 ||
          strncmp(url_utf8.str, "lite://bookmarks", 16) == 0 ||
          strncmp(url_utf8.str, "edge://favorites", 16) == 0 ||
          strncmp(url_utf8.str, "chrome://favorites", 18) == 0) {
        char mgr_url_buf[MAX_PATH + 32];
        ResolveUIFilePath("ui/manager.html", NULL, 0, mgr_url_buf, sizeof(mgr_url_buf));
        cef_string_t mgr_url = {};
        cef_string_from_utf8(mgr_url_buf, strlen(mgr_url_buf), &mgr_url);
        frame->load_url(frame, &mgr_url);
        cef_string_clear(&mgr_url);
        cef_string_utf8_clear(&url_utf8);
        cef_string_userfree_free(url_userfree);
        return 1;
      } else if (strncmp(url_utf8.str, "lite://downloads", 16) == 0 ||
                 strncmp(url_utf8.str, "edge://downloads", 16) == 0 ||
                 strncmp(url_utf8.str, "chrome://downloads", 18) == 0) {
        char dl_url_buf[MAX_PATH + 32];
        ResolveUIFilePath("ui/downloads.html", NULL, 0, dl_url_buf, sizeof(dl_url_buf));
        cef_string_t dl_url = {};
        cef_string_from_utf8(dl_url_buf, strlen(dl_url_buf), &dl_url);
        frame->load_url(frame, &dl_url);
        cef_string_clear(&dl_url);
        cef_string_utf8_clear(&url_utf8);
        cef_string_userfree_free(url_userfree);
        return 1;
      } else if (strncmp(url_utf8.str, "lite://sidepanel", 16) == 0 ||
                 strncmp(url_utf8.str, "lite://ai", 9) == 0 ||
                 strncmp(url_utf8.str, "lite://agent", 12) == 0) {
        char sp_url_buf[MAX_PATH + 32];
        ResolveUIFilePath("ui/sidepanel.html", NULL, 0, sp_url_buf, sizeof(sp_url_buf));
        cef_string_t sp_url = {};
        cef_string_from_utf8(sp_url_buf, strlen(sp_url_buf), &sp_url);
        frame->load_url(frame, &sp_url);
        cef_string_clear(&sp_url);
        cef_string_utf8_clear(&url_utf8);
        cef_string_userfree_free(url_userfree);
        return 1;
      }
    }

    if (url_utf8.str && strncmp(url_utf8.str, "http://ui-action/", 17) == 0) {
      const char *action = url_utf8.str + 17;
      LogMsg("Interrupted ui-action: %s\n", action);

      simple_request_handler_t *req_handler = (simple_request_handler_t*)self;
      simple_handler_t *parent_handler = req_handler->parent;
      browser_window_t *win_ctx = parent_handler->window_ctx;

      if (win_ctx) {
        cef_browser_t *cb = NULL;
        if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
          tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
          if (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_browser) {
            cb = active_tab->right_browser;
          } else {
            cb = active_tab->browser;
          }
        }

        if (strcmp(action, "toggle-ai-sidepanel") == 0) {
          win_ctx->show_sidepanel = !win_ctx->show_sidepanel;
          RECT r;
          GetClientRect(win_ctx->main_hwnd, &r);
          PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
          update_ui_nav_state(win_ctx);
        } else if (strcmp(action, "toggle-dual-split") == 0) {
          if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
            tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
            if (!active_tab->is_split) {
              CreateRightSplitBrowser(win_ctx, active_tab, "lite://favorites");
            } else {
              if (active_tab->active_split == 1 && active_tab->right_url[0]) {
                if (active_tab->browser) {
                  cef_frame_t* f = active_tab->browser->get_main_frame(active_tab->browser);
                  if (f) {
                    cef_string_t u = {};
                    cef_string_from_utf8(active_tab->right_url, strlen(active_tab->right_url), &u);
                    f->load_url(f, &u);
                    cef_string_clear(&u);
                    f->base.release(&f->base);
                  }
                }
              }
              if (active_tab->right_browser) {
                cef_browser_host_t* host = active_tab->right_browser->get_host(active_tab->right_browser);
                if (host) {
                  host->close_browser(host, 1);
                  host->base.release(&host->base);
                }
                if (active_tab->right_hwnd) ShowWindow(active_tab->right_hwnd, SW_HIDE);
                active_tab->right_browser = NULL;
                active_tab->right_hwnd = NULL;
                active_tab->right_tab_handler = NULL;
              }
              active_tab->is_split = 0;
              active_tab->active_split = 0;
              RECT r;
              GetClientRect(win_ctx->main_hwnd, &r);
              PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
              update_ui_nav_state(win_ctx);
            }
          }
        } else if (strcmp(action, "vault-get-list") == 0) {
          char buf[65536];
          vault_get_list_json(buf, sizeof(buf));
          if (frame) {
            char js_code[65536 + 128];
            snprintf(js_code, sizeof(js_code), "if (window.renderVaultList) { window.renderVaultList(%s); }", buf);
            cef_string_t js_str = {};
            cef_string_from_utf8(js_code, strlen(js_code), &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "vault-save?", 11) == 0) {
          const char* query = action + 11;
          char* domain = get_query_param(query, "domain");
          char* user = get_query_param(query, "user");
          char* pass = get_query_param(query, "pass");
          if (domain && pass) {
            vault_save_credential(domain, user ? user : "", pass);
          }
          if (domain) free(domain);
          if (user) free(user);
          if (pass) free(pass);
          if (frame) {
            cef_string_t js_str = {};
            cef_string_from_utf8("if (window.onVaultUpdated) window.onVaultUpdated(true);", 55, &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "vault-delete?", 13) == 0) {
          const char* query = action + 13;
          char* domain = get_query_param(query, "domain");
          if (domain) {
            vault_delete_credential(domain);
            free(domain);
          }
          if (frame) {
            cef_string_t js_str = {};
            cef_string_from_utf8("if (window.onVaultUpdated) window.onVaultUpdated(true);", 55, &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strcmp(action, "vault-clear") == 0) {
          vault_clear_all();
          if (frame) {
            cef_string_t js_str = {};
            cef_string_from_utf8("if (window.onVaultUpdated) window.onVaultUpdated(true);", 55, &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "vault-autofill?", 15) == 0 || strcmp(action, "vault-autofill") == 0) {
          const char* query = (strncmp(action, "vault-autofill?", 15) == 0) ? (action + 15) : "";
          char* domain = get_query_param(query, "domain");
          int success = 0;
          if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
            tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
            const char* active_pane_url = (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_url[0]) ? active_tab->right_url : active_tab->url;
            const char* target_dom = (domain && strlen(domain) > 0) ? domain : active_pane_url;
            if (cb) {
              success = vault_execute_autofill(cb, target_dom);
            }
          }
          if (domain) free(domain);
          if (frame) {
            char js_code[128];
            snprintf(js_code, sizeof(js_code), "if (window.onVaultAutofillResult) window.onVaultAutofillResult(%s);", success ? "true" : "false");
            cef_string_t js_str = {};
            cef_string_from_utf8(js_code, strlen(js_code), &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strcmp(action, "auth-get-status") == 0) {
          char buf[8192];
          auth_get_status_json(buf, sizeof(buf));
          if (frame) {
            char js_code[8192 + 128];
            snprintf(js_code, sizeof(js_code), "if (window.renderAuthStatus) { window.renderAuthStatus(%s); }", buf);
            cef_string_t js_str = {};
            cef_string_from_utf8(js_code, strlen(js_code), &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "auth-save-session?", 18) == 0) {
          const char* query = action + 18;
          char* provider = get_query_param(query, "provider");
          char* email = get_query_param(query, "email");
          char* tier = get_query_param(query, "tier");
          char* access_token = get_query_param(query, "access_token");
          char* refresh_token = get_query_param(query, "refresh_token");
          char* exp_str = get_query_param(query, "expires_at");
          long expires_at = exp_str ? atol(exp_str) : 0;

          if (provider && access_token) {
            auth_save_session(provider, email ? email : "", tier ? tier : "",
                              access_token, refresh_token ? refresh_token : "", expires_at);
          }
          if (provider) free(provider);
          if (email) free(email);
          if (tier) free(tier);
          if (access_token) free(access_token);
          if (refresh_token) free(refresh_token);
          if (exp_str) free(exp_str);

          if (win_ctx->sidepanel_browser) {
            cef_frame_t* sf = win_ctx->sidepanel_browser->get_main_frame(win_ctx->sidepanel_browser);
            if (sf) {
              cef_string_t js_str = {};
              cef_string_from_utf8("if (window.onAuthUpdated) window.onAuthUpdated(true);", 53, &js_str);
              sf->execute_java_script(sf, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              sf->base.release(&sf->base);
            }
          }
          if (frame) {
            cef_string_t js_str = {};
            cef_string_from_utf8("if (window.onAuthUpdated) window.onAuthUpdated(true);", 53, &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "auth-delete-session?", 20) == 0) {
          const char* query = action + 20;
          char* provider = get_query_param(query, "provider");
          if (provider) {
            auth_delete_session(provider);
            free(provider);
          }
          if (win_ctx->sidepanel_browser) {
            cef_frame_t* sf = win_ctx->sidepanel_browser->get_main_frame(win_ctx->sidepanel_browser);
            if (sf) {
              cef_string_t js_str = {};
              cef_string_from_utf8("if (window.onAuthUpdated) window.onAuthUpdated(true);", 53, &js_str);
              sf->execute_java_script(sf, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              sf->base.release(&sf->base);
            }
          }
          if (frame) {
            cef_string_t js_str = {};
            cef_string_from_utf8("if (window.onAuthUpdated) window.onAuthUpdated(true);", 53, &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
        } else if (strncmp(action, "auth-get-token?", 15) == 0) {
          const char* query = action + 15;
          char* provider = get_query_param(query, "provider");
          char token_buf[4096] = {0};
          int ok = 0;
          if (provider) {
            ok = auth_get_token(provider, token_buf, sizeof(token_buf));
          }
          if (frame) {
            char js_code[4096 + 256];
            snprintf(js_code, sizeof(js_code),
                     "if (window.onAuthTokenReceived) { window.onAuthTokenReceived('%s', '%s', %s); }",
                     provider ? provider : "", ok ? token_buf : "", ok ? "true" : "false");
            cef_string_t js_str = {};
            cef_string_from_utf8(js_code, strlen(js_code), &js_str);
            frame->execute_java_script(frame, &js_str, NULL, 0);
            cef_string_clear(&js_str);
          }
          if (provider) free(provider);
        } else if (strncmp(action, "auth-login?", 11) == 0) {
          const char* query = action + 11;
          char* provider = get_query_param(query, "provider");
          if (provider) {
            char login_url[2048] = {0};
            auth_get_login_url(provider, login_url, sizeof(login_url));
            if (login_url[0]) {
              CreateNewTab(win_ctx, login_url);
            }
            free(provider);
          }
        } else if (strncmp(action, "ai-highlight-element?", 21) == 0) {
          const char* query = action + 21;
          char* selector = get_query_param(query, "selector");
          if (selector && cb) {
            char script[2048];
            snprintf(script, sizeof(script),
              "(function() {"
              "  try {"
              "    const old = document.getElementById('__lite_ai_highlight');"
              "    if (old) old.remove();"
              "    const el = document.querySelector('%s');"
              "    if (el) {"
              "      el.scrollIntoView({ behavior: 'smooth', block: 'center' });"
              "      const rect = el.getBoundingClientRect();"
              "      const ring = document.createElement('div');"
              "      ring.id = '__lite_ai_highlight';"
              "      ring.style.position = 'absolute';"
              "      ring.style.top = (rect.top + window.scrollY - 3) + 'px';"
              "      ring.style.left = (rect.left + window.scrollX - 3) + 'px';"
              "      ring.style.width = (rect.width + 6) + 'px';"
              "      ring.style.height = (rect.height + 6) + 'px';"
              "      ring.style.border = '3px solid #2563eb';"
              "      ring.style.borderRadius = '6px';"
              "      ring.style.boxShadow = '0 0 15px rgba(37, 99, 235, 0.6)';"
              "      ring.style.pointerEvents = 'none';"
              "      ring.style.zIndex = '9999999';"
              "      ring.style.transition = 'all 0.3s ease';"
              "      document.body.appendChild(ring);"
              "      setTimeout(() => { if (ring.parentNode) ring.parentNode.removeChild(ring); }, 3000);"
              "    }"
              "  } catch(e) {}"
              "})();", selector);
            cef_frame_t* f = cb->get_main_frame(cb);
            if (f) {
              cef_string_t js_str = {};
              cef_string_from_utf8(script, strlen(script), &js_str);
              f->execute_java_script(f, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              f->base.release(&f->base);
            }
            free(selector);
          }
        } else if (strncmp(action, "ai-click-element?", 17) == 0) {
          const char* query = action + 17;
          char* selector = get_query_param(query, "selector");
          char* text = get_query_param(query, "text");
          if (cb) {
            char script[2048];
            snprintf(script, sizeof(script),
              "(function() {"
              "  try {"
              "    let el = null;"
              "    const sel = '%s';"
              "    const txt = '%s';"
              "    if (sel && sel.length > 0) el = document.querySelector(sel);"
              "    if (!el && txt && txt.length > 0) {"
              "      const all = Array.from(document.querySelectorAll('button, a, input[type=\"submit\"], [role=\"button\"]'));"
              "      el = all.find(e => (e.innerText || e.value || '').trim().includes(txt));"
              "    }"
              "    if (el) {"
              "      el.scrollIntoView({ behavior: 'smooth', block: 'center' });"
              "      el.focus();"
              "      el.dispatchEvent(new MouseEvent('mousedown', { bubbles: true, cancelable: true }));"
              "      el.dispatchEvent(new MouseEvent('mouseup', { bubbles: true, cancelable: true }));"
              "      el.click();"
              "      return true;"
              "    }"
              "    return false;"
              "  } catch(e) { return false; }"
              "})();", selector ? selector : "", text ? text : "");
            cef_frame_t* f = cb->get_main_frame(cb);
            if (f) {
              cef_string_t js_str = {};
              cef_string_from_utf8(script, strlen(script), &js_str);
              f->execute_java_script(f, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              f->base.release(&f->base);
            }
          }
          if (selector) free(selector);
          if (text) free(text);
        } else if (strncmp(action, "ai-type-element?", 16) == 0) {
          const char* query = action + 16;
          char* selector = get_query_param(query, "selector");
          char* text = get_query_param(query, "text");
          char* enter = get_query_param(query, "enter");
          if (selector && text && cb) {
            char script[4096];
            snprintf(script, sizeof(script),
              "(function() {"
              "  try {"
              "    const el = document.querySelector('%s');"
              "    if (el) {"
              "      el.scrollIntoView({ behavior: 'smooth', block: 'center' });"
              "      el.focus();"
              "      if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {"
              "        el.value = '%s';"
              "      } else if (el.isContentEditable) {"
              "        el.innerText = '%s';"
              "      }"
              "      el.dispatchEvent(new Event('input', { bubbles: true }));"
              "      el.dispatchEvent(new Event('change', { bubbles: true }));"
              "      if (%s) {"
              "        el.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', keyCode: 13, which: 13, bubbles: true }));"
              "        el.dispatchEvent(new KeyboardEvent('keypress', { key: 'Enter', keyCode: 13, which: 13, bubbles: true }));"
              "        el.dispatchEvent(new KeyboardEvent('keyup', { key: 'Enter', keyCode: 13, which: 13, bubbles: true }));"
              "        if (el.form) el.form.submit();"
              "      }"
              "    }"
              "  } catch(e) {}"
              "})();", selector, text, text, (enter && strcmp(enter, "1") == 0) ? "true" : "false");
            cef_frame_t* f = cb->get_main_frame(cb);
            if (f) {
              cef_string_t js_str = {};
              cef_string_from_utf8(script, strlen(script), &js_str);
              f->execute_java_script(f, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              f->base.release(&f->base);
            }
          }
          if (selector) free(selector);
          if (text) free(text);
          if (enter) free(enter);
        } else if (strncmp(action, "ai-scroll?", 10) == 0) {
          const char* query = action + 10;
          char* dir = get_query_param(query, "direction");
          char* amt = get_query_param(query, "amount");
          int scroll_amt = amt ? atoi(amt) : 500;
          if (cb) {
            char script[256];
            if (dir && strcmp(dir, "up") == 0) {
              snprintf(script, sizeof(script), "window.scrollBy({ top: -%d, behavior: 'smooth' });", scroll_amt);
            } else if (dir && strcmp(dir, "top") == 0) {
              snprintf(script, sizeof(script), "window.scrollTo({ top: 0, behavior: 'smooth' });");
            } else if (dir && strcmp(dir, "bottom") == 0) {
              snprintf(script, sizeof(script), "window.scrollTo({ top: document.body.scrollHeight, behavior: 'smooth' });");
            } else {
              snprintf(script, sizeof(script), "window.scrollBy({ top: %d, behavior: 'smooth' });", scroll_amt);
            }
            cef_frame_t* f = cb->get_main_frame(cb);
            if (f) {
              cef_string_t js_str = {};
              cef_string_from_utf8(script, strlen(script), &js_str);
              f->execute_java_script(f, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              f->base.release(&f->base);
            }
          }
          if (dir) free(dir);
          if (amt) free(amt);
        } else if (strcmp(action, "ai-get-dom-summary") == 0) {
          if (cb) {
            const char* dom_collector = 
              "(function() {"
              "  try {"
              "    function getMeta(doc, prop) {"
              "      if (!doc) return '';"
              "      const el = doc.querySelector('meta[property=\"' + prop + '\"], meta[name=\"' + prop + '\"], meta[itemprop=\"' + prop + '\"]');"
              "      return el ? (el.getAttribute('content') || '').trim() : '';"
              "    }"
              "    function isVisible(el, win) {"
              "      if (!el || el.nodeType !== 1) return true;"
              "      try {"
              "        const s = (win || window).getComputedStyle(el);"
              "        if (s.display === 'none' || s.visibility === 'hidden' || parseFloat(s.opacity || '1') < 0.05) return false;"
              "        const r = el.getBoundingClientRect();"
              "        if (r.width === 0 && r.height === 0 && el.children.length === 0) return false;"
              "        return true;"
              "      } catch(e) { return true; }"
              "    }"
              "    function findBestDoc() {"
              "      const candidates = [];"
              "      function evalDoc(d, w, isIframe) {"
              "        if (!d) return;"
              "        try {"
              "          let score = 0;"
              "          const textLen = (d.body ? (d.body.innerText || '').trim().length : 0);"
              "          score += Math.min(textLen, 5000);"
              "          const selList = ["
              "            '.se-main-container', '.se_component_wrap', '#postViewArea', '.post-view',"
              "            '#dic_area', '#articleBody', '.article_view', '.news_body',"
              "            '.entry-content', '.post-content', '.article-body', '.article_body',"
              "            'article', 'main', '[role=\"main\"]', '#content', '.content-body', '#article'"
              "          ];"
              "          for (let s of selList) {"
              "            const el = d.querySelector(s);"
              "            if (el && el.innerText && el.innerText.trim().length > 100) {"
              "              score += 3000;"
              "              break;"
              "            }"
              "          }"
              "          if (isIframe && textLen > 200) score += 2000;"
              "          candidates.push({ doc: d, win: w, score: score });"
              "          const ifrs = d.querySelectorAll('iframe, frame');"
              "          for (let i = 0; i < ifrs.length; i++) {"
              "            try {"
              "              const idoc = ifrs[i].contentDocument || (ifrs[i].contentWindow && ifrs[i].contentWindow.document);"
              "              const iwin = ifrs[i].contentWindow;"
              "              if (idoc) evalDoc(idoc, iwin, true);"
              "            } catch(e) {}"
              "          }"
              "        } catch(e) {}"
              "      }"
              "      evalDoc(document, window, false);"
              "      candidates.sort((a, b) => b.score - a.score);"
              "      return candidates.length > 0 ? candidates[0] : { doc: document, win: window };"
              "    }"
              "    if (window.location.hostname.includes('youtube.com') && window.location.pathname.includes('/watch')) {"
              "      const ytTitle = (document.querySelector('h1.ytd-watch-metadata yt-formatted-string, #title h1, h1.title')?.innerText || document.title || '').trim();"
              "      const ytAuthor = (document.querySelector('#channel-name, ytd-channel-name, #owner')?.innerText || '').trim();"
              "      const ytDesc = (document.querySelector('#description-inline-expander, #description, ytd-text-inline-expander')?.innerText || '').trim();"
              "      const ytComments = Array.from(document.querySelectorAll('ytd-comment-thread-renderer, ytd-comment-view-model'));"
              "      let commentsList = [];"
              "      ytComments.slice(0, 50).forEach(c => {"
              "        const a = (c.querySelector('#author-text, #header-author, .ytd-comment-view-model #author-text')?.innerText || '').trim().replace(/^@/, '');"
              "        const t = (c.querySelector('#content-text, yt-attributed-string#content-text, .yt-core-attributed-string')?.innerText || '').trim();"
              "        const l = (c.querySelector('#vote-count-middle, .ytd-comment-action-buttons-renderer span.yt-core-attributed-string')?.innerText || '').trim();"
              "        if (t) {"
              "          commentsList.push('- **' + (a || '시청자') + '**' + (l ? ' [좋아요 ' + l + ']' : '') + ': ' + t);"
              "        }"
              "      });"
              "      let ytMd = '# 🎬 ' + ytTitle + '\\n\\n';"
              "      if (ytAuthor) ytMd += '- **채널**: ' + ytAuthor + '\\n';"
              "      ytMd += '- **URL**: ' + window.location.href + '\\n\\n';"
              "      if (ytDesc) ytMd += '### 📝 동영상 상세 설명\\n' + ytDesc.slice(0, 2000) + '\\n\\n';"
              "      if (commentsList.length > 0) {"
              "        ytMd += '### 💬 시청자 댓글 목록 (' + commentsList.length + '개 로딩됨)\\n' + commentsList.join('\\n\\n') + '\\n\\n';"
              "      } else {"
              "        const cnt = (document.querySelector('ytd-comments-header-renderer #count, yt-formatted-string.count-text')?.innerText || '').trim();"
              "        if (cnt) ytMd += '### 💬 시청자 댓글\\n' + cnt + ' (댓글 영역을 보려면 스크롤을 아래로 조금 더 내려주세요)\\n\\n';"
              "      }"
              "      const ytData = {"
              "        title: ytTitle,"
              "        url: window.location.href,"
              "        author: ytAuthor,"
              "        bodySnippet: ytMd,"
              "        markdown: ytMd,"
              "        buttons: [],"
              "        inputs: []"
              "      };"
              "      const b64 = btoa(unescape(encodeURIComponent(JSON.stringify(ytData))));"
              "      window.location.href = 'http://ui-action/ai-dom-result?data=' + encodeURIComponent(b64);"
              "      return;"
              "    }"
              "    const best = findBestDoc();"
              "    const tDoc = best.doc || document;"
              "    const tWin = best.win || window;"
              "    const selList = ["
              "      '.se-main-container', '#postViewArea', '.se_component_wrap',"
              "      '#dic_area', '#articleBody', '.article_view', '.news_body',"
              "      '.entry-content', '.post-content', '.article-body', '.article_body',"
              "      'article', 'main', '[role=\"main\"]', '#content'"
              "    ];"
              "    let articleEl = null;"
              "    for (let s of selList) {"
              "      const el = tDoc.querySelector(s);"
              "      if (el && el.innerText && el.innerText.trim().length > 100) {"
              "        articleEl = el;"
              "        break;"
              "      }"
              "    }"
              "    if (!articleEl) {"
              "      const nodes = Array.from(tDoc.querySelectorAll('div, section, article, main'));"
              "      let maxScore = -1;"
              "      articleEl = tDoc.body || tDoc.documentElement;"
              "      const vH = tWin.innerHeight || 800;"
              "      const vW = tWin.innerWidth || 1200;"
              "      for (let n of nodes) {"
              "        if (!isVisible(n, tWin)) continue;"
              "        const txt = (n.innerText || '').trim();"
              "        if (txt.length < 50) continue;"
              "        let sc = txt.length / 20.0 + (txt.match(/,/g) || []).length * 2;"
              "        const links = n.querySelectorAll('a');"
              "        let lLen = 0;"
              "        links.forEach(a => lLen += (a.innerText || '').trim().length);"
              "        if (txt.length > 0 && (lLen / txt.length) > 0.5) sc *= 0.5;"
              "        try {"
              "          const r = n.getBoundingClientRect();"
              "          if (r.top >= 0 && r.top <= vH * 1.5 && r.width >= vW * 0.3) sc *= 1.3;"
              "        } catch(e) {}"
              "        sc += n.querySelectorAll('p').length * 5;"
              "        if (sc > maxScore) {"
              "          maxScore = sc;"
              "          articleEl = n;"
              "        }"
              "      }"
              "    }"
              "    const clone = articleEl.cloneNode(true);"
              "    const dropTags = ['script', 'style', 'noscript', 'template', 'svg', 'canvas', 'iframe', 'frame', 'nav', 'header', 'footer', 'aside', 'form', 'select', 'dialog', 'menu'];"
              "    dropTags.forEach(t => clone.querySelectorAll(t).forEach(e => e.remove()));"
              "    const dropClasses = ['.ad', '.ads', '.banner', '.sponsor', '.social-share', '.sns-share', '.share-box', '.pagination', '.paging', '.sidebar', '.widget', '.popup', '.modal', '.footer', '.header-nav'];"
              "    dropClasses.forEach(c => {"
              "      try {"
              "        clone.querySelectorAll(c).forEach(e => {"
              "          if (!e.matches('article, main, .se-main-container, #postViewArea, #dic_area, .entry-content, .article-body')) e.remove();"
              "        });"
              "      } catch(e) {}"
              "    });"
              "    function toMd(node) {"
              "      if (!node) return '';"
              "      let md = '';"
              "      function walk(el) {"
              "        if (!el) return;"
              "        if (el.nodeType === 3) {"
              "          md += el.textContent.replace(/\\s+/g, ' ');"
              "          return;"
              "        }"
              "        if (el.nodeType !== 1) return;"
              "        const tag = el.tagName.toLowerCase();"
              "        if (/^h[1-6]$/.test(tag)) {"
              "          const lvl = parseInt(tag[1], 10);"
              "          const t = (el.innerText || el.textContent || '').trim();"
              "          if (t) md += '\\n\\n' + '#'.repeat(lvl) + ' ' + t + '\\n\\n';"
              "          return;"
              "        }"
              "        if (tag === 'p') {"
              "          const t = (el.innerText || el.textContent || '').trim();"
              "          if (t) {"
              "            md += '\\n\\n';"
              "            for (let c of el.childNodes) walk(c);"
              "            md += '\\n\\n';"
              "          }"
              "          return;"
              "        }"
              "        if (tag === 'blockquote') {"
              "          const t = (el.innerText || el.textContent || '').trim();"
              "          if (t) md += '\\n\\n> ' + t.replace(/\\n+/g, '\\n> ') + '\\n\\n';"
              "          return;"
              "        }"
              "        if (tag === 'ul' || tag === 'ol') {"
              "          md += '\\n\\n';"
              "          let i = 1;"
              "          for (let c of el.children) {"
              "            if (c.tagName && c.tagName.toLowerCase() === 'li') {"
              "              const it = (c.innerText || c.textContent || '').trim();"
              "              if (it) md += (tag === 'ol' ? (i++ + '. ') : '- ') + it + '\\n';"
              "            }"
              "          }"
              "          md += '\\n';"
              "          return;"
              "        }"
              "        if (tag === 'table') {"
              "          md += '\\n\\n';"
              "          const trs = el.querySelectorAll('tr');"
              "          trs.forEach((tr, rIdx) => {"
              "            const tds = tr.querySelectorAll('th, td');"
              "            if (!tds.length) return;"
              "            md += '| ' + Array.from(tds).map(c => (c.innerText || '').trim().replace(/\\|/g, '\\\\|')).join(' | ') + ' |\\n';"
              "            if (rIdx === 0) md += '| ' + Array.from(tds).map(() => '---').join(' | ') + ' |\\n';"
              "          });"
              "          md += '\\n';"
              "          return;"
              "        }"
              "        if (tag === 'pre' || tag === 'code') {"
              "          const ct = (el.innerText || el.textContent || '').trim();"
              "          if (ct) md += '\\n\\n```\\n' + ct + '\\n```\\n\\n';"
              "          return;"
              "        }"
              "        if (tag === 'br') { md += '\\n'; return; }"
              "        if (tag === 'hr') { md += '\\n\\n---\\n\\n'; return; }"
              "        if (tag === 'strong' || tag === 'b') {"
              "          const bt = (el.innerText || el.textContent || '').trim();"
              "          if (bt) md += ' **' + bt + '** ';"
              "          return;"
              "        }"
              "        for (let c of el.childNodes) walk(c);"
              "        if (['div', 'section', 'article'].includes(tag)) md += '\\n';"
              "      }"
              "      walk(node);"
              "      return md.replace(/[ \\t]+/g, ' ').replace(/\\n{3,}/g, '\\n\\n').trim();"
              "    }"
              "    let markdownBody = toMd(clone);"
              "    if (!markdownBody || markdownBody.length < 50) {"
              "      markdownBody = (clone.innerText || (tDoc.body ? tDoc.body.innerText : '') || '').trim();"
              "    }"
              "    if (markdownBody.length > 8000) {"
              "      markdownBody = markdownBody.slice(0, 8000) + '\\n\\n...(이하 생략)';"
              "    }"
              "    const title = getMeta(tDoc, 'og:title') || getMeta(document, 'og:title') || tDoc.title || document.title || '';"
              "    const author = getMeta(tDoc, 'author') || getMeta(tDoc, 'article:author') || getMeta(document, 'author') || '';"
              "    const publishedTime = getMeta(tDoc, 'article:published_time') || getMeta(document, 'article:published_time') || '';"
              "    const image = getMeta(tDoc, 'og:image') || getMeta(document, 'og:image') || '';"
              "    const buttons = Array.from(tDoc.querySelectorAll('button, a, input[type=\"submit\"], [role=\"button\"]'))"
              "      .filter(el => isVisible(el, tWin))"
              "      .slice(0, 30)"
              "      .map(el => ({"
              "        tag: el.tagName.toLowerCase(),"
              "        text: (el.innerText || el.value || '').trim().slice(0, 50),"
              "        id: el.id || '',"
              "        selector: el.id ? ('#' + el.id) : (el.className ? ('.' + el.className.trim().split(/\\s+/)[0]) : el.tagName.toLowerCase())"
              "      }))"
              "      .filter(x => x.text.length > 0);"
              "    const inputs = Array.from(tDoc.querySelectorAll('input:not([type=\"hidden\"]), textarea, select'))"
              "      .filter(el => isVisible(el, tWin))"
              "      .slice(0, 20)"
              "      .map(el => ({"
              "        tag: el.tagName.toLowerCase(),"
              "        type: el.type || '',"
              "        name: el.name || '',"
              "        placeholder: el.placeholder || '',"
              "        selector: el.id ? ('#' + el.id) : (el.name ? ('[name=\"' + el.name + '\"]') : (el.type ? ('input[type=\"' + el.type + '\"]') : el.tagName.toLowerCase()))"
              "      }));"
              "    const data = {"
              "      title: title,"
              "      url: window.location.href,"
              "      author: author,"
              "      publishedTime: publishedTime,"
              "      image: image,"
              "      bodySnippet: markdownBody,"
              "      markdown: markdownBody,"
              "      buttons: buttons,"
              "      inputs: inputs"
              "    };"
              "    const b64 = btoa(unescape(encodeURIComponent(JSON.stringify(data))));"
              "    window.location.href = 'http://ui-action/ai-dom-result?data=' + encodeURIComponent(b64);"
              "  } catch(e) { console.error(e); }"
              "})();";
            cef_frame_t* f = cb->get_main_frame(cb);
            if (f) {
              cef_string_t js_str = {};
              cef_string_from_utf8(dom_collector, strlen(dom_collector), &js_str);
              f->execute_java_script(f, &js_str, NULL, 0);
              cef_string_clear(&js_str);
              f->base.release(&f->base);
            }
          }
        } else if (strncmp(action, "ai-dom-result?", 14) == 0) {
          const char* query = action + 14;
          char* data_b64 = get_query_param(query, "data");
          if (data_b64) {
            for (char* p = data_b64; *p; p++) {
              if (*p == ' ') *p = '+';
            }
            size_t decoded_len = 0;
            unsigned char* decoded = base64_decode(data_b64, &decoded_len);
            if (decoded) {
              cef_browser_t* target_b = win_ctx->sidepanel_browser;
              if (!target_b && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
                tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
                target_b = (active_tab->is_split && active_tab->right_browser) ? active_tab->right_browser : active_tab->browser;
              }
              if (target_b) {
                char* js_code = (char*)malloc(decoded_len + 128);
                if (js_code) {
                  snprintf(js_code, decoded_len + 128, "if (window.onAgentDomExtracted) { window.onAgentDomExtracted(%s); }", (char*)decoded);
                  cef_frame_t* rf = target_b->get_main_frame(target_b);
                  if (rf) {
                    cef_string_t js_str = {};
                    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
                    rf->execute_java_script(rf, &js_str, NULL, 0);
                    cef_string_clear(&js_str);
                    rf->base.release(&rf->base);
                  }
                  free(js_code);
                }
              }
              free(decoded);
            }
            free(data_b64);
          }
        } else if (strcmp(action, "back") == 0) {
          if (cb) cb->go_back(cb);
        } else if (strcmp(action, "forward") == 0) {
          if (cb) cb->go_forward(cb);
        } else if (strcmp(action, "reload") == 0) {
          if (cb) cb->reload(cb);
        } else if (strcmp(action, "get-downloads") == 0) {
          char *buf = (char*)malloc(1024 * 1024);
          if (buf) {
            download_manager_get_list_json(buf, 1024 * 1024);
            cef_frame_t *active_frame = frame;
            if (active_frame) {
              char *js_code = (char*)malloc(1024 * 1024 + 128);
              if (js_code) {
                snprintf(js_code, 1024 * 1024 + 128, "if (window.renderDownloads) { window.renderDownloads(%s); }", buf);
                cef_string_t js_str = {};
                cef_string_from_utf8(js_code, strlen(js_code), &js_str);
                active_frame->execute_java_script(active_frame, &js_str, NULL, 0);
                cef_string_clear(&js_str);
                free(js_code);
              }
            }
            free(buf);
          }
        } else if (strncmp(action, "download-open-file?", 19) == 0) {
          const char* query = action + 19;
          char* path = get_query_param(query, "path");
          if (path) {
            download_manager_open_file(path);
            free(path);
          }
        } else if (strncmp(action, "download-show-in-folder", 23) == 0) {
          const char* query = (action[23] == '?') ? (action + 24) : "";
          char* path = (query[0] != '\0') ? get_query_param(query, "path") : NULL;
          download_manager_show_in_folder(path ? path : "");
          if (path) free(path);
        } else if (strncmp(action, "download-delete-file?", 21) == 0) {
          const char* query = action + 21;
          char* id_str = get_query_param(query, "id");
          char* path = get_query_param(query, "path");
          if (id_str && path) {
            uint32_t id = (uint32_t)atoi(id_str);
            download_manager_delete_file(id, path);
          }
          if (id_str) free(id_str);
          if (path) free(path);
        } else if (strncmp(action, "download-remove-history?", 24) == 0) {
          const char* query = action + 24;
          char* id_str = get_query_param(query, "id");
          if (id_str) {
            uint32_t id = (uint32_t)atoi(id_str);
            download_manager_remove_history(id);
            free(id_str);
          }
        } else if (strcmp(action, "download-clear-history") == 0) {
          download_manager_clear_history();
        } else if (strncmp(action, "download-pause?", 15) == 0) {
          const char* query = action + 15;
          char* id_str = get_query_param(query, "id");
          if (id_str) {
            uint32_t id = (uint32_t)atoi(id_str);
            download_manager_pause(id);
            free(id_str);
          }
        } else if (strncmp(action, "download-resume?", 16) == 0) {
          const char* query = action + 16;
          char* id_str = get_query_param(query, "id");
          if (id_str) {
            uint32_t id = (uint32_t)atoi(id_str);
            download_manager_resume(id);
            free(id_str);
          }
        } else if (strncmp(action, "download-cancel?", 16) == 0) {
          const char* query = action + 16;
          char* id_str = get_query_param(query, "id");
          if (id_str) {
            uint32_t id = (uint32_t)atoi(id_str);
            download_manager_cancel(id);
            free(id_str);
          }
        } else if (strcmp(action, "check-cef-update") == 0) {
          simple_installer_check_update_async(win_ctx);
        } else if (strncmp(action, "show-menu?", 10) == 0) {
          int click_x = 0, click_y = 0;
          if (sscanf(action + 10, "x=%d&y=%d", &click_x, &click_y) == 2) {
            double scale = 1.0;
            UINT dpi = GetDpiForWindow(win_ctx->ui_hwnd);
            if (dpi > 0) {
              scale = (double)dpi / 96.0;
            }
            POINT pt = {(int)(click_x * scale), (int)(click_y * scale)};
            ClientToScreen(win_ctx->ui_hwnd, &pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1001, L"새 탭");
            AppendMenuW(hMenu, MF_STRING, 1002, L"새 창");
            AppendMenuW(hMenu, MF_STRING, 1008, L"다운로드 관리자 (Ctrl+J)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 1003, L"인쇄...");
            AppendMenuW(hMenu, MF_STRING, 1004, L"개발자 도구 (Inspect)");
            AppendMenuW(hMenu, MF_STRING, 1005, L"페이지 소스 보기");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 1009, L"CEF 런타임 업데이트 확인");
            AppendMenuW(hMenu, MF_STRING, 1006, L"종료");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, win_ctx->main_hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 1001) {
              CreateNewTab(win_ctx, "lite://favorites");
            } else if (cmd == 1002) {
              create_browser_window("lite://favorites");
            } else if (cmd == 1008) {
              CreateNewTab(win_ctx, "lite://downloads");
            } else if (cmd == 1009) {
              simple_installer_check_update_async(win_ctx);
            } else if (cmd == 1003) {
              if (cb) {
                cef_browser_host_t* host = cb->get_host(cb);
                if (host) {
                  host->print(host);
                  host->base.release(&host->base);
                }
              }
            } else if (cmd == 1004) {
              if (cb) {
                cef_window_info_t windowInfo = {};
                windowInfo.size = sizeof(cef_window_info_t);
                windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                windowInfo.parent_window = NULL;
                windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

                cef_browser_settings_t settings = {};
                settings.size = sizeof(cef_browser_settings_t);

                cef_browser_host_t* host = cb->get_host(cb);
                if (host) {
                  host->show_dev_tools(host, &windowInfo, NULL, &settings, NULL);
                  host->base.release(&host->base);
                }
              }
            } else if (cmd == 1005) {
              if (win_ctx && win_ctx->tab_count < MAX_TABS && win_ctx->active_tab_index >= 0) {
                char vs_url[1200] = "view-source:";
                strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);

                RECT rect;
                GetClientRect(win_ctx->main_hwnd, &rect);
                int width = rect.right;
                int height = rect.bottom;

                int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
                int content_y = ui_height + 1;
                int content_h = height - content_y - 1;

                cef_browser_settings_t browser_settings = {};
                browser_settings.size = sizeof(cef_browser_settings_t);

                cef_window_info_t content_window_info = {};
                content_window_info.size = sizeof(cef_window_info_t);
                content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                content_window_info.parent_window = win_ctx->main_hwnd;
                content_window_info.bounds.x = 1;
                content_window_info.bounds.y = content_y;
                content_window_info.bounds.width = width - 2;
                content_window_info.bounds.height = content_h;
                content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

                cef_string_t content_url = {};
                cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);

                simple_handler_t *content_handler = simple_handler_create(0);
                content_handler->window_ctx = win_ctx;

                int next_idx = win_ctx->tab_count;
                int max_id = 0;
                for(int k=0; k<win_ctx->tab_count; k++) {
                  if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
                }
                win_ctx->tabs[next_idx].tab_id = max_id + 1;
                win_ctx->tabs[next_idx].browser = NULL;
                win_ctx->tabs[next_idx].hwnd = NULL;
                strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
                strcpy(win_ctx->tabs[next_idx].url, vs_url);
                win_ctx->tab_count++;

                cef_browser_host_create_browser(
                    &content_window_info, &content_handler->client, &content_url,
                    &browser_settings, NULL, NULL);
                cef_string_clear(&content_url);
              }
            } else if (cmd == 1006) {
              PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
            }
          }
        } else if (strcmp(action, "print") == 0) {
          if (cb) {
            cef_browser_host_t* host = cb->get_host(cb);
            if (host) {
              host->print(host);
              host->base.release(&host->base);
            }
          }
        } else if (strcmp(action, "devtools") == 0) {
          if (cb) {
            cef_window_info_t windowInfo = {};
            windowInfo.size = sizeof(cef_window_info_t);
            windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            windowInfo.parent_window = NULL;
            windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

            cef_browser_settings_t settings = {};
            settings.size = sizeof(cef_browser_settings_t);

            cef_browser_host_t* host = cb->get_host(cb);
            if (host) {
              host->show_dev_tools(host, &windowInfo, NULL, &settings, NULL);
              host->base.release(&host->base);
            }
          }
        } else if (strcmp(action, "view-source") == 0) {
          if (win_ctx && win_ctx->tab_count < MAX_TABS && win_ctx->active_tab_index >= 0) {
            char vs_url[1200] = "view-source:";
            strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);

            RECT rect;
            GetClientRect(win_ctx->main_hwnd, &rect);
            int width = rect.right;
            int height = rect.bottom;

            int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
            int content_y = ui_height + 1;
            int content_h = height - content_y - 1;

            cef_browser_settings_t browser_settings = {};
            browser_settings.size = sizeof(cef_browser_settings_t);

            cef_window_info_t content_window_info = {};
            content_window_info.size = sizeof(cef_window_info_t);
            content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            content_window_info.parent_window = win_ctx->main_hwnd;
            content_window_info.bounds.x = 1;
            content_window_info.bounds.y = content_y;
            content_window_info.bounds.width = width - 2;
            content_window_info.bounds.height = content_h;
            content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

            cef_string_t content_url = {};
            cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);

            simple_handler_t *content_handler = simple_handler_create(0);
            content_handler->window_ctx = win_ctx;

            int next_idx = win_ctx->tab_count;
            int max_id = 0;
            for(int k=0; k<win_ctx->tab_count; k++) {
              if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
            }
            win_ctx->tabs[next_idx].tab_id = max_id + 1;
            win_ctx->tabs[next_idx].browser = NULL;
            win_ctx->tabs[next_idx].hwnd = NULL;
            strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
            strcpy(win_ctx->tabs[next_idx].url, vs_url);
            win_ctx->tab_count++;

            cef_browser_host_create_browser(
                &content_window_info, &content_handler->client, &content_url,
                &browser_settings, NULL, NULL);
            cef_string_clear(&content_url);
          }
        } else if (strcmp(action, "exit") == 0) {
          PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
        } else if (strcmp(action, "drag-window") == 0) {
          ReleaseCapture();
          SendMessage(win_ctx->main_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        } else if (strcmp(action, "window-minimize") == 0) {
          ShowWindow(win_ctx->main_hwnd, SW_MINIMIZE);
        } else if (strcmp(action, "window-maximize") == 0) {
          if (IsZoomed(win_ctx->main_hwnd)) {
            ShowWindow(win_ctx->main_hwnd, SW_RESTORE);
          } else {
            ShowWindow(win_ctx->main_hwnd, SW_MAXIMIZE);
          }
        } else if (strcmp(action, "window-close") == 0) {
          PostMessage(win_ctx->main_hwnd, WM_CLOSE, 0, 0);
        } else if (strncmp(action, "load?url=", 9) == 0) {
          if (cb) {
            const char *encoded_url = action + 9;
            char *decoded = (char *)malloc(strlen(encoded_url) + 1);
            if (decoded) {
              size_t i = 0, j = 0;
              while (encoded_url[i]) {
                if (encoded_url[i] == '%' && encoded_url[i + 1] && encoded_url[i + 2]) {
                  char hex[3] = {encoded_url[i + 1], encoded_url[i + 2], '\0'};
                  decoded[j++] = (char)strtol(hex, NULL, 16);
                  i += 3;
                } else if (encoded_url[i] == '+') {
                  decoded[j++] = ' ';
                  i++;
                } else {
                  decoded[j++] = encoded_url[i];
                  i++;
                }
              }
              decoded[j] = '\0';

              cef_string_t cef_url = {};
              cef_string_from_utf8(decoded, strlen(decoded), &cef_url);

              cef_frame_t *main_frame = cb->get_main_frame(cb);
              if (main_frame) {
                main_frame->load_url(main_frame, &cef_url);
                main_frame->base.release(&main_frame->base);
              }
              cef_string_clear(&cef_url);
              free(decoded);
            }
          }
        } else if (strncmp(action, "expand-ui?", 10) == 0) {
          int h = 500;
          if (sscanf(action + 10, "height=%d", &h) == 1) {
            if (win_ctx) {
              double scale = 1.0;
              UINT dpi = GetDpiForWindow(win_ctx->main_hwnd);
              if (dpi > 0) scale = (double)dpi / 96.0;
              win_ctx->is_ui_expanded = 1;
              win_ctx->ui_expanded_height = (int)(h * scale);
              if (win_ctx->ui_hwnd) {
                RECT rect;
                GetClientRect(win_ctx->main_hwnd, &rect);
                SetWindowPos(win_ctx->ui_hwnd, HWND_TOP, 0, 0, rect.right, win_ctx->ui_expanded_height, SWP_NOACTIVATE);
              }
            }
          }
        } else if (strcmp(action, "collapse-ui") == 0) {
          if (win_ctx) {
            win_ctx->is_ui_expanded = 0;
            if (win_ctx->ui_hwnd) {
              RECT rect;
              GetClientRect(win_ctx->main_hwnd, &rect);
              int default_h = GetUIHeightForWindow(win_ctx->main_hwnd);
              SetWindowPos(win_ctx->ui_hwnd, HWND_TOP, 0, 0, rect.right, default_h, SWP_NOACTIVATE);
            }
          }
        } else if (strcmp(action, "load-bookmarks") == 0) {
          char filepath[MAX_PATH];
          GetBookmarksFilePath(filepath, sizeof(filepath));
          
          FILE* f = fopen(filepath, "rb");
          if (!f) {
            const char* default_json = "{\"folders\":[\"\\uC990\\uACA8\\uCC3E\\uAE30 \\uBC14\",\"\\uAE30\\uD0C0 \\uC990\\uACA8\\uCC3E\\uAE30\"],\"bookmarks\":[]}";
            f = fopen(filepath, "wb");
            if (f) {
              fwrite(default_json, 1, strlen(default_json), f);
              fclose(f);
            }
            f = fopen(filepath, "rb");
          }

          if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
              unsigned char* buf = (unsigned char*)malloc(fsize + 1);
              if (buf) {
                fread(buf, 1, fsize, f);
                buf[fsize] = '\0';
                
                char* b64_str = base64_encode(buf, fsize);
                if (b64_str && win_ctx && win_ctx->ui_browser) {
                  size_t b64_len = strlen(b64_str);
                  char* js_call = (char*)malloc(b64_len + 128);
                  if (js_call) {
                    snprintf(js_call, b64_len + 128, "if (window.loadBookmarksDataB64) { window.loadBookmarksDataB64('%s'); }", b64_str);
                    
                    cef_frame_t* ui_frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
                    if (ui_frame) {
                      cef_string_t js_str = {};
                      cef_string_from_utf8(js_call, strlen(js_call), &js_str);
                      ui_frame->execute_java_script(ui_frame, &js_str, NULL, 0);
                      cef_string_clear(&js_str);
                      ui_frame->base.release(&ui_frame->base);
                    }
                    free(js_call);
                  }
                  free(b64_str);
                }
                free(buf);
              }
            }
            fclose(f);
          }
        } else if (strncmp(action, "save-bookmarks?", 15) == 0) {
          const char* query = action + 15;
          char* data_base64 = get_query_param(query, "data");
          if (data_base64) {
            size_t decoded_len = 0;
            unsigned char* decoded = base64_decode(data_base64, &decoded_len);
            if (decoded) {
              char filepath[MAX_PATH];
              GetBookmarksFilePath(filepath, sizeof(filepath));
              FILE* f = fopen(filepath, "wb");
              if (f) {
                fwrite(decoded, 1, decoded_len, f);
                fclose(f);
              }
              free(decoded);
            }
            free(data_base64);
          }
        } else if (strcmp(action, "extract-and-save-bookmark") == 0) {
          if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
            cef_browser_t* act_browser = win_ctx->tabs[win_ctx->active_tab_index].browser;
            if (act_browser) {
              char extractor_path[MAX_PATH];
              ResolveUIFilePath("ui/extractor.js", extractor_path, sizeof(extractor_path), NULL, 0);
              FILE* f_js = fopen(extractor_path, "rb");
              if (f_js) {
                fseek(f_js, 0, SEEK_END);
                long js_sz = ftell(f_js);
                fseek(f_js, 0, SEEK_SET);

                if (js_sz > 0) {
                  char* js_buf = (char*)malloc(js_sz + 1);
                  if (js_buf) {
                    fread(js_buf, 1, js_sz, f_js);
                    js_buf[js_sz] = '\0';

                    cef_frame_t* c_frame = act_browser->get_main_frame(act_browser);
                    if (c_frame) {
                      cef_string_t js_str = {};
                      cef_string_from_utf8(js_buf, strlen(js_buf), &js_str);
                      c_frame->execute_java_script(c_frame, &js_str, NULL, 0);
                      cef_string_clear(&js_str);
                      c_frame->base.release(&c_frame->base);
                    }
                    free(js_buf);
                  }
                }
                fclose(f_js);
              }
            }
          }
        } else if (strncmp(action, "save-contextual-bookmark?", 25) == 0) {
          const char* query = action + 25;
          char* data_b64 = get_query_param(query, "data");
          if (data_b64) {
            if (win_ctx && win_ctx->ui_browser) {
              char* js_call = (char*)malloc(strlen(data_b64) + 128);
              if (js_call) {
                snprintf(js_call, strlen(data_b64) + 128, "if (window.onContextualBookmarkExtractedB64) { window.onContextualBookmarkExtractedB64('%s'); }", data_b64);
                cef_frame_t* ui_frame = win_ctx->ui_browser->get_main_frame(win_ctx->ui_browser);
                if (ui_frame) {
                  cef_string_t js_str = {};
                  cef_string_from_utf8(js_call, strlen(js_call), &js_str);
                  ui_frame->execute_java_script(ui_frame, &js_str, NULL, 0);
                  cef_string_clear(&js_str);
                  ui_frame->base.release(&ui_frame->base);
                }
                free(js_call);
              }
            }
            free(data_b64);
          }
        } else if (strcmp(action, "open-bookmark-manager") == 0) {
          if (win_ctx) {
            CreateNewTab(win_ctx, "lite://favorites");
          }
        } else if (strcmp(action, "open-download-manager") == 0) {
          if (win_ctx) {
            CreateNewTab(win_ctx, "lite://downloads");
          }
        } else if (strcmp(action, "load-bookmarks-v2") == 0) {
          char filepath[MAX_PATH];
          GetBookmarksFilePath(filepath, sizeof(filepath));
          
          FILE* f = fopen(filepath, "rb");
          if (!f) {
            const char* default_json = "{\"bookmarks\":[]}";
            f = fopen(filepath, "wb");
            if (f) {
              fwrite(default_json, 1, strlen(default_json), f);
              fclose(f);
            }
            f = fopen(filepath, "rb");
          }

          if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
              unsigned char* buf = (unsigned char*)malloc(fsize + 1);
              if (buf) {
                fread(buf, 1, fsize, f);
                buf[fsize] = '\0';
                
                char* b64_str = base64_encode(buf, fsize);
                if (b64_str && win_ctx) {
                  size_t b64_len = strlen(b64_str);
                  char* js_call = (char*)malloc(b64_len + 128);
                  if (js_call) {
                    snprintf(js_call, b64_len + 128, "if (window.loadBookmarksDataB64) { window.loadBookmarksDataB64('%s'); }", b64_str);
                    
                    if (win_ctx->ui_browser) {
                      ExecuteJsOnBrowser(win_ctx->ui_browser, js_call);
                    }
                    if (browser && browser != win_ctx->ui_browser) {
                      ExecuteJsOnBrowser(browser, js_call);
                    }
                    free(js_call);
                  }
                  free(b64_str);
                }
                free(buf);
              }
            }
            fclose(f);
          }
        } else if (strcmp(action, "load-history") == 0) {
          char filepath[MAX_PATH];
          GetHistoryFilePath(filepath, sizeof(filepath));
          
          FILE* f = fopen(filepath, "rb");
          if (!f) {
            const char* default_json = "{\"history\":[]}";
            f = fopen(filepath, "wb");
            if (f) {
              fwrite(default_json, 1, strlen(default_json), f);
              fclose(f);
            }
            f = fopen(filepath, "rb");
          }

          if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
              unsigned char* buf = (unsigned char*)malloc(fsize + 1);
              if (buf) {
                fread(buf, 1, fsize, f);
                buf[fsize] = '\0';
                
                char* b64_str = base64_encode(buf, fsize);
                if (b64_str && win_ctx) {
                  size_t b64_len = strlen(b64_str);
                  char* js_call = (char*)malloc(b64_len + 128);
                  if (js_call) {
                    snprintf(js_call, b64_len + 128, "if (window.loadHistoryDataB64) { window.loadHistoryDataB64('%s'); }", b64_str);
                    
                    if (win_ctx->ui_browser) {
                      ExecuteJsOnBrowser(win_ctx->ui_browser, js_call);
                    }
                    if (browser && browser != win_ctx->ui_browser) {
                      ExecuteJsOnBrowser(browser, js_call);
                    }
                    free(js_call);
                  }
                  free(b64_str);
                }
                free(buf);
              }
            }
            fclose(f);
          }
        } else if (strncmp(action, "save-history?", 13) == 0) {
          const char* query = action + 13;
          char* data_base64 = get_query_param(query, "data");
          if (data_base64) {
            size_t decoded_len = 0;
            unsigned char* decoded = base64_decode(data_base64, &decoded_len);
            if (decoded) {
              char filepath[MAX_PATH];
              GetHistoryFilePath(filepath, sizeof(filepath));
              FILE* f = fopen(filepath, "wb");
              if (f) {
                fwrite(decoded, 1, decoded_len, f);
                fclose(f);
              }
              free(decoded);
            }
            free(data_base64);
          }
        } else if (strncmp(action, "send-prompt?", 12) == 0) {
          const char* query = action + 12;
          char* text_base64 = get_query_param(query, "text");
          
          if (text_base64) {
            if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
              cef_browser_t* content_browser = win_ctx->tabs[win_ctx->active_tab_index].browser;
              if (content_browser) {
                size_t js_len = strlen(text_base64) + 1024;
                char* js_code = (char*)malloc(js_len);
                if (js_code) {
                  snprintf(js_code, js_len,
                    "(function() {"
                    "  const base64 = '%s';"
                    "  const binary = atob(base64);"
                    "  const len = binary.length;"
                    "  const bytes = new Uint8Array(len);"
                    "  for (let i = 0; i < len; i++) {"
                    "    bytes[i] = binary.charCodeAt(i);"
                    "  }"
                    "  const text = new TextDecoder().decode(bytes);"
                    "  let el = document.querySelector('#prompt-textarea');"
                    "  if (!el) {"
                    "    el = document.querySelector('div[contenteditable=\"true\"]') || document.querySelector('[contenteditable=\"true\"]');"
                    "  }"
                    "  if (!el) {"
                    "    el = document.querySelector('textarea') || document.querySelector('input[type=\"text\"]');"
                    "  }"
                    "  if (el) {"
                    "    el.focus();"
                    "    if (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT') {"
                    "      el.value = text;"
                    "    } else {"
                    "      el.innerText = text;"
                    "    }"
                    "    const inputEvent = new Event('input', { bubbles: true });"
                    "    el.dispatchEvent(inputEvent);"
                    "    const changeEvent = new Event('change', { bubbles: true });"
                    "    el.dispatchEvent(changeEvent);"
                    "    el.focus();"
                    "  }"
                    "})();",
                    text_base64
                  );
                  
                  cef_frame_t* c_frame = content_browser->get_main_frame(content_browser);
                  if (c_frame) {
                    cef_string_t js_str = {};
                    cef_string_from_utf8(js_code, strlen(js_code), &js_str);
                    c_frame->execute_java_script(c_frame, &js_str, NULL, 0);
                    cef_string_clear(&js_str);
                    c_frame->base.release(&c_frame->base);
                  }
                  free(js_code);
                }
              }
            }
            free(text_base64);
          }
        } else if (strcmp(action, "new-tab") == 0) {
          CreateNewTab(win_ctx, "lite://favorites");
        } else if (strncmp(action, "switch-tab?id=", 14) == 0) {
          int target_id = atoi(action + 14);
          int found_idx = -1;
          for (int i = 0; i < win_ctx->tab_count; i++) {
            if (win_ctx->tabs[i].tab_id == target_id) {
              found_idx = i;
              break;
            }
          }
          if (found_idx != -1 && found_idx != win_ctx->active_tab_index) {
            for (int k = 0; k < win_ctx->tab_count; k++) {
              if (k != found_idx) {
                if (win_ctx->tabs[k].hwnd) ShowWindow(win_ctx->tabs[k].hwnd, SW_HIDE);
                if (win_ctx->tabs[k].right_hwnd) ShowWindow(win_ctx->tabs[k].right_hwnd, SW_HIDE);
              }
            }

            win_ctx->active_tab_index = found_idx;
            win_ctx->tabs[found_idx].is_loaded = 1;
            if (win_ctx->tabs[found_idx].hwnd) {
              ShowWindow(win_ctx->tabs[found_idx].hwnd, SW_SHOW);
              SetFocus(win_ctx->tabs[found_idx].hwnd);
              
              RECT rect;
              GetClientRect(win_ctx->main_hwnd, &rect);
              PostMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));

              cef_browser_host_t* host = win_ctx->tabs[found_idx].browser->get_host(win_ctx->tabs[found_idx].browser);
              if (host) {
                host->was_resized(host);
                host->set_focus(host, 1);
                host->base.release(&host->base);
              }
            }
            update_ui_tabs(win_ctx);
            update_ui_nav_state(win_ctx);
          }
        } else if (strcmp(action, "trigger-find") == 0) {
          if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
            tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
            HWND target_hwnd = (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_hwnd)
                                   ? active_tab->right_hwnd : active_tab->hwnd;
            cef_browser_t* target_b = (active_tab->is_split && active_tab->active_split == 1 && active_tab->right_browser)
                                   ? active_tab->right_browser : active_tab->browser;
            if (target_hwnd && IsWindowVisible(target_hwnd)) {
              SetFocus(target_hwnd);
            }
            if (target_b) {
              cef_browser_host_t* host = target_b->get_host(target_b);
              if (host) {
                host->set_focus(host, 1);

                cef_key_event_t key_event = {};
                key_event.size = sizeof(cef_key_event_t);
                key_event.type = KEYEVENT_RAWKEYDOWN;
                key_event.windows_key_code = 'F';
                key_event.native_key_code = 'F';
                key_event.modifiers = EVENTFLAG_CONTROL_DOWN;
                host->send_key_event(host, &key_event);

                key_event.type = KEYEVENT_KEYUP;
                host->send_key_event(host, &key_event);

                host->base.release(&host->base);
              }
            }
          }
        } else if (strncmp(action, "close-tab?id=", 13) == 0) {
          int target_id = atoi(action + 13);
          CloseTab(win_ctx, target_id);
        } else if (strncmp(action, "show-tab-menu?", 14) == 0) {
          int tab_id = 0, click_x = 0, click_y = 0;
          if (sscanf(action + 14, "id=%d&x=%d&y=%d", &tab_id, &click_x, &click_y) == 3) {
            double scale = 1.0;
            UINT dpi = GetDpiForWindow(win_ctx->ui_hwnd);
            if (dpi > 0) {
              scale = (double)dpi / 96.0;
            }
            POINT pt = {(int)(click_x * scale), (int)(click_y * scale)};
            ClientToScreen(win_ctx->ui_hwnd, &pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 2001, L"새 탭");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 2002, L"탭 닫기");
            AppendMenuW(hMenu, MF_STRING, 2003, L"다른 탭 닫기");
            AppendMenuW(hMenu, MF_STRING, 2004, L"오른쪽 탭 닫기");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, win_ctx->main_hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 2001) {
              CreateNewTab(win_ctx, "lite://favorites");
            } else if (cmd == 2002) {
              CloseTab(win_ctx, tab_id);
            } else if (cmd == 2003) {
              CloseOtherTabs(win_ctx, tab_id);
            } else if (cmd == 2004) {
              CloseTabsToRight(win_ctx, tab_id);
            }
          }
        } else if (strcmp(action, "new-window") == 0) {
          create_browser_window("lite://favorites");
        } else if (strncmp(action, "detach-tab?id=", 14) == 0) {
          int target_id = atoi(action + 14);
          int found_idx = -1;
          for (int i = 0; i < win_ctx->tab_count; i++) {
            if (win_ctx->tabs[i].tab_id == target_id) {
              found_idx = i;
              break;
            }
          }
          if (found_idx != -1 && win_ctx->tab_count > 1) {
            cef_browser_t* detached_browser = win_ctx->tabs[found_idx].browser;
            HWND detached_hwnd = win_ctx->tabs[found_idx].hwnd;
            char target_url[4096];
            char target_title[256];
            strcpy(target_url, win_ctx->tabs[found_idx].url);
            strcpy(target_title, win_ctx->tabs[found_idx].title);

            browser_window_t* new_win = create_browser_window_for_detached(
                detached_browser, detached_hwnd, target_url, target_title, CW_USEDEFAULT, CW_USEDEFAULT);

            if (new_win) {
              RemoveTabAt(win_ctx, found_idx, 0);

              cef_browser_host_t* host = detached_browser->get_host(detached_browser);
              cef_client_t* client = host->get_client(host);
              if (client) {
                simple_handler_t* detached_handler = (simple_handler_t*)client;
                detached_handler->window_ctx = new_win;
              }
              host->base.release(&host->base);
            }
          }
        } else if (strncmp(action, "drag-end?id=", 12) == 0) {
          int target_id = atoi(action + 12);
          POINT pt;
          GetCursorPos(&pt);
          RECT rect;
          GetWindowRect(win_ctx->main_hwnd, &rect);
          if (!PtInRect(&rect, pt)) {
            int found_idx = -1;
            for (int i = 0; i < win_ctx->tab_count; i++) {
              if (win_ctx->tabs[i].tab_id == target_id) {
                found_idx = i;
                break;
              }
            }
            if (found_idx != -1 && win_ctx->tab_count > 1) {
              cef_browser_t* detached_browser = win_ctx->tabs[found_idx].browser;
              HWND detached_hwnd = win_ctx->tabs[found_idx].hwnd;
              char target_url[4096];
              char target_title[256];
              strcpy(target_url, win_ctx->tabs[found_idx].url);
              strcpy(target_title, win_ctx->tabs[found_idx].title);

              browser_window_t* new_win = create_browser_window_for_detached(
                  detached_browser, detached_hwnd, target_url, target_title, pt.x - 100, pt.y - 10);

              if (new_win) {
                RemoveTabAt(win_ctx, found_idx, 0);

                cef_browser_host_t* host = detached_browser->get_host(detached_browser);
                cef_client_t* client = host->get_client(host);
                if (client) {
                  simple_handler_t* detached_handler = (simple_handler_t*)client;
                  detached_handler->window_ctx = new_win;
                }
                host->base.release(&host->base);
              }
            }
          }
        }
      }

      cef_string_utf8_clear(&url_utf8);
      cef_string_userfree_free(url_userfree);

      return 1;
    }

    cef_string_utf8_clear(&url_utf8);
    cef_string_userfree_free(url_userfree);
  }

  return 0;
}

int CEF_CALLBACK request_handler_on_open_urlfrom_tab(
    struct _cef_request_handler_t* self,
    struct _cef_browser_t* browser,
    struct _cef_frame_t* frame,
    const cef_string_t* target_url,
    cef_window_open_disposition_t target_disposition,
    int user_gesture) {

  simple_request_handler_t* handler = (simple_request_handler_t*)self;
  browser_window_t *win_ctx = (handler && handler->parent) ? handler->parent->window_ctx : NULL;

  cef_string_utf8_t url_utf8 = {};
  int conv_ok = 0;

  if (target_url && target_url->str && target_url->length > 0) {
    cef_string_to_utf8(target_url->str, target_url->length, &url_utf8);
    conv_ok = 1;
  }

  char target_url_str[4096] = {0};
  if (conv_ok && url_utf8.str && strlen(url_utf8.str) > 0) {
    strncpy(target_url_str, url_utf8.str, sizeof(target_url_str) - 1);
    cef_string_utf8_clear(&url_utf8);
  } else {
    strcpy(target_url_str, "about:blank");
  }

  if (win_ctx) {
    int found_tab_idx = -1;
    for (int i = 0; i < win_ctx->tab_count; i++) {
      if ((win_ctx->tabs[i].browser && browser->get_identifier(browser) == win_ctx->tabs[i].browser->get_identifier(win_ctx->tabs[i].browser)) ||
          (win_ctx->tabs[i].right_browser && browser->get_identifier(browser) == win_ctx->tabs[i].right_browser->get_identifier(win_ctx->tabs[i].right_browser))) {
        found_tab_idx = i;
        break;
      }
    }

    if (found_tab_idx != -1 && win_ctx->tabs[found_tab_idx].is_split) {
      cef_frame_t* target_frame = browser->get_main_frame(browser);
      if (target_frame) {
        cef_string_t url_str = {};
        cef_string_from_utf8(target_url_str, strlen(target_url_str), &url_str);
        target_frame->load_url(target_frame, &url_str);
        cef_string_clear(&url_str);
        target_frame->base.release(&target_frame->base);
      }
      return 1;
    }

    if (win_ctx->tab_count < MAX_TABS) {
      CreateNewTab(win_ctx, target_url_str);
    }
  }

  // Return 1 (true) to cancel default Chromium navigation/new window action for Ctrl+Click / Middle Click
  return 1;
}

simple_request_handler_t *request_handler_create(simple_handler_t *parent) {
  simple_request_handler_t *handler =
      (simple_request_handler_t *)calloc(1, sizeof(simple_request_handler_t));
  CHECK(handler);

  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_request_handler_t,
                           request_handler);

  handler->handler.on_before_browse = request_handler_on_before_browse;
  handler->handler.on_open_urlfrom_tab = request_handler_on_open_urlfrom_tab;
  handler->parent = parent;

  atomic_store(&handler->ref_count, 1);

  return handler;
}

//
// Context menu handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_context_menu_handler_t, context_menu_handler,
                             ref_count)

void CEF_CALLBACK context_menu_on_before_context_menu(
    cef_context_menu_handler_t* self,
    cef_browser_t* browser,
    cef_frame_t* frame,
    cef_context_menu_params_t* params,
    cef_menu_model_t* model) {
  
  LogMsg("on_before_context_menu: start\n");
  
  // Clear default items to prevent CEF from displaying anything.
  model->clear(model);
  
  // Check if this right-click is on the UI browser.
  // If so, do not display any context menu.
  simple_context_menu_handler_t* ctx_handler = (simple_context_menu_handler_t*)self;
  if (ctx_handler && ctx_handler->parent && ctx_handler->parent->window_ctx) {
    browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
    if (win_ctx->ui_browser) {
      int ui_id = win_ctx->ui_browser->get_identifier(win_ctx->ui_browser);
      int cur_id = browser->get_identifier(browser);
      if (ui_id == cur_id) {
        LogMsg("on_before_context_menu: bypassed for ui_browser\n");
        return;
      }
    }
  }
  
  // Use GetCursorPos to get exact mouse coordinate on the screen,
  // preventing alignment/DPI/DWM shadow margins offset mismatch.
  POINT pt = {};
  GetCursorPos(&pt);
  
  int has_link = 0;
  char* link_url_str = NULL;
  
  if (params) {
    cef_string_userfree_t link_url = params->get_link_url(params);
    if (link_url && link_url->length > 0) {
      cef_string_utf8_t utf8 = {};
      cef_string_to_utf8(link_url->str, link_url->length, &utf8);
      if (utf8.str && strlen(utf8.str) > 0) {
        has_link = 1;
        link_url_str = _strdup(utf8.str);
      }
      cef_string_utf8_clear(&utf8);
    }
    if (link_url) {
      cef_string_userfree_free(link_url);
    }
  }
  
  cef_browser_host_t* host = browser->get_host(browser);
  if (host) {
    HWND hwnd = host->get_window_handle(host);
    
    HMENU hMenu = CreatePopupMenu();
    
    if (has_link) {
      AppendMenuW(hMenu, MF_STRING, 3001, L"새 탭에서 링크 열기");
      AppendMenuW(hMenu, MF_STRING, 3004, L"다른 분할 화면에서 열기");
      AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
      AppendMenuW(hMenu, MF_STRING, 3002, L"링크 페이지 저장");
      AppendMenuW(hMenu, MF_STRING, 3003, L"링크 복사");
      AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
      AppendMenuW(hMenu, MF_STRING, MENU_ID_USER_FIRST, L"검사 (Inspect)");
    } else {
      int can_back = browser->can_go_back(browser);
      int can_forward = browser->can_go_forward(browser);
      
      AppendMenuW(hMenu, MF_STRING | (can_back ? 0 : MF_GRAYED), MENU_ID_BACK, L"뒤로 가기");
      AppendMenuW(hMenu, MF_STRING | (can_forward ? 0 : MF_GRAYED), MENU_ID_FORWARD, L"앞으로 가기");
      AppendMenuW(hMenu, MF_STRING, MENU_ID_RELOAD, L"새로고침");
      AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
      AppendMenuW(hMenu, MF_STRING, MENU_ID_PRINT, L"인쇄...");
      AppendMenuW(hMenu, MF_STRING, MENU_ID_VIEW_SOURCE, L"페이지 소스 보기");
      AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
      AppendMenuW(hMenu, MF_STRING, MENU_ID_USER_FIRST, L"검사 (Inspect)");
    }
    
    LogMsg("on_before_context_menu: calling TrackPopupMenu (blocking) at screen coords x=%d, y=%d\n", pt.x, pt.y);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    LogMsg("on_before_context_menu: TrackPopupMenu returned cmd=%d\n", cmd);
    DestroyMenu(hMenu);
    
    if (cmd > 0) {
      if (cmd == MENU_ID_USER_FIRST) {
        LogMsg("on_before_context_menu: show dev tools\n");
        cef_window_info_t windowInfo = {};
        windowInfo.size = sizeof(cef_window_info_t);
        windowInfo.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        windowInfo.parent_window = NULL;
        windowInfo.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
        
        cef_browser_settings_t settings = {};
        settings.size = sizeof(cef_browser_settings_t);
        
        int click_x = params->get_xcoord(params);
        int click_y = params->get_ycoord(params);
        cef_point_t inspect_at = { click_x, click_y };
        host->show_dev_tools(host, &windowInfo, NULL, &settings, &inspect_at);
      } else if (cmd == MENU_ID_BACK) {
        browser->go_back(browser);
      } else if (cmd == MENU_ID_FORWARD) {
        browser->go_forward(browser);
      } else if (cmd == MENU_ID_RELOAD) {
        browser->reload(browser);
      } else if (cmd == MENU_ID_PRINT) {
        host->print(host);
      } else if (cmd == 3001) {
        if (link_url_str) {
          browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
          if (win_ctx) {
            CreateNewTab(win_ctx, link_url_str);
          }
        }
      } else if (cmd == 3004) {
        if (link_url_str) {
          browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
          if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
            tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
            if (!active_tab->is_split) {
              CreateRightSplitBrowser(win_ctx, active_tab, link_url_str);
            } else {
              int target_side = 1 - active_tab->active_split;
              cef_browser_t* target_browser = (target_side == 1) ? active_tab->right_browser : active_tab->browser;
              if (!target_browser && target_side == 1) {
                CreateRightSplitBrowser(win_ctx, active_tab, link_url_str);
              } else if (target_browser) {
                cef_frame_t* target_frame = target_browser->get_main_frame(target_browser);
                if (target_frame) {
                  cef_string_t url_str = {};
                  cef_string_from_utf8(link_url_str, strlen(link_url_str), &url_str);
                  target_frame->load_url(target_frame, &url_str);
                  cef_string_clear(&url_str);
                  target_frame->base.release(&target_frame->base);
                }
              }
              active_tab->active_split = target_side;
              update_ui_nav_state(win_ctx);
              InvalidateRect(win_ctx->main_hwnd, NULL, FALSE);
            }
          }
        }
      } else if (cmd == 3002) {
        if (link_url_str) {
          cef_string_t cef_url = {};
          cef_string_from_utf8(link_url_str, strlen(link_url_str), &cef_url);
          host->start_download(host, &cef_url);
          cef_string_clear(&cef_url);
        }
      } else if (cmd == 3003) {
        if (link_url_str) {
          if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            int len = MultiByteToWideChar(CP_UTF8, 0, link_url_str, -1, NULL, 0);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
            if (hMem) {
              wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
              if (pMem) {
                MultiByteToWideChar(CP_UTF8, 0, link_url_str, -1, pMem, len);
              }
              GlobalUnlock(hMem);
              SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
          }
        }
      } else if (cmd == MENU_ID_VIEW_SOURCE) {
        LogMsg("on_before_context_menu: view source trigger\n");
        browser_window_t* win_ctx = ctx_handler->parent->window_ctx;
        if (win_ctx && win_ctx->active_tab_index >= 0) {
          char vs_url[1200] = "view-source:";
          strncat(vs_url, win_ctx->tabs[win_ctx->active_tab_index].url, sizeof(vs_url) - 13);
          
          RECT rect;
          GetClientRect(win_ctx->main_hwnd, &rect);
          int width = rect.right;
          int height = rect.bottom;
          
          cef_browser_settings_t browser_settings = {};
          browser_settings.size = sizeof(cef_browser_settings_t);
          
          cef_window_info_t content_window_info = {};
          content_window_info.size = sizeof(cef_window_info_t);
          content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
          content_window_info.parent_window = win_ctx->main_hwnd;
          content_window_info.bounds.x = 1;
          content_window_info.bounds.y = 101;
          content_window_info.bounds.width = width - 2;
          content_window_info.bounds.height = height - 102;
          content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
          
          cef_string_t content_url = {};
          cef_string_from_utf8(vs_url, strlen(vs_url), &content_url);
          
          simple_handler_t *content_handler = simple_handler_create(0);
          content_handler->window_ctx = win_ctx;
          
          int next_idx = win_ctx->tab_count;
          int max_id = 0;
          for(int k=0; k<win_ctx->tab_count; k++) {
            if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
          }
          win_ctx->tabs[next_idx].tab_id = max_id + 1;
          win_ctx->tabs[next_idx].browser = NULL;
          win_ctx->tabs[next_idx].hwnd = NULL;
          strcpy(win_ctx->tabs[next_idx].title, "소스 보기");
          strcpy(win_ctx->tabs[next_idx].url, vs_url);
          win_ctx->tab_count++;
          
          cef_browser_host_create_browser(
              &content_window_info, &content_handler->client, &content_url,
              &browser_settings, NULL, NULL);
          cef_string_clear(&content_url);
        }
      }
    }
    
    host->base.release(&host->base);
  }
  
  if (link_url_str) {
    free(link_url_str);
  }
  
  LogMsg("on_before_context_menu: end\n");
}

simple_context_menu_handler_t *context_menu_handler_create(simple_handler_t *parent) {
  simple_context_menu_handler_t *handler =
      (simple_context_menu_handler_t *)calloc(1, sizeof(simple_context_menu_handler_t));
  CHECK(handler);

  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_context_menu_handler_t,
                           context_menu_handler);

  handler->handler.on_before_context_menu = context_menu_on_before_context_menu;
  handler->handler.on_context_menu_command = NULL;
  handler->handler.run_context_menu = NULL;
  handler->parent = parent;

  atomic_store(&handler->ref_count, 1);

  return handler;
}

void CreateNewTab(browser_window_t* win_ctx, const char* url) {
  if (!win_ctx) return;
  if (win_ctx->tab_count >= MAX_TABS) return;

  RECT rect;
  GetClientRect(win_ctx->main_hwnd, &rect);
  int width = rect.right;
  int height = rect.bottom;

  int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
  int content_y = ui_height + 1;
  int content_h = height - content_y - 1;

  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  cef_window_info_t content_window_info = {};
  content_window_info.size = sizeof(cef_window_info_t);
  content_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  content_window_info.parent_window = win_ctx->main_hwnd;
  content_window_info.bounds.x = 1;
  content_window_info.bounds.y = content_y;
  content_window_info.bounds.width = width - 2;
  content_window_info.bounds.height = content_h;
  content_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t content_url = {};
  if (url && strlen(url) > 0) {
    cef_string_from_utf8(url, strlen(url), &content_url);
  } else {
    cef_string_from_ascii("lite://favorites", 16, &content_url);
  }

  simple_handler_t *content_handler = simple_handler_create(0);
  content_handler->window_ctx = win_ctx;

  int insert_idx = win_ctx->tab_count;
  if (win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
    insert_idx = win_ctx->active_tab_index + 1;
  }

  int max_id = 0;
  for (int k = 0; k < win_ctx->tab_count; k++) {
    if (win_ctx->tabs[k].tab_id > max_id) max_id = win_ctx->tabs[k].tab_id;
  }

  for (int i = win_ctx->tab_count; i > insert_idx; i--) {
    win_ctx->tabs[i] = win_ctx->tabs[i - 1];
  }

  memset(&win_ctx->tabs[insert_idx], 0, sizeof(tab_info_t));
  win_ctx->tabs[insert_idx].tab_id = max_id + 1;
  win_ctx->tabs[insert_idx].browser = NULL;
  win_ctx->tabs[insert_idx].hwnd = NULL;
  strcpy(win_ctx->tabs[insert_idx].title, "새 탭");
  if (url && strlen(url) > 0) {
    strncpy(win_ctx->tabs[insert_idx].url, url, sizeof(win_ctx->tabs[insert_idx].url) - 1);
  } else {
    strcpy(win_ctx->tabs[insert_idx].url, "lite://favorites");
  }
  win_ctx->tabs[insert_idx].is_loaded = 0;
  win_ctx->tabs[insert_idx].tab_handler = content_handler;
  win_ctx->active_tab_index = insert_idx;
  win_ctx->tab_count++;
  update_ui_tabs(win_ctx);

  cef_browser_host_create_browser(
      &content_window_info, &content_handler->client, &content_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&content_url);
}

void CreateRightSplitBrowser(browser_window_t* win_ctx, tab_info_t* tab, const char* initial_url) {
  if (!win_ctx || !tab) return;

  const char* target_url = (initial_url && strlen(initial_url) > 0) ? initial_url : "lite://favorites";

  RECT rect;
  GetClientRect(win_ctx->main_hwnd, &rect);
  int width = rect.right;
  int height = rect.bottom;

  int ui_height = GetUIHeightForWindow(win_ctx->main_hwnd);
  int content_y = ui_height + 1;
  int content_h = height - content_y - 1;
  int content_w = width - 2;

  tab->is_split = 1;
  if (tab->split_ratio <= 0.1f || tab->split_ratio >= 0.9f) {
    tab->split_ratio = 0.5f;
  }
  tab->active_split = 1;
  tab->right_is_loaded = 0;
  strncpy(tab->right_url, target_url, sizeof(tab->right_url) - 1);
  strcpy(tab->right_title, "북마크 관리자");

  int split_bar_w = 6;
  int left_w = (int)((content_w - split_bar_w) * tab->split_ratio);
  int right_w = content_w - split_bar_w - left_w;
  int right_x = 1 + left_w + split_bar_w;

  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  cef_window_info_t right_window_info = {};
  right_window_info.size = sizeof(cef_window_info_t);
  right_window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  right_window_info.parent_window = win_ctx->main_hwnd;
  right_window_info.bounds.x = right_x + 2;
  right_window_info.bounds.y = content_y + 2;
  right_window_info.bounds.width = right_w - 4;
  right_window_info.bounds.height = content_h - 4;
  right_window_info.runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  cef_string_t content_url = {};
  cef_string_from_utf8(target_url, strlen(target_url), &content_url);

  simple_handler_t *right_handler = simple_handler_create(0);
  right_handler->window_ctx = win_ctx;
  tab->right_tab_handler = right_handler;

  cef_browser_host_create_browser(
      &right_window_info, &right_handler->client, &content_url,
      &browser_settings, NULL, NULL);
  cef_string_clear(&content_url);

  SendMessage(win_ctx->main_hwnd, WM_SIZE, 0, MAKELPARAM(width, height));
  update_ui_nav_state(win_ctx);
}

IMPLEMENT_REFCOUNTING_SIMPLE(simple_focus_handler_t, focus_handler, ref_count)

int CEF_CALLBACK focus_handler_on_set_focus(cef_focus_handler_t* self,
                                             cef_browser_t* browser,
                                             cef_focus_source_t source) {
  simple_focus_handler_t* handler = (simple_focus_handler_t*)self;
  browser_window_t *win_ctx = (handler && handler->parent) ? handler->parent->window_ctx : NULL;
  if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
    tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
    if (active_tab->is_split && active_tab->right_browser) {
      int new_split = -1;
      if (active_tab->browser &&
          browser->get_identifier(browser) == active_tab->browser->get_identifier(active_tab->browser)) {
        new_split = 0;
      } else if (active_tab->right_browser &&
                 browser->get_identifier(browser) == active_tab->right_browser->get_identifier(active_tab->right_browser)) {
        new_split = 1;
      }
      if (new_split != -1 && active_tab->active_split != new_split) {
        active_tab->active_split = new_split;
        update_ui_nav_state(win_ctx);
        InvalidateRect(win_ctx->main_hwnd, NULL, FALSE);
      }
    }
  }
  return 0;
}

void CEF_CALLBACK focus_handler_on_got_focus(cef_focus_handler_t* self,
                                             cef_browser_t* browser) {
  simple_focus_handler_t* handler = (simple_focus_handler_t*)self;
  browser_window_t *win_ctx = (handler && handler->parent) ? handler->parent->window_ctx : NULL;
  if (win_ctx && win_ctx->active_tab_index >= 0 && win_ctx->active_tab_index < win_ctx->tab_count) {
    tab_info_t* active_tab = &win_ctx->tabs[win_ctx->active_tab_index];
    if (active_tab->is_split && active_tab->right_browser) {
      int new_split = -1;
      if (active_tab->browser &&
          browser->get_identifier(browser) == active_tab->browser->get_identifier(active_tab->browser)) {
        new_split = 0;
      } else if (active_tab->right_browser &&
                 browser->get_identifier(browser) == active_tab->right_browser->get_identifier(active_tab->right_browser)) {
        new_split = 1;
      }
      if (new_split != -1 && active_tab->active_split != new_split) {
        active_tab->active_split = new_split;
        update_ui_nav_state(win_ctx);
        InvalidateRect(win_ctx->main_hwnd, NULL, FALSE);
      }
    }
  }
}

simple_focus_handler_t *focus_handler_create(simple_handler_t *parent) {
  simple_focus_handler_t *handler = (simple_focus_handler_t *)calloc(1, sizeof(simple_focus_handler_t));
  CHECK(handler);

  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_focus_handler_t, focus_handler);

  handler->handler.on_take_focus = NULL;
  handler->handler.on_set_focus = focus_handler_on_set_focus;
  handler->handler.on_got_focus = focus_handler_on_got_focus;
  handler->parent = parent;

  atomic_store(&handler->ref_count, 1);
  return handler;
}
