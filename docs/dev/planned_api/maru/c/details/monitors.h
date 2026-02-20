/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_MONITORS_H_INCLUDED
#define MARU_DETAILS_MONITORS_H_INCLUDED

#include "maru/c/monitors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Internal representation of MARU_Monitor. 
    CHANGING THIS REQUIRES A MAJOR VERSION BUMP
*/
typedef struct MARU_MonitorExposed {
  void *userdata;
  MARU_Context *context;
  uint64_t flags;
  const MARU_MonitorMetrics *metrics;

  const char *name;
  MARU_Vec2Dip physical_size;
  MARU_VideoMode current_mode;
  MARU_Vec2Dip logical_position;
  MARU_Vec2Dip logical_size;
  bool is_primary;
  MARU_Scalar scale;
} MARU_MonitorExposed;

static inline void *maru_getMonitorUserdata(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->userdata;
}

static inline void maru_setMonitorUserdata(MARU_Monitor *monitor, void *userdata) {
  ((MARU_MonitorExposed *)monitor)->userdata = userdata;
}

static inline MARU_Context *maru_getMonitorContext(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->context;
}

static inline bool maru_isMonitorLost(const MARU_Monitor *monitor) {
  return (((const MARU_MonitorExposed *)monitor)->flags & MARU_MONITOR_STATE_LOST) != 0;
}

static inline const MARU_MonitorMetrics *maru_getMonitorMetrics(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->metrics;
}

static inline const char *maru_getMonitorName(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->name;
}

static inline MARU_Vec2Dip maru_getMonitorPhysicalSize(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->physical_size;
}

static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->current_mode;
}

static inline MARU_Vec2Dip maru_getMonitorLogicalPosition(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->logical_position;
}

static inline MARU_Vec2Dip maru_getMonitorLogicalSize(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->logical_size;
}

static inline bool maru_isMonitorPrimary(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->is_primary;
}

static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor *monitor) {
  return ((const MARU_MonitorExposed *)monitor)->scale;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_MONITORS_H_INCLUDED
