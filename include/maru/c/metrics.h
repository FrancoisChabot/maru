// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_METRICS_H_INCLUDED
#define MARU_METRICS_H_INCLUDED

#include <stdint.h>

#include "maru/c/core.h"

/**
 * @file metrics.h
 * @brief Shared metrics structures that are exposed through every handle.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Metrics around the internal user event queue. */
typedef struct MARU_UserEventMetrics {
  uint32_t peak_count;
  uint32_t current_capacity;
  uint32_t drop_count;
} MARU_UserEventMetrics;

/**
 * @brief Context-scoped metrics that live for the lifetime of a context handle.
 *
 * The `version` field exists so new fields can be added in a backwards-compatible
 * way; future releases must only append data after this struct.
 */
typedef struct MARU_ContextMetrics {
  const MARU_UserEventMetrics *user_events;
  uint64_t cursor_create_count_total;
  uint64_t cursor_destroy_count_total;
  uint64_t cursor_create_count_system;
  uint64_t cursor_create_count_custom;
  uint64_t cursor_alive_current;
  uint64_t cursor_alive_peak;
  uint64_t pump_call_count_total;
  uint64_t pump_duration_avg_ns;
  uint64_t pump_duration_peak_ns;
} MARU_ContextMetrics;

/**
 * @brief Window-scoped metrics placeholder.
 *
 * Additional window-specific statistics can be added later as tail fields.
 */
typedef struct MARU_WindowMetrics {
  void* reserved[4];
} MARU_WindowMetrics;

/**
 * @brief Monitor-scoped metrics placeholder.
 */
typedef struct MARU_MonitorMetrics {
  void* reserved[4];
} MARU_MonitorMetrics;

/**
 * @brief Cursor-scoped metrics placeholder.
 */
typedef struct MARU_CursorMetrics {
  void* reserved[4];
} MARU_CursorMetrics;

/**
 * @brief Controller-scoped metrics placeholder.
 */
typedef struct MARU_ControllerMetrics {
  void* reserved[4];
} MARU_ControllerMetrics;

#ifdef __cplusplus
}
#endif

#endif  // MARU_METRICS_H_INCLUDED
