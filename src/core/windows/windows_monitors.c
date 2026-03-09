// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_updateMonitors_Windows(MARU_Context *context) {
  (void)context;
  return MARU_FAILURE;
}

const MARU_VideoMode *maru_getMonitorModes_Windows(const MARU_Monitor *monitor,
                                         uint32_t *out_count) {
  (void)monitor;
  if (out_count) {
    *out_count = 0;
  }
  return NULL;
}

MARU_Status maru_setMonitorMode_Windows(const MARU_Monitor *monitor,
                                        MARU_VideoMode mode) {
  (void)monitor;
  (void)mode;
  return MARU_FAILURE;
}

void maru_resetMonitorMetrics_Windows(MARU_Monitor *monitor) {
  (void)monitor;
}

void _maru_monitor_free(MARU_Monitor_Base *monitor) {
  (void)monitor;
}
