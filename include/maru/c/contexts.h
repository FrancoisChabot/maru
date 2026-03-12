// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONTEXT_H_INCLUDED
#define MARU_CONTEXT_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/inputs.h"
#include "maru/c/metrics.h"
#include "maru/c/tuning.h"

/**
 * @file contexts.h
 * @brief Context management, lifecycle attributes, and core state.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server
 * connection. */
typedef struct MARU_Context MARU_Context;

/** @brief Runtime state flags for a context. */
#define MARU_CONTEXT_STATE_LOST MARU_BIT(0)
#define MARU_CONTEXT_STATE_READY MARU_BIT(1)

/** @brief Forward declaration of diagnostic info. */
struct MARU_DiagnosticInfo;

/** @brief Callback function for library diagnostics. */
typedef void (*MARU_DiagnosticCallback)(const struct MARU_DiagnosticInfo *info,
                                        void *userdata);

/* ----- Direct-Return State Access (External Synchronization Required) -----
 *
 * These functions perform direct reads/writes of cached handle state. They can
 * be called from any thread, provided access is synchronized with owner-thread
 * operations (like maru_pumpEvents or maru_updateContext) on the same context
 * to ensure memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a context. */
static inline void *maru_getContextUserdata(const MARU_Context *context);

/** @brief Sets the user-defined data pointer associated with a context. */
static inline void maru_setContextUserdata(MARU_Context *context,
                                           void *userdata);

/** @brief Checks if the context has been lost due to an unrecoverable error. */
static inline bool maru_isContextLost(const MARU_Context *context);

/** @brief Checks if the context is fully initialized and ready for use. */
static inline bool maru_isContextReady(const MARU_Context *context);

/** @brief Retrieves the backend type of a context. */
static inline MARU_BackendType maru_getContextBackend(const MARU_Context *context);

/** @brief Retrieves the runtime performance metrics for a context. */
static inline const MARU_ContextMetrics *
maru_getContextMetrics(const MARU_Context *context);

/** @brief Retrieves the number of mouse button channels supported by this
 * context. */
static inline uint32_t
maru_getContextMouseButtonCount(const MARU_Context *context);

/** @brief Retrieves the current context-level mouse button states table. */
static inline const MARU_ButtonState8 *
maru_getContextMouseButtonStates(const MARU_Context *context);

/** @brief Retrieves metadata for context-level mouse button channels. */
static inline const MARU_ChannelInfo *
maru_getContextMouseButtonChannelInfo(const MARU_Context *context);

/** @brief Retrieves the channel index for a default mouse role on this context.
 */
static inline int32_t
maru_getContextMouseDefaultButtonChannel(const MARU_Context *context,
                                         MARU_MouseDefaultButton which);

/** @brief Checks whether a context-level mouse button channel is currently
 * pressed. */
static inline bool maru_isContextMouseButtonPressed(const MARU_Context *context,
                                                    uint32_t button_id);

/* ----- Context Management ----- */

/** @brief Special value for timeouts to indicate it should never trigger. */
#define MARU_NEVER UINT32_MAX

#define MARU_CONTEXT_ATTR_INHIBIT_IDLE MARU_BIT(0)
#define MARU_CONTEXT_ATTR_DIAGNOSTICS MARU_BIT(1)
#define MARU_CONTEXT_ATTR_DEFAULT_CURSOR MARU_BIT(2)
#define MARU_CONTEXT_ATTR_IDLE_TIMEOUT MARU_BIT(3)

#define MARU_CONTEXT_ATTR_ALL                                                  \
  (MARU_CONTEXT_ATTR_INHIBIT_IDLE | MARU_CONTEXT_ATTR_DIAGNOSTICS |    \
   MARU_CONTEXT_ATTR_DEFAULT_CURSOR | MARU_CONTEXT_ATTR_IDLE_TIMEOUT)

/** @brief Updatable parameters for an active context. */
typedef struct MARU_ContextAttributes {
  MARU_DiagnosticCallback
      diagnostic_cb;         ///< Optional callback for library diagnostics.
  void *diagnostic_userdata; ///< Passed to the diagnostic callback.

  struct MARU_Cursor *default_cursor; ///< Default cursor for all windows.

  bool inhibit_idle; ///< If true, prevents the OS from entering sleep.

  uint32_t idle_timeout_ms; ///< Threshold for MARU_EVENT_IDLE_CHANGED.
                            ///< 0 disables idle notifications.

} MARU_ContextAttributes;

/** @brief Parameters for maru_createContext(). */
typedef struct MARU_ContextCreateInfo {
  MARU_Allocator allocator; ///< Custom memory allocator.
  MARU_BackendType
      backend; ///< Requested backend, if set to MARU_BACKEND_UNKNOWN, a
               ///< sensible default will be picked if possible.
  MARU_ContextAttributes attributes; ///< Initial runtime attributes.
  MARU_ContextTuning tuning;         ///< Low-level tuning.
} MARU_ContextCreateInfo;

/** @brief Default initialization macro for MARU_ContextCreateInfo. */
#define MARU_CONTEXT_CREATE_INFO_DEFAULT                                       \
  {                                                                            \
      .allocator =                                                             \
          {                                                                    \
              .alloc_cb = NULL,                                                \
              .realloc_cb = NULL,                                              \
              .free_cb = NULL,                                                 \
              .userdata = NULL,                                                \
          },                                                                   \
      .backend = MARU_BACKEND_UNKNOWN,                                         \
      .attributes =                                                            \
          {                                                                    \
              .diagnostic_cb = NULL,                                           \
              .diagnostic_userdata = NULL,                                     \
              .default_cursor = NULL,                                          \
              .inhibit_idle = false,                                           \
              .idle_timeout_ms = 0,                                            \
          },                                                                   \
      .tuning = MARU_CONTEXT_TUNING_DEFAULT,                                   \
  }

/** @brief Creates a new context.
 *
 *  The creating thread becomes the 'Owner Thread'. Only this thread may
 *  call owner-thread APIs (pump, update, etc.). Other threads may call
 *  direct-return state access APIs with external synchronization to ensure
 *  memory visibility. See maru.h for the full threading model.
 */
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context);

/** @brief Destroys a context and all associated windows. */
MARU_API MARU_Status maru_destroyContext(MARU_Context *context);

/** @brief Updates one or more context attributes. */
MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes);

/* Most MARU_Status APIs short-circuit with MARU_ERROR_CONTEXT_LOST after a
 * context loss. This is intentional: once a context is lost, further calls are
 * harmless but futile, so callers only need to check for loss around important
 * operations. maru_destroyContext() remains valid so teardown and rebuild stay
 * straightforward.
 */

/** @brief Resets the metrics counters attached to a context handle. */
MARU_API MARU_Status maru_resetContextMetrics(MARU_Context *context);

#include "maru/c/details/contexts.h"

#ifdef __cplusplus
}
#endif

#endif // MARU_CONTEXT_H_INCLUDED
