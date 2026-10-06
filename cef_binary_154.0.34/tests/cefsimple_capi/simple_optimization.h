#ifndef LITEBROWSER_SIMPLE_OPTIMIZATION_H_
#define LITEBROWSER_SIMPLE_OPTIMIZATION_H_

#include "include/capi/cef_command_line_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OPTIMIZATION_MODE_SPEED = 0,   // "speed" - 실행 속도 우선 (Default)
  OPTIMIZATION_MODE_MEMORY = 1   // "memory" - 메모리 절감 우선
} optimization_mode_t;

// Get current saved optimization mode from persistent storage (defaults to OPTIMIZATION_MODE_SPEED)
optimization_mode_t optimization_get_mode(void);

// Save optimization mode to persistent storage (returns 1 on success, 0 on failure)
int optimization_set_mode(optimization_mode_t mode);

// Get optimization mode active in the current running process session
optimization_mode_t optimization_get_launch_mode(void);

// Apply command line switches for the configured optimization mode
void optimization_apply_command_line_switches(cef_command_line_t* command_line);

// Convert mode enum to string ("speed" or "memory")
const char* optimization_mode_to_string(optimization_mode_t mode);

// Convert string ("memory" or "speed") to mode enum
optimization_mode_t optimization_mode_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif  // LITEBROWSER_SIMPLE_OPTIMIZATION_H_
