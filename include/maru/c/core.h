// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CORE_H_INCLUDED
#define MARU_CORE_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "maru/c/details/maru_config.h"
/**
 * @file core.h
 * @brief Core types, versioning, and memory management.
 */

#ifndef MARU_API
#ifdef _WIN32
#ifdef MARU_BUILD_DLL
#define MARU_API __declspec(dllexport)
#else
#define MARU_API __declspec(dllimport)
#endif
#else
#define MARU_API __attribute__((visibility("default")))
#endif
#endif

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
MARU_API MARU_Version maru_getVersion(void);

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

/** @brief Generic flags type. */
typedef uint64_t MARU_Flags;

#ifdef __cplusplus
}
#endif

#endif  // MARU_CORE_H_INCLUDED
