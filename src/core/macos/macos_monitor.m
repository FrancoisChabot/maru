// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Monitor *const *maru_getMonitors_Cocoa(MARU_Context *context, uint32_t *out_count) {
  if (out_count) *out_count = 0;
  return NULL;
}

void maru_retainMonitor_Cocoa(MARU_Monitor *monitor) {
}

void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor) {
}

const MARU_VideoMode *maru_getMonitorModes_Cocoa(const MARU_Monitor *monitor, uint32_t *out_count) {
  if (out_count) *out_count = 0;
  return NULL;
}

MARU_Status maru_setMonitorMode_Cocoa(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  return MARU_FAILURE;
}

void maru_resetMonitorMetrics_Cocoa(MARU_Monitor *monitor) {
}
