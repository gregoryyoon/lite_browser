// Copyright (c) 2026 Lite Browser. All rights reserved.

#include "tests/cefsimple_capi/simple_download_handler.h"

#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tests/cefsimple_capi/browser_context.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

#define MAX_DOWNLOAD_RECORDS 500

typedef struct {
  uint32_t id;
  char full_path[MAX_PATH];
  char url[4096];
  char file_name[MAX_PATH];
  char mime_type[256];
  int64_t total_bytes;
  int64_t received_bytes;
  int percent_complete;
  int64_t current_speed;
  int is_in_progress;
  int is_complete;
  int is_canceled;
  int is_interrupted;
  int file_exists;
  int64_t start_time;
  int64_t end_time;
  cef_download_item_callback_t *active_callback;
} download_record_t;

static download_record_t g_download_records[MAX_DOWNLOAD_RECORDS];
static int g_download_record_count = 0;
static CRITICAL_SECTION g_download_cs;
static int g_download_cs_initialized = 0;

static void EnsureCriticalSectionInitialized(void) {
  if (!g_download_cs_initialized) {
    InitializeCriticalSection(&g_download_cs);
    g_download_cs_initialized = 1;
  }
}

static void GetDownloadsJsonFilePath(char *out_path, size_t max_len) {
  char profile_path[MAX_PATH] = {0};
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, TRUE)) {
    snprintf(out_path, max_len, "%s\\.lite-browser\\downloads.json", profile_path);
  } else {
    snprintf(out_path, max_len, "C:\\downloads.json");
  }
}

static void EnsureAppDirExists(void) {
  char profile_path[MAX_PATH] = {0};
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, TRUE)) {
    char app_dir[MAX_PATH];
    snprintf(app_dir, sizeof(app_dir), "%s\\.lite-browser", profile_path);
    CreateDirectoryA(app_dir, NULL);
  }
}

static void EscapeJsonStringLocal(const char *in, char *out, size_t out_size) {
  if (!in || !out || out_size == 0) return;
  size_t j = 0;
  for (size_t i = 0; in[i] != '\0' && j < out_size - 6; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == '"') {
      out[j++] = '\\'; out[j++] = '"';
    } else if (c == '\\') {
      out[j++] = '\\'; out[j++] = '\\';
    } else if (c == '\n') {
      out[j++] = '\\'; out[j++] = 'n';
    } else if (c == '\r') {
      out[j++] = '\\'; out[j++] = 'r';
    } else if (c == '\t') {
      out[j++] = '\\'; out[j++] = 't';
    } else if (c < 32) {
      j += snprintf(out + j, out_size - j, "\\u%04x", c);
    } else {
      out[j++] = (char)c;
    }
  }
  out[j] = '\0';
}

static void SaveDownloadsHistoryToDisk(void) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  EnsureAppDirExists();
  char json_path[MAX_PATH];
  GetDownloadsJsonFilePath(json_path, sizeof(json_path));

  FILE *fp = fopen(json_path, "wb");
  if (fp) {
    fputs("[\n", fp);
    int written = 0;
    for (int i = 0; i < g_download_record_count; i++) {
      download_record_t *rec = &g_download_records[i];
      if (written > 0) fputs(",\n", fp);

      char esc_path[MAX_PATH * 2] = {0};
      char esc_url[8192] = {0};
      char esc_name[MAX_PATH * 2] = {0};
      char esc_mime[512] = {0};

      EscapeJsonStringLocal(rec->full_path, esc_path, sizeof(esc_path));
      EscapeJsonStringLocal(rec->url, esc_url, sizeof(esc_url));
      EscapeJsonStringLocal(rec->file_name, esc_name, sizeof(esc_name));
      EscapeJsonStringLocal(rec->mime_type, esc_mime, sizeof(esc_mime));

      int file_exists = (rec->full_path[0] != '\0' && GetFileAttributesA(rec->full_path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;

      fprintf(fp,
              "  {\n"
              "    \"id\": %u,\n"
              "    \"full_path\": \"%s\",\n"
              "    \"url\": \"%s\",\n"
              "    \"file_name\": \"%s\",\n"
              "    \"mime_type\": \"%s\",\n"
              "    \"total_bytes\": %lld,\n"
              "    \"received_bytes\": %lld,\n"
              "    \"percent_complete\": %d,\n"
              "    \"current_speed\": %lld,\n"
              "    \"is_in_progress\": %s,\n"
              "    \"is_complete\": %s,\n"
              "    \"is_canceled\": %s,\n"
              "    \"is_interrupted\": %s,\n"
              "    \"file_exists\": %s,\n"
              "    \"start_time\": %lld,\n"
              "    \"end_time\": %lld\n"
              "  }",
              rec->id, esc_path, esc_url, esc_name, esc_mime,
              (long long)rec->total_bytes, (long long)rec->received_bytes,
              rec->percent_complete, (long long)rec->current_speed,
              rec->is_in_progress ? "true" : "false",
              rec->is_complete ? "true" : "false",
              rec->is_canceled ? "true" : "false",
              rec->is_interrupted ? "true" : "false",
              file_exists ? "true" : "false",
              (long long)rec->start_time, (long long)rec->end_time);
      written++;
    }
    fputs("\n]\n", fp);
    fclose(fp);
  }

  LeaveCriticalSection(&g_download_cs);
}

void download_manager_init(void) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  char json_path[MAX_PATH];
  GetDownloadsJsonFilePath(json_path, sizeof(json_path));

  FILE *fp = fopen(json_path, "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size > 0 && size < 5 * 1024 * 1024) {
      char *buffer = (char*)malloc(size + 1);
      if (buffer) {
        fread(buffer, 1, size, fp);
        buffer[size] = '\0';

        // Parse simplified JSON items
        char *p = buffer;
        while ((p = strstr(p, "\"id\":")) != NULL && g_download_record_count < MAX_DOWNLOAD_RECORDS) {
          download_record_t rec;
          memset(&rec, 0, sizeof(rec));

          unsigned int id_val = 0;
          if (sscanf(p, "\"id\": %u", &id_val) == 1) {
            rec.id = id_val;
          }

          char *p_path = strstr(p, "\"full_path\": \"");
          if (p_path) {
            p_path += 14;
            char *end_q = strchr(p_path, '"');
            if (end_q && (size_t)(end_q - p_path) < sizeof(rec.full_path)) {
              strncpy(rec.full_path, p_path, end_q - p_path);
            }
          }

          char *p_url = strstr(p, "\"url\": \"");
          if (p_url) {
            p_url += 8;
            char *end_q = strchr(p_url, '"');
            if (end_q && (size_t)(end_q - p_url) < sizeof(rec.url)) {
              strncpy(rec.url, p_url, end_q - p_url);
            }
          }

          char *p_name = strstr(p, "\"file_name\": \"");
          if (p_name) {
            p_name += 14;
            char *end_q = strchr(p_name, '"');
            if (end_q && (size_t)(end_q - p_name) < sizeof(rec.file_name)) {
              strncpy(rec.file_name, p_name, end_q - p_name);
            }
          }

          char *p_mime = strstr(p, "\"mime_type\": \"");
          if (p_mime) {
            p_mime += 14;
            char *end_q = strchr(p_mime, '"');
            if (end_q && (size_t)(end_q - p_mime) < sizeof(rec.mime_type)) {
              strncpy(rec.mime_type, p_mime, end_q - p_mime);
            }
          }

          char *p_tot = strstr(p, "\"total_bytes\": ");
          if (p_tot) sscanf(p_tot, "\"total_bytes\": %lld", (long long*)&rec.total_bytes);

          char *p_rec = strstr(p, "\"received_bytes\": ");
          if (p_rec) sscanf(p_rec, "\"received_bytes\": %lld", (long long*)&rec.received_bytes);

          char *p_pct = strstr(p, "\"percent_complete\": ");
          if (p_pct) sscanf(p_pct, "\"percent_complete\": %d", &rec.percent_complete);

          char *p_cmp = strstr(p, "\"is_complete\": ");
          if (p_cmp && strncmp(p_cmp + 15, "true", 4) == 0) rec.is_complete = 1;

          char *p_can = strstr(p, "\"is_canceled\": ");
          if (p_can && strncmp(p_can + 15, "true", 4) == 0) rec.is_canceled = 1;

          char *p_int = strstr(p, "\"is_interrupted\": ");
          if (p_int && strncmp(p_int + 18, "true", 4) == 0) rec.is_interrupted = 1;

          char *p_st = strstr(p, "\"start_time\": ");
          if (p_st) sscanf(p_st, "\"start_time\": %lld", (long long*)&rec.start_time);

          char *p_et = strstr(p, "\"end_time\": ");
          if (p_et) sscanf(p_et, "\"end_time\": %lld", (long long*)&rec.end_time);

          rec.is_in_progress = 0; // Loaded items are non-active historical items
          rec.file_exists = (rec.full_path[0] != '\0' && GetFileAttributesA(rec.full_path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;

          if (rec.id > 0) {
            g_download_records[g_download_record_count++] = rec;
          }
          p++;
        }
        free(buffer);
      }
    }
    fclose(fp);
  }

  LeaveCriticalSection(&g_download_cs);
}

static void BroadcastDownloadUpdate(void) {
  simple_handler_t *handler = simple_handler_get_instance();
  if (!handler || !handler->window_ctx) return;
  browser_window_t *win_ctx = handler->window_ctx;

  char *buf = (char*)malloc(1024 * 1024);
  if (!buf) return;

  download_manager_get_list_json(buf, 1024 * 1024);

  char *js_code = (char*)malloc(1024 * 1024 + 128);
  if (js_code) {
    snprintf(js_code, 1024 * 1024 + 128, "if (window.renderDownloads) { window.renderDownloads(%s); }", buf);
    cef_string_t js_str = {};
    cef_string_from_utf8(js_code, strlen(js_code), &js_str);

    for (int i = 0; i < win_ctx->tab_count; i++) {
      cef_browser_t *b = win_ctx->tabs[i].browser;
      if (b) {
        cef_frame_t *f = b->get_main_frame(b);
        if (f) {
          f->execute_java_script(f, &js_str, NULL, 0);
          f->base.release(&f->base);
        }
      }
    }

    cef_string_clear(&js_str);
    free(js_code);
  }

  free(buf);
}

static void UpdateDownloadRecord(uint32_t id, const char *full_path, const char *url,
                                 const char *file_name, const char *mime_type,
                                 int64_t total_bytes, int64_t received_bytes,
                                 int percent_complete, int64_t speed,
                                 int is_in_progress, int is_complete,
                                 int is_canceled, int is_interrupted,
                                 cef_download_item_callback_t *callback) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  download_record_t *rec = NULL;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id) {
      rec = &g_download_records[i];
      break;
    }
  }

  if (!rec) {
    if (g_download_record_count < MAX_DOWNLOAD_RECORDS) {
      // Prepend to show latest downloads first
      memmove(&g_download_records[1], &g_download_records[0],
              g_download_record_count * sizeof(download_record_t));
      rec = &g_download_records[0];
      g_download_record_count++;
      memset(rec, 0, sizeof(download_record_t));
      rec->id = id;
      rec->start_time = (int64_t)time(NULL);
    } else {
      rec = &g_download_records[0];
    }
  }

  if (full_path && full_path[0] != '\0') strncpy(rec->full_path, full_path, sizeof(rec->full_path) - 1);
  if (url && url[0] != '\0') strncpy(rec->url, url, sizeof(rec->url) - 1);
  if (file_name && file_name[0] != '\0') strncpy(rec->file_name, file_name, sizeof(rec->file_name) - 1);
  if (mime_type && mime_type[0] != '\0') strncpy(rec->mime_type, mime_type, sizeof(rec->mime_type) - 1);

  rec->total_bytes = total_bytes;
  rec->received_bytes = received_bytes;
  rec->percent_complete = percent_complete;
  rec->current_speed = speed;
  rec->is_in_progress = is_in_progress;
  rec->is_complete = is_complete;
  rec->is_canceled = is_canceled;
  rec->is_interrupted = is_interrupted;

  if (is_complete || is_canceled || is_interrupted) {
    rec->end_time = (int64_t)time(NULL);
    if (rec->active_callback) {
      rec->active_callback->base.release(&rec->active_callback->base);
      rec->active_callback = NULL;
    }
  } else if (is_in_progress && callback) {
    if (rec->active_callback != callback) {
      if (rec->active_callback) {
        rec->active_callback->base.release(&rec->active_callback->base);
      }
      rec->active_callback = callback;
      rec->active_callback->base.add_ref(&rec->active_callback->base);
    }
  }

  rec->file_exists = (rec->full_path[0] != '\0' && GetFileAttributesA(rec->full_path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;

  LeaveCriticalSection(&g_download_cs);

  SaveDownloadsHistoryToDisk();
}

static void GetDefaultDownloadsDirectory(char *out_path, size_t max_len) {
  char profile_path[MAX_PATH] = {0};
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, TRUE)) {
    snprintf(out_path, max_len, "%s\\Downloads", profile_path);
  } else {
    snprintf(out_path, max_len, "C:\\Downloads");
  }
  CreateDirectoryA(out_path, NULL);
}

static void GenerateNonConflictingPath(const char *downloads_dir, const char *suggested_name, char *out_path, size_t max_len) {
  char stem[MAX_PATH] = {0};
  char ext[64] = {0};

  const char *dot = strrchr(suggested_name, '.');
  if (dot && dot != suggested_name) {
    size_t stem_len = dot - suggested_name;
    if (stem_len >= sizeof(stem)) stem_len = sizeof(stem) - 1;
    strncpy(stem, suggested_name, stem_len);
    stem[stem_len] = '\0';
    strncpy(ext, dot, sizeof(ext) - 1);
  } else {
    strncpy(stem, suggested_name, sizeof(stem) - 1);
    ext[0] = '\0';
  }

  snprintf(out_path, max_len, "%s\\%s", downloads_dir, suggested_name);
  if (GetFileAttributesA(out_path) == INVALID_FILE_ATTRIBUTES) {
    return;
  }

  for (int i = 1; i <= 9999; i++) {
    snprintf(out_path, max_len, "%s\\%s (%d)%s", downloads_dir, stem, i, ext);
    if (GetFileAttributesA(out_path) == INVALID_FILE_ATTRIBUTES) {
      return;
    }
  }
}

// CEF Download Handler Callback implementations

int CEF_CALLBACK download_handler_can_download(
    cef_download_handler_t *self,
    cef_browser_t *browser,
    const cef_string_t *url,
    const cef_string_t *request_method) {
  return 1; // Always allow download
}

int CEF_CALLBACK download_handler_on_before_download(
    cef_download_handler_t *self,
    cef_browser_t *browser,
    cef_download_item_t *download_item,
    const cef_string_t *suggested_name,
    cef_before_download_callback_t *callback) {

  char dir[MAX_PATH] = {0};
  GetDefaultDownloadsDirectory(dir, sizeof(dir));

  char name_utf8[MAX_PATH] = "download";
  if (suggested_name && suggested_name->str && suggested_name->length > 0) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(suggested_name->str, suggested_name->length, &u8);
    if (u8.str && u8.length > 0) {
      strncpy(name_utf8, u8.str, sizeof(name_utf8) - 1);
    }
    cef_string_utf8_clear(&u8);
  }

  char full_path[MAX_PATH] = {0};
  GenerateNonConflictingPath(dir, name_utf8, full_path, sizeof(full_path));

  cef_string_t cef_path = {};
  cef_string_from_utf8(full_path, strlen(full_path), &cef_path);

  // show_dialog = 0: Save directly to downloads directory without opening Save As dialog
  callback->cont(callback, &cef_path, 0);

  cef_string_clear(&cef_path);
  return 1;
}

void CEF_CALLBACK download_handler_on_download_updated(
    cef_download_handler_t *self,
    cef_browser_t *browser,
    cef_download_item_t *download_item,
    cef_download_item_callback_t *callback) {

  if (!download_item || !download_item->is_valid(download_item)) return;

  uint32_t id = download_item->get_id(download_item);
  int is_in_progress = download_item->is_in_progress(download_item);
  int is_complete = download_item->is_complete(download_item);
  int is_canceled = download_item->is_canceled(download_item);
  int is_interrupted = download_item->is_interrupted(download_item);

  int64_t total = download_item->get_total_bytes(download_item);
  int64_t received = download_item->get_received_bytes(download_item);
  int percent = download_item->get_percent_complete(download_item);
  int64_t speed = download_item->get_current_speed(download_item);

  char full_path[MAX_PATH] = {0};
  cef_string_userfree_t fp = download_item->get_full_path(download_item);
  if (fp) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(fp->str, fp->length, &u8);
    if (u8.str) strncpy(full_path, u8.str, sizeof(full_path) - 1);
    cef_string_utf8_clear(&u8);
    cef_string_userfree_free(fp);
  }

  char url_str[4096] = {0};
  cef_string_userfree_t u = download_item->get_url(download_item);
  if (u) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(u->str, u->length, &u8);
    if (u8.str) strncpy(url_str, u8.str, sizeof(url_str) - 1);
    cef_string_utf8_clear(&u8);
    cef_string_userfree_free(u);
  }

  char file_name[MAX_PATH] = {0};
  cef_string_userfree_t fn = download_item->get_suggested_file_name(download_item);
  if (fn) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(fn->str, fn->length, &u8);
    if (u8.str) strncpy(file_name, u8.str, sizeof(file_name) - 1);
    cef_string_utf8_clear(&u8);
    cef_string_userfree_free(fn);
  }
  if (file_name[0] == '\0' && full_path[0] != '\0') {
    const char *slash = strrchr(full_path, '\\');
    if (!slash) slash = strrchr(full_path, '/');
    if (slash) strncpy(file_name, slash + 1, sizeof(file_name) - 1);
    else strncpy(file_name, full_path, sizeof(file_name) - 1);
  }

  char mime_type[256] = {0};
  cef_string_userfree_t mt = download_item->get_mime_type(download_item);
  if (mt) {
    cef_string_utf8_t u8 = {};
    cef_string_to_utf8(mt->str, mt->length, &u8);
    if (u8.str) strncpy(mime_type, u8.str, sizeof(mime_type) - 1);
    cef_string_utf8_clear(&u8);
    cef_string_userfree_free(mt);
  }

  UpdateDownloadRecord(id, full_path, url_str, file_name, mime_type,
                       total, received, percent, speed,
                       is_in_progress, is_complete, is_canceled, is_interrupted,
                       callback);
}

// Implement ref counting for simple_download_handler_t
IMPLEMENT_REFCOUNTING_SIMPLE(simple_download_handler_t, download_handler, ref_count)

simple_download_handler_t *download_handler_create(simple_handler_t *parent) {
  simple_download_handler_t *h = (simple_download_handler_t*)calloc(1, sizeof(simple_download_handler_t));
  if (!h) return NULL;

  INIT_CEF_BASE_REFCOUNTED(&h->handler.base, cef_download_handler_t, download_handler);
  h->ref_count = 1;
  h->parent = parent;

  h->handler.can_download = download_handler_can_download;
  h->handler.on_before_download = download_handler_on_before_download;
  h->handler.on_download_updated = download_handler_on_download_updated;

  return h;
}

// Exported Download Manager IPC functions

void download_manager_get_list_json(char *out_buf, size_t max_len) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  size_t offset = 0;
  offset += snprintf(out_buf + offset, max_len - offset, "[\n");

  for (int i = 0; i < g_download_record_count && offset < max_len - 512; i++) {
    download_record_t *rec = &g_download_records[i];
    if (i > 0) offset += snprintf(out_buf + offset, max_len - offset, ",\n");

    char esc_path[MAX_PATH * 2] = {0};
    char esc_url[8192] = {0};
    char esc_name[MAX_PATH * 2] = {0};
    char esc_mime[512] = {0};

    EscapeJsonStringLocal(rec->full_path, esc_path, sizeof(esc_path));
    EscapeJsonStringLocal(rec->url, esc_url, sizeof(esc_url));
    EscapeJsonStringLocal(rec->file_name, esc_name, sizeof(esc_name));
    EscapeJsonStringLocal(rec->mime_type, esc_mime, sizeof(esc_mime));

    int file_exists = (rec->full_path[0] != '\0' && GetFileAttributesA(rec->full_path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
    rec->file_exists = file_exists;

    offset += snprintf(out_buf + offset, max_len - offset,
      "  {\n"
      "    \"id\": %u,\n"
      "    \"full_path\": \"%s\",\n"
      "    \"url\": \"%s\",\n"
      "    \"file_name\": \"%s\",\n"
      "    \"mime_type\": \"%s\",\n"
      "    \"total_bytes\": %lld,\n"
      "    \"received_bytes\": %lld,\n"
      "    \"percent_complete\": %d,\n"
      "    \"current_speed\": %lld,\n"
      "    \"is_in_progress\": %s,\n"
      "    \"is_complete\": %s,\n"
      "    \"is_canceled\": %s,\n"
      "    \"is_interrupted\": %s,\n"
      "    \"file_exists\": %s,\n"
      "    \"start_time\": %lld,\n"
      "    \"end_time\": %lld\n"
      "  }",
      rec->id, esc_path, esc_url, esc_name, esc_mime,
      (long long)rec->total_bytes, (long long)rec->received_bytes,
      rec->percent_complete, (long long)rec->current_speed,
      rec->is_in_progress ? "true" : "false",
      rec->is_complete ? "true" : "false",
      rec->is_canceled ? "true" : "false",
      rec->is_interrupted ? "true" : "false",
      rec->file_exists ? "true" : "false",
      (long long)rec->start_time, (long long)rec->end_time);
  }

  snprintf(out_buf + offset, max_len - offset, "\n]\n");

  LeaveCriticalSection(&g_download_cs);
}

int download_manager_open_file(const char *path) {
  if (!path || path[0] == '\0') return 0;
  HINSTANCE hInst = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
  return ((INT_PTR)hInst > 32) ? 1 : 0;
}

int download_manager_show_in_folder(const char *path) {
  if (!path || path[0] == '\0') return 0;
  char cmd_args[MAX_PATH + 32];
  snprintf(cmd_args, sizeof(cmd_args), "/select,\"%s\"", path);
  HINSTANCE hInst = ShellExecuteA(NULL, "open", "explorer.exe", cmd_args, NULL, SW_SHOWNORMAL);
  return ((INT_PTR)hInst > 32) ? 1 : 0;
}

int download_manager_delete_file(uint32_t id, const char *path) {
  int success = 0;
  if (path && path[0] != '\0') {
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
      if (DeleteFileA(path)) {
        success = 1;
      }
    }
  }

  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id) {
      g_download_records[i].file_exists = 0;
      break;
    }
  }
  LeaveCriticalSection(&g_download_cs);

  SaveDownloadsHistoryToDisk();
  return success;
}

int download_manager_remove_history(uint32_t id) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  int removed = 0;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id) {
      if (g_download_records[i].active_callback) {
        g_download_records[i].active_callback->base.release(&g_download_records[i].active_callback->base);
      }
      for (int j = i; j < g_download_record_count - 1; j++) {
        g_download_records[j] = g_download_records[j + 1];
      }
      g_download_record_count--;
      removed = 1;
      break;
    }
  }

  LeaveCriticalSection(&g_download_cs);

  if (removed) SaveDownloadsHistoryToDisk();
  return removed;
}

int download_manager_clear_history(void) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);

  int new_count = 0;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].is_in_progress) {
      g_download_records[new_count++] = g_download_records[i];
    } else {
      if (g_download_records[i].active_callback) {
        g_download_records[i].active_callback->base.release(&g_download_records[i].active_callback->base);
      }
    }
  }
  g_download_record_count = new_count;

  LeaveCriticalSection(&g_download_cs);

  SaveDownloadsHistoryToDisk();
  return 1;
}

int download_manager_pause(uint32_t id) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);
  int ok = 0;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id && g_download_records[i].active_callback) {
      g_download_records[i].active_callback->pause(g_download_records[i].active_callback);
      ok = 1;
      break;
    }
  }
  LeaveCriticalSection(&g_download_cs);
  return ok;
}

int download_manager_resume(uint32_t id) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);
  int ok = 0;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id && g_download_records[i].active_callback) {
      g_download_records[i].active_callback->resume(g_download_records[i].active_callback);
      ok = 1;
      break;
    }
  }
  LeaveCriticalSection(&g_download_cs);
  return ok;
}

int download_manager_cancel(uint32_t id) {
  EnsureCriticalSectionInitialized();
  EnterCriticalSection(&g_download_cs);
  int ok = 0;
  for (int i = 0; i < g_download_record_count; i++) {
    if (g_download_records[i].id == id && g_download_records[i].active_callback) {
      g_download_records[i].active_callback->cancel(g_download_records[i].active_callback);
      ok = 1;
      break;
    }
  }
  LeaveCriticalSection(&g_download_cs);
  return ok;
}
