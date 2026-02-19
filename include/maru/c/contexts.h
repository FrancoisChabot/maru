// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONTEXT_H_INCLUDED
#define MARU_CONTEXT_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"

/**
 * @file contexts.h
 * @brief Context management, lifecycle attributes, and core state.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;

/** @brief Runtime state flags for a context. */
typedef enum MARU_ContextStateFlagBits {
  MARU_CONTEXT_STATE_LOST = 1ULL << 0,
  MARU_CONTEXT_STATE_READY = 1ULL << 1,
} MARU_ContextStateFlagBits;

/** @brief Callback function for library diagnostics. */
struct MARU_DiagnosticInfo;
typedef void (*MARU_DiagnosticCallback)(const struct MARU_DiagnosticInfo *info, void *userdata);

/** @brief Bitmask for selecting which attributes to update in maru_updateContext(). */
typedef enum MARU_ContextAttributesField {
  MARU_CONTEXT_ATTR_DIAGNOSTICS = 1ULL << 1,

  MARU_CONTEXT_ATTR_ALL = 0x7FFFFFFF,
} MARU_ContextAttributesField;

/** @brief Configurable parameters for an active context. */
typedef struct MARU_ContextAttributes {
  MARU_DiagnosticCallback diagnostic_cb;  ///< Optional callback for library diagnostics.
  void *diagnostic_userdata;              ///< Passed to the diagnostic callback.
} MARU_ContextAttributes;

/** @brief Supported windowing backends. */
typedef enum MARU_BackendType {
  MARU_BACKEND_UNKNOWN = 0,
  MARU_BACKEND_WAYLAND = 1,
  MARU_BACKEND_X11 = 2,
  MARU_BACKEND_WINDOWS = 3,
  MARU_BACKEND_COCOA = 4,
} MARU_BackendType;

/** @brief Parameters for maru_createContext(). */
typedef struct MARU_ContextCreateInfo {
  MARU_Allocator allocator;
  void *userdata;
  MARU_BackendType backend;
  MARU_ContextAttributes attributes;
} MARU_ContextCreateInfo;

#define MARU_CONTEXT_CREATE_INFO_DEFAULT                                       \
  {                                                                            \
    .allocator = {.alloc_cb = NULL,                                            \
                  .realloc_cb = NULL,                                          \
                  .free_cb = NULL,                                             \
                  .userdata = NULL},                                           \
    .userdata = NULL, .backend = MARU_BACKEND_UNKNOWN, .attributes = {         \
      .diagnostic_cb = NULL,                                                   \
      .diagnostic_userdata = NULL,                                             \
    }                                                                          \
  }

MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                               MARU_Context **out_context);

MARU_API MARU_Status maru_destroyContext(MARU_Context *context);

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes);

/* Passive Accessors */
static inline void *maru_getContextUserdata(const MARU_Context *context);
static inline void maru_setContextUserdata(MARU_Context *context, void *userdata);
static inline bool maru_isContextLost(const MARU_Context *context);
static inline bool maru_isContextReady(const MARU_Context *context);

#include "maru/c/details/contexts.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_CONTEXT_H_INCLUDED
