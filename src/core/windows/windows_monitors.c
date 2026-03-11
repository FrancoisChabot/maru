// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <stdatomic.h>
#include <string.h>
#include <wchar.h>

static BOOL CALLBACK _maru_windows_monitor_enum_proc(HMONITOR hmonitor, HDC hdc,
                                                   LPRECT rect, LPARAM data) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)data;

  MONITORINFOEXW info;
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(hmonitor, (LPMONITORINFO)&info)) {
    return TRUE;
  }

  MARU_Monitor_Windows *monitor = NULL;
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    MARU_Monitor_Windows *candidate = (MARU_Monitor_Windows *)ctx->base.monitor_cache[i];
    if (candidate->hmonitor == hmonitor) {
      monitor = candidate;
      break;
    }
  }

  if (!monitor) {
    monitor = (MARU_Monitor_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Monitor_Windows));
    if (!monitor) {
      return TRUE;
    }
    memset(monitor, 0, sizeof(*monitor));
    monitor->base.ctx_base = &ctx->base;
    monitor->base.pub.context = (MARU_Context *)ctx;
    monitor->base.pub.metrics = &monitor->base.metrics;
    monitor->base.pub.scale = (MARU_Scalar)1.0;
    atomic_init(&monitor->base.ref_count, 1u);
#ifdef MARU_INDIRECT_BACKEND
    monitor->base.backend = ctx->base.backend;
#endif

    monitor->hmonitor = hmonitor;
    wcsncpy(monitor->device_name, info.szDevice, 32);

    if (ctx->base.monitor_cache_count >= ctx->base.monitor_cache_capacity) {
      uint32_t old_capacity = ctx->base.monitor_cache_capacity;
      uint32_t new_capacity = old_capacity == 0 ? 4 : old_capacity * 2;
      MARU_Monitor **new_cache = (MARU_Monitor **)maru_context_realloc(
          &ctx->base, ctx->base.monitor_cache, sizeof(MARU_Monitor *) * old_capacity, sizeof(MARU_Monitor *) * new_capacity);
      if (!new_cache) {
        maru_context_free(&ctx->base, monitor);
        return TRUE;
      }
      ctx->base.monitor_cache = new_cache;
      ctx->base.monitor_cache_capacity = new_capacity;
    }
    ctx->base.monitor_cache[ctx->base.monitor_cache_count++] = (MARU_Monitor *)monitor;
  }

  monitor->base.is_active = true;

  // Update metrics
  int32_t width = info.rcMonitor.right - info.rcMonitor.left;
  int32_t height = info.rcMonitor.bottom - info.rcMonitor.top;

  UINT dpi_x = 96, dpi_y = 96;
  if (ctx->GetDpiForMonitor) {
    ctx->GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
  }
  MARU_Scalar scale = (MARU_Scalar)dpi_x / 96.0;

  monitor->base.pub.logical_position.x = (MARU_Scalar)info.rcMonitor.left / scale;
  monitor->base.pub.logical_position.y = (MARU_Scalar)info.rcMonitor.top / scale;
  monitor->base.pub.logical_size.x = (MARU_Scalar)width / scale;
  monitor->base.pub.logical_size.y = (MARU_Scalar)height / scale;
  monitor->base.pub.scale = scale;

  // Windows doesn't easily give physical size in mm via GetMonitorInfo.
  // We could use GetDeviceCaps with the monitor's DC, but it's often unreliable.
  // For now, we stub it or use a heuristic.
  monitor->base.pub.physical_size.x = (MARU_Scalar)0.0;
  monitor->base.pub.physical_size.y = (MARU_Scalar)0.0;

  // Current mode
  DEVMODEW devmode;
  devmode.dmSize = sizeof(devmode);
  if (EnumDisplaySettingsW(monitor->device_name, ENUM_CURRENT_SETTINGS, &devmode)) {
      monitor->base.pub.current_mode.size.x = devmode.dmPelsWidth;
      monitor->base.pub.current_mode.size.y = devmode.dmPelsHeight;
      monitor->base.pub.current_mode.refresh_rate_mhz = devmode.dmDisplayFrequency * 1000;
  }

  return TRUE;
}

MARU_Status maru_updateMonitors_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    ((MARU_Monitor_Base *)ctx->base.monitor_cache[i])->is_active = false;
  }

  if (!EnumDisplayMonitors(NULL, NULL, _maru_windows_monitor_enum_proc, (LPARAM)ctx)) {
    return MARU_FAILURE;
  }

  // Cleanup inactive monitors that have no external references
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ) {
    MARU_Monitor_Base *monitor = (MARU_Monitor_Base *)ctx->base.monitor_cache[i];
    if (!monitor->is_active) {
       uint32_t expected = 1;
       if (atomic_compare_exchange_strong(&monitor->ref_count, &expected, 0)) {
         _maru_monitor_free(monitor);
         for (uint32_t j = i; j + 1 < ctx->base.monitor_cache_count; ++j) {
           ctx->base.monitor_cache[j] = ctx->base.monitor_cache[j + 1];
         }
         ctx->base.monitor_cache_count--;
         continue;
       }
    }
    i++;
  }

  return MARU_SUCCESS;
}

const MARU_VideoMode *maru_getMonitorModes_Windows(const MARU_Monitor *monitor,
                                         uint32_t *out_count) {
  MARU_Monitor_Windows *win_mon = (MARU_Monitor_Windows *)monitor;

  if (win_mon->modes) {
    *out_count = win_mon->mode_count;
    return win_mon->modes;
  }

  // Enumerate modes
  DEVMODEW devmode;
  devmode.dmSize = sizeof(devmode);
  
  uint32_t capacity = 16;
  win_mon->modes = (MARU_VideoMode *)maru_context_alloc(win_mon->base.ctx_base, sizeof(MARU_VideoMode) * capacity);
  if (!win_mon->modes) {
      *out_count = 0;
      return NULL;
  }

  uint32_t mode_index = 0;
  while (EnumDisplaySettingsW(win_mon->device_name, mode_index, &devmode)) {
    if (win_mon->mode_count >= capacity) {
        uint32_t old_capacity = capacity;
        capacity *= 2;
        MARU_VideoMode *new_modes = (MARU_VideoMode *)maru_context_realloc(win_mon->base.ctx_base, win_mon->modes, sizeof(MARU_VideoMode) * old_capacity, sizeof(MARU_VideoMode) * capacity);
        if (!new_modes) break;
        win_mon->modes = new_modes;
    }

    MARU_VideoMode *m = &win_mon->modes[win_mon->mode_count++];
    m->size.x = devmode.dmPelsWidth;
    m->size.y = devmode.dmPelsHeight;
    m->refresh_rate_mhz = devmode.dmDisplayFrequency * 1000;
    
    mode_index++;
  }

  *out_count = win_mon->mode_count;
  return win_mon->modes;
}

MARU_Status maru_setMonitorMode_Windows(const MARU_Monitor *monitor,
                                        MARU_VideoMode mode) {
  MARU_Monitor_Windows *win_mon = (MARU_Monitor_Windows *)monitor;
  
  DEVMODEW devmode;
  memset(&devmode, 0, sizeof(devmode));
  devmode.dmSize = sizeof(devmode);
  devmode.dmPelsWidth = mode.size.x;
  devmode.dmPelsHeight = mode.size.y;
  devmode.dmDisplayFrequency = mode.refresh_rate_mhz / 1000;
  devmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

  LONG result = ChangeDisplaySettingsExW(win_mon->device_name, &devmode, NULL, CDS_FULLSCREEN, NULL);
  if (result != DISP_CHANGE_SUCCESSFUL) {
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

MARU_Status maru_resetMonitorMetrics_Windows(MARU_Monitor *monitor) {
  MARU_Monitor_Windows *win_mon = (MARU_Monitor_Windows *)monitor;
  (void)ChangeDisplaySettingsExW(win_mon->device_name, NULL, NULL, 0, NULL);
  return MARU_SUCCESS;
}

void _maru_monitor_free(MARU_Monitor_Base *monitor) {
  MARU_Monitor_Windows *win_mon = (MARU_Monitor_Windows *)monitor;
  if (win_mon->modes) {
    maru_context_free(monitor->ctx_base, win_mon->modes);
  }
  maru_context_free(monitor->ctx_base, win_mon);
}
