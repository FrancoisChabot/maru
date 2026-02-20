// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CORE_H_INCLUDED
#define MARU_CORE_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "maru/c/details/maru_config.h"

#ifndef MARU_API
#ifdef MARU_STATIC
#define MARU_API
#elif defined(_WIN32)
#ifdef MARU_BUILDING_DLL
#define MARU_API __declspec(dllexport)
#else
#define MARU_API __declspec(dllimport)
#endif
#else
#define MARU_API __attribute__((visibility("default")))
#endif
#endif

/**
 * @file core.h
 * @brief Core types, versioning, and memory management.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Semantic version info. */
typedef struct MARU_Version {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} MARU_Version;

/** @brief Retrieves the linked version of the MARU library. */
MARU_Version maru_getVersion(void);

/** @brief Function pointer for memory allocation. */
typedef void *(*MARU_AllocationFunction)(size_t size, void *userdata);

/** @brief Function pointer for resizing a memory allocation. */
typedef void *(*MARU_ReallocationFunction)(void *ptr, size_t new_size, void *userdata);

/** @brief Function pointer for memory deallocation. */
typedef void (*MARU_FreeFunction)(void *ptr, void *userdata);

/** @brief Custom memory allocator configuration. */
typedef struct MARU_Allocator {
  MARU_AllocationFunction alloc_cb;
  MARU_ReallocationFunction realloc_cb;
  MARU_FreeFunction free_cb;
  void *userdata;
} MARU_Allocator;

/** @brief Return codes for MARU functions. 

The codes returned by the Maru API are not meant to tell you "What went wrong",
but rather "What you can do about it" at runtime. They are flow-control mechanisms, 
not a debugging tool.

Information as to what caused the failure is reported via diagnostics.
*/
typedef enum MARU_Status {
// All good
  MARU_SUCCESS = 0,

// The operation could not be completed, but you may carry on
  MARU_FAILURE = 1,

// The entire context was dead either before, or became dead because of, the operation.
  MARU_ERROR_CONTEXT_LOST = 2, 

  // The API was used in an invalid way.
  // Guaranteed to only ever come up in special VALIDATION builds.
  MARU_ERROR_INVALID_USAGE = -1,
} MARU_Status;

/** @brief Runtime state flags for a context. */
typedef enum MARU_ContextStateFlagBits {
  MARU_CONTEXT_STATE_LOST = 1ULL << 0,
  MARU_CONTEXT_STATE_READY = 1ULL << 1,
} MARU_ContextStateFlagBits;

/** @brief Runtime state flags for a window. */
typedef enum MARU_WindowStateFlagBits {
  MARU_WINDOW_STATE_LOST = 1ULL << 0,
  MARU_WINDOW_STATE_READY = 1ULL << 1,
  MARU_WINDOW_STATE_FOCUSED = 1ULL << 2,
  MARU_WINDOW_STATE_MAXIMIZED = 1ULL << 3,
  MARU_WINDOW_STATE_FULLSCREEN = 1ULL << 4,
  MARU_WINDOW_STATE_MOUSE_PASSTHROUGH = 1ULL << 5,
} MARU_WindowStateFlagBits;

/** @brief Runtime state flags for a cursor. */
typedef enum MARU_CursorStateFlagBits {
  MARU_CURSOR_FLAG_SYSTEM = 1ULL << 0,
} MARU_CursorStateFlagBits;

/** @brief Runtime state flags for a monitor. */
typedef enum MARU_MonitorStateFlagBits {
  MARU_MONITOR_STATE_LOST = 1ULL << 0,
} MARU_MonitorStateFlagBits;

/** @brief Generic flags type. */
typedef uint64_t MARU_Flags;

/** @brief Bitmask for filtering events during polling or waiting. */
typedef uint64_t MARU_EventMask;

/** @brief Represents a single event type */
typedef uint64_t MARU_EventType;

/** @brief Tracks whether a button is currently pressed or released. */
typedef enum MARU_ButtonState {
  MARU_BUTTON_STATE_RELEASED = 0,
  MARU_BUTTON_STATE_PRESSED = 1,
} MARU_ButtonState;

/** @brief 1-byte version of MARU_ButtonState for efficient array storage. */
typedef uint8_t MARU_ButtonState8;

/** @brief Cursor visibility and constraint modes. */
typedef enum MARU_CursorMode {
  MARU_CURSOR_NORMAL = 0,
  MARU_CURSOR_HIDDEN = 1,
  MARU_CURSOR_LOCKED = 2,
} MARU_CursorMode;

#ifdef __cplusplus
}
#endif

#endif  // MARU_CORE_H_INCLUDED
