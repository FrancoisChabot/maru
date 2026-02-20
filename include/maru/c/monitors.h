// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_MONITOR_H_INCLUDED
#define MARU_MONITOR_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/metrics.h"

/**
 * @file monitors.h
 * @brief Physical monitor discovery and video mode APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;

/** @brief Opaque handle representing a physical monitor. 
 *
 *  Handles obtained via maru_getMonitors() or within events are TRANSIENT 
 *  and only valid until the next call to maru_pumpEvents().
 * 
 *  If you need to hold on to a monitor handle longer, use maru_retainMonitor().
 *  Every call to maru_retainMonitor() must be matched by a call to maru_releaseMonitor().
 */
typedef struct MARU_Monitor MARU_Monitor;

/** @brief A video mode supported by a monitor. */
typedef struct MARU_VideoMode {
  MARU_Vec2Px size;
  uint32_t refresh_rate_mhz;
} MARU_VideoMode;

/* ----- Passive Accessors (External Synchronization Required) ----- 
 *
 * These functions are essentially zero-cost member accesses. They are safe to 
 * call from any thread, provided the access is synchronized with mutating 
 * operations (like maru_pumpEvents) on the same monitor to ensure memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a monitor. */
static inline void *maru_getMonitorUserdata(const MARU_Monitor *monitor);

/** @brief Sets the user-defined data pointer associated with a monitor. */
static inline void maru_setMonitorUserdata(MARU_Monitor *monitor, void *userdata);

/** @brief Retrieves the context that owns the specified monitor. */
static inline MARU_Context *maru_getMonitorContext(const MARU_Monitor *monitor);

/** @brief Checks if the monitor has been lost due to disconnection. */
static inline bool maru_isMonitorLost(const MARU_Monitor *monitor);

/** @brief Retrieves the runtime performance metrics for a monitor. */
static inline const MARU_MonitorMetrics *maru_getMonitorMetrics(const MARU_Monitor *monitor);

/** @brief Retrieves the OS-provided name of the monitor. */
static inline const char *maru_getMonitorName(const MARU_Monitor *monitor);

/** @brief Retrieves the physical dimensions of the monitor in millimeters. */
static inline MARU_Vec2Dip maru_getMonitorPhysicalSize(const MARU_Monitor *monitor);

/** @brief Retrieves the current video mode of the monitor. */
static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor *monitor);

/** @brief Retrieves the logical position of the monitor in the virtual desktop. */
static inline MARU_Vec2Dip maru_getMonitorLogicalPosition(const MARU_Monitor *monitor);

/** @brief Retrieves the logical dimensions of the monitor. */
static inline MARU_Vec2Dip maru_getMonitorLogicalSize(const MARU_Monitor *monitor);

/** @brief Checks if this is the primary system monitor. */
static inline bool maru_isMonitorPrimary(const MARU_Monitor *monitor);

/** @brief Retrieves the scaling factor for the monitor. */
static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor *monitor);

/** @brief Retrieves the list of currently connected monitors. 

The returned list is valid until the next call to maru_pumpEvents().
*/
MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count);

/** @brief Increments the reference count of a monitor handle. 
 *
 * This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 * without external synchronization.
 */
void maru_retainMonitor(MARU_Monitor *monitor);

/** @brief Decrements the reference count of a monitor handle. 
 *
 * This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 * without external synchronization.
 */
void maru_releaseMonitor(MARU_Monitor *monitor);

/** @brief Retrieves all video modes supported by a specific monitor. 

The returned list is valid until the next call to maru_pumpEvents().
*/
const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count);

/** @brief Sets the monitor to the provided mode */
MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode);

/** @brief Resets the metrics counters attached to a monitor handle. */
void maru_resetMonitorMetrics(MARU_Monitor *monitor);

#include "maru/c/details/monitors.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_MONITOR_H_INCLUDED
