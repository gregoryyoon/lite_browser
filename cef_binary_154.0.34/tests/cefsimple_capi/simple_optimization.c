#include "tests/cefsimple_capi/simple_optimization.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_WIN) || defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

static optimization_mode_t g_launch_optimization_mode = OPTIMIZATION_MODE_SPEED;
static int g_launch_mode_initialized = 0;

static void get_optimization_config_path(char* out_path, size_t max_len) {
#if defined(OS_WIN) || defined(_WIN32)
  char profile_path[MAX_PATH];
  if (SHGetSpecialFolderPathA(NULL, profile_path, CSIDL_PROFILE, FALSE) ||
      GetEnvironmentVariableA("USERPROFILE", profile_path, MAX_PATH) > 0) {
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\.lite-browser", profile_path);
    CreateDirectoryA(dir_path, NULL);
    snprintf(out_path, max_len, "%s\\.lite-browser\\optimization_mode.txt", profile_path);
    return;
  }
#endif
  snprintf(out_path, max_len, ".lite_browser_optimization_mode.txt");
}

optimization_mode_t optimization_get_mode(void) {
  char config_path[MAX_PATH];
  get_optimization_config_path(config_path, sizeof(config_path));
  FILE* fp = fopen(config_path, "r");
  if (!fp) {
    return OPTIMIZATION_MODE_SPEED;
  }
  char buf[64] = {0};
  if (fgets(buf, sizeof(buf) - 1, fp)) {
    char* start = buf;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
      start++;
    }
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
      end--;
      *end = '\0';
    }
    optimization_mode_t mode = optimization_mode_from_string(start);
    fclose(fp);
    return mode;
  }
  fclose(fp);
  return OPTIMIZATION_MODE_SPEED;
}

int optimization_set_mode(optimization_mode_t mode) {
  char config_path[MAX_PATH];
  get_optimization_config_path(config_path, sizeof(config_path));
  FILE* fp = fopen(config_path, "w");
  if (!fp) {
    return 0;
  }
  const char* str = optimization_mode_to_string(mode);
  fprintf(fp, "%s\n", str);
  fflush(fp);
  fclose(fp);
  return 1;
}

optimization_mode_t optimization_get_launch_mode(void) {
  if (!g_launch_mode_initialized) {
    g_launch_optimization_mode = optimization_get_mode();
    g_launch_mode_initialized = 1;
  }
  return g_launch_optimization_mode;
}

const char* optimization_mode_to_string(optimization_mode_t mode) {
  if (mode == OPTIMIZATION_MODE_MEMORY) {
    return "memory";
  }
  return "speed";
}

optimization_mode_t optimization_mode_from_string(const char* str) {
  if (str && (_stricmp(str, "memory") == 0 || strcmp(str, "1") == 0)) {
    return OPTIMIZATION_MODE_MEMORY;
  }
  return OPTIMIZATION_MODE_SPEED;
}

static void append_switch_if_missing(cef_command_line_t* cmd, const char* name) {
  if (!cmd || !name) return;
  cef_string_t s = {};
  cef_string_from_ascii(name, strlen(name), &s);
  if (!cmd->has_switch(cmd, &s)) {
    cmd->append_switch(cmd, &s);
  }
  cef_string_clear(&s);
}

static void append_switch_with_value_if_missing(cef_command_line_t* cmd, const char* name, const char* value) {
  if (!cmd || !name || !value) return;
  cef_string_t n = {};
  cef_string_t v = {};
  cef_string_from_ascii(name, strlen(name), &n);
  cef_string_from_ascii(value, strlen(value), &v);
  if (!cmd->has_switch(cmd, &n)) {
    cmd->append_switch_with_value(cmd, &n, &v);
  }
  cef_string_clear(&n);
  cef_string_clear(&v);
}

static void append_feature_if_missing(cef_command_line_t* cmd, const char* feature_name) {
  if (!cmd || !feature_name) return;
  cef_string_t n = {};
  cef_string_from_ascii("enable-features", 15, &n);
  if (cmd->has_switch(cmd, &n)) {
    cef_string_userfree_t existing = cmd->get_switch_value(cmd, &n);
    if (existing && existing->length > 0) {
      cef_string_utf8_t u8 = {};
      cef_string_to_utf8(existing->str, existing->length, &u8);
      if (u8.str && strstr(u8.str, feature_name) == NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s,%s", u8.str, feature_name);
        cef_string_t new_val = {};
        cef_string_from_utf8(buf, strlen(buf), &new_val);
        cmd->append_switch_with_value(cmd, &n, &new_val);
        cef_string_clear(&new_val);
      }
      cef_string_utf8_clear(&u8);
      cef_string_userfree_free(existing);
    } else {
      if (existing) cef_string_userfree_free(existing);
      cef_string_t v = {};
      cef_string_from_ascii(feature_name, strlen(feature_name), &v);
      cmd->append_switch_with_value(cmd, &n, &v);
      cef_string_clear(&v);
    }
  } else {
    cef_string_t v = {};
    cef_string_from_ascii(feature_name, strlen(feature_name), &v);
    cmd->append_switch_with_value(cmd, &n, &v);
    cef_string_clear(&v);
  }
  cef_string_clear(&n);
}

void optimization_apply_command_line_switches(cef_command_line_t* command_line) {
  if (!command_line) return;

  if (!g_launch_mode_initialized) {
    g_launch_optimization_mode = optimization_get_mode();
    g_launch_mode_initialized = 1;
  }

  // Always use the launch mode for the duration of the process session
  optimization_mode_t mode = g_launch_optimization_mode;

  if (mode == OPTIMIZATION_MODE_SPEED) {
    // 1. 실행 속도 우선 (Speed Priority)
    // 하드웨어 가속 강제 활성화
    append_switch_if_missing(command_line, "enable-gpu");
    // GPU 타일 렌더링 시 메모리 복사 단계 단축
    append_switch_if_missing(command_line, "enable-zero-copy");
    // GPU 래스터화 활성화 (Zero-copy 및 하드웨어 가속 연동)
    append_switch_if_missing(command_line, "enable-gpu-rasterization");
  } else if (mode == OPTIMIZATION_MODE_MEMORY) {
    // 2. 메모리 절감 우선 (Memory Saving Priority)
    // 사이트 격리 프로세스 통합 (프로세스 수 및 메모리 사용량 대폭 절감)
    append_switch_if_missing(command_line, "disable-site-isolation-trials");
    // 렌더러 프로세스 수 최대 2개로 제한
    append_switch_with_value_if_missing(command_line, "renderer-process-limit", "2");
    // V8 JavaScript 엔진 힙 메모리 한도 256MB 제한
    append_switch_with_value_if_missing(command_line, "js-flags", "--max-old-space-size=256");
    // Chromium 메모리 절약 모드 활성화 (비활성 리소스 적극 해제)
    append_feature_if_missing(command_line, "MemorySaverMode");
    // 확장 프로그램 엔진 비활성화
    append_switch_if_missing(command_line, "disable-extensions");
  }
}
