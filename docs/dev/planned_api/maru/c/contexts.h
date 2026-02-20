/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONTEXT_H_INCLUDED
#define MARU_CONTEXT_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/metrics.h"
#include "maru/c/events.h"

/**
 * @file contexts.h
 * @brief Context management, lifecycle attributes, and core state.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;

/** @brief Forward declaration of diagnostic info. */
struct MARU_DiagnosticInfo;

/** @brief Callback function for library diagnostics. */
typedef void (*MARU_DiagnosticCallback)(const struct MARU_DiagnosticInfo *info, void *userdata);

/** @brief Runtime state flags for a context. */
enum MARU_ContextStateFlagBits {
  MARU_CONTEXT_STATE_LOST = 1ULL << 0,
  MARU_CONTEXT_STATE_READY = 1ULL << 1,
};

/* ----- Passive Accessors (External Synchronization Required) ----- 
 *
 * These functions are essentially zero-cost member accesses. They are safe to 
 * call from any thread, provided the access is synchronized with mutating 
 * operations (like maru_pumpEvents or maru_updateContext) on the same context 
 * to ensure memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a context. */
static inline void *maru_getContextUserdata(const MARU_Context *context);

/** @brief Sets the user-defined data pointer associated with a context. */
static inline void maru_setContextUserdata(MARU_Context *context, void *userdata);

/** @brief Checks if the context has been lost due to an unrecoverable error. */
static inline bool maru_isContextLost(const MARU_Context *context);

/** @brief Checks if the context is fully initialized and ready for use. */
static inline bool maru_isContextReady(const MARU_Context *context);

/** @brief Retrieves the runtime performance metrics for a context. */
static inline const MARU_ContextMetrics *maru_getContextMetrics(const MARU_Context *context);

/* ----- Context Management ----- */

/** @brief Special value for timeouts to indicate it should never trigger. */
#define MARU_NEVER (uint32_t)-1

/** @brief Bitmask for selecting which attributes to update in maru_updateContext(). */
typedef enum MARU_ContextAttributesField {
  MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE = 1ULL << 0,
  MARU_CONTEXT_ATTR_DIAGNOSTICS = 1ULL << 1,
  MARU_CONTEXT_ATTR_EVENT_MASK = 1ULL << 2,
  MARU_CONTEXT_ATTR_EVENT_CALLBACK = 1ULL << 3,

  MARU_CONTEXT_ATTR_ALL = 0xFFFFFFFFFFFFFFFFULL,
} MARU_ContextAttributesField;

/** @brief Configurable parameters for an active context. */
typedef struct MARU_ContextAttributes {
  bool inhibit_idle;                      ///< If true, prevents the OS from entering sleep.
  MARU_DiagnosticCallback diagnostic_cb;  ///< Optional callback for library diagnostics.
  void *diagnostic_userdata;              ///< Passed to the diagnostic callback.
  MARU_EventMask event_mask;              ///< Bitmask of events to allow.
  MARU_EventCallback event_callback;      ///< Primary event sink.
} MARU_ContextAttributes;

/** @brief Supported windowing backends. */
typedef enum MARU_BackendType {
  MARU_BACKEND_UNKNOWN = 0,
  MARU_BACKEND_WAYLAND = 1,
  MARU_BACKEND_X11 = 2,
  MARU_BACKEND_WINDOWS = 3,
  MARU_BACKEND_COCOA = 4,
} MARU_BackendType;

/** @brief Forward declaration of tuning parameters. */
struct MARU_ContextTuning; // Forward declared, defined in tuning.h

/** @brief Parameters for maru_createContext(). */
typedef struct MARU_ContextCreateInfo {
  MARU_Allocator allocator;           ///< Custom memory allocator.
  void *userdata;                     ///< Initial value for MARU_Context::userdata.
  MARU_BackendType backend;           ///< Requested backend, if set to MARU_BACKEND_UNKNOWN, a sensible default will be picked if possible.
  MARU_ContextAttributes attributes;  ///< Initial runtime attributes.
  const struct MARU_ContextTuning *tuning; ///< Optional low-level tuning.
} MARU_ContextCreateInfo;

/** @brief Default initialization macro for MARU_ContextCreateInfo. */
#define MARU_CONTEXT_CREATE_INFO_DEFAULT        \
  {                                             \
      .backend = MARU_BACKEND_UNKNOWN,          \
      .attributes =                             \
          {                                     \
              .inhibit_idle = false,            \
              .event_mask = (MARU_EventMask)-1, \
          },                                    \
      .tuning = NULL,                           \
  }

/** @brief Creates a new context. 
 *
 *  The creating thread becomes the 'Owner Thread'. Only this thread may 
 *  perform mutating operations (pump, update, etc.). Other threads may 
 *  use passive accessors if they provide external synchronization to 
 *  ensure memory visibility. See maru.h for the full threading model.
 */
MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                          MARU_Context **out_context);

/** @brief Destroys a context and all associated windows. */
MARU_Status maru_destroyContext(MARU_Context *context);

/** @brief Updates one or more context attributes. */
MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes);

/** @brief Resets the metrics counters attached to a context handle. */
MARU_Status maru_resetContextMetrics(MARU_Context *context);

#include "maru/c/details/contexts.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_CONTEXT_H_INCLUDED
