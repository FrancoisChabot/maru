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
} MARU_Status;

/** @brief Identifiers for formalized library extensions. */
typedef uint32_t MARU_ExtensionID;
#define MARU_EXT_INVALID ((MARU_ExtensionID)-1)

#define MARU_EXT_VULKAN 0
#define MARU_EXT_COUNT 1

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/** @brief Supported windowing backends. */
typedef enum MARU_BackendType {
  MARU_BACKEND_UNKNOWN = 0,
  MARU_BACKEND_WAYLAND = 1,
  MARU_BACKEND_X11 = 2,
  MARU_BACKEND_WINDOWS = 3,
  MARU_BACKEND_COCOA = 4,
} MARU_BackendType;


/** @brief Generic flags type. */
typedef uint64_t MARU_Flags;

/** @brief Bitmask for filtering events during polling or waiting. */
typedef uint64_t MARU_EventMask;

/** @brief Represents a single event type */
typedef uint64_t MARU_EventType;


#ifdef __cplusplus
}
#endif

#endif  // MARU_CORE_H_INCLUDED
