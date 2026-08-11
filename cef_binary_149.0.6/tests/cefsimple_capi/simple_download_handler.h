// Copyright (c) 2026 Lite Browser. All rights reserved.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DOWNLOAD_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DOWNLOAD_HANDLER_H_

#include <stdatomic.h>
#include <windows.h>
#include <stdint.h>

#include "include/capi/cef_download_handler_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"

typedef struct _simple_handler_t simple_handler_t;

typedef struct _simple_download_handler_t {
  // MUST be first member - CEF base structure.
  cef_download_handler_t handler;

  // Reference count for this object.
  atomic_int ref_count;

  // Back reference to parent handler.
  simple_handler_t *parent;
} simple_download_handler_t;

// Create a download handler instance.
simple_download_handler_t *download_handler_create(simple_handler_t *parent);

// IPC & Download Management API functions
void download_manager_init(void);
void download_manager_get_list_json(char *out_buf, size_t max_len);
int download_manager_open_file(const char *path);
int download_manager_show_in_folder(const char *path);
int download_manager_delete_file(uint32_t id, const char *path);
int download_manager_remove_history(uint32_t id);
int download_manager_clear_history(void);
int download_manager_pause(uint32_t id);
int download_manager_resume(uint32_t id);
int download_manager_cancel(uint32_t id);

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_DOWNLOAD_HANDLER_H_
