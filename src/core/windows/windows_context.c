// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#define _CRT_SECURE_NO_WARNINGS
#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static HMODULE _maru_win32_get_user32(void) {
  return GetModuleHandleA("user32.dll");
}

static bool _maru_windows_init_context_mouse_channels(MARU_Context_Windows *ctx) {
  static const struct {
    const char *name;
    uint32_t native_code;
  } channel_defs[] = {
      {"left", VK_LBUTTON}, {"right", VK_RBUTTON}, {"middle", VK_MBUTTON},
      {"x1", 1}, {"x2", 2},
  };
  const uint32_t channel_count = (uint32_t)(sizeof(channel_defs) / sizeof(channel_defs[0]));

  ctx->base.mouse_button_channels =
      (MARU_MouseButtonChannelInfo *)maru_context_alloc(&ctx->base,
                                                        sizeof(MARU_MouseButtonChannelInfo) * channel_count);
  if (!ctx->base.mouse_button_channels) {
    return false;
  }

  ctx->base.mouse_button_states =
      (MARU_ButtonState8 *)maru_context_alloc(&ctx->base, sizeof(MARU_ButtonState8) * channel_count);
  if (!ctx->base.mouse_button_states) {
    maru_context_free(&ctx->base, ctx->base.mouse_button_channels);
    ctx->base.mouse_button_channels = NULL;
    return false;
  }

  memset(ctx->base.mouse_button_states, 0, sizeof(MARU_ButtonState8) * channel_count);
  for (uint32_t i = 0; i < channel_count; ++i) {
    ctx->base.mouse_button_channels[i].name = channel_defs[i].name;
    ctx->base.mouse_button_channels[i].native_code = channel_defs[i].native_code;
    ctx->base.mouse_button_channels[i].is_default = false;
  }

  ctx->base.pub.mouse_button_count = channel_count;
  ctx->base.pub.mouse_button_channels = ctx->base.mouse_button_channels;
  ctx->base.pub.mouse_button_state = ctx->base.mouse_button_states;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_LEFT] = 0;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_RIGHT] = 1;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_MIDDLE] = 2;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_BACK] = 3;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_FORWARD] = 4;

  ctx->base.mouse_button_channels[0].is_default = true;
  ctx->base.mouse_button_channels[1].is_default = true;
  ctx->base.mouse_button_channels[2].is_default = true;
  ctx->base.mouse_button_channels[3].is_default = true;
  ctx->base.mouse_button_channels[4].is_default = true;
  return true;
}

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Windows));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t*)ctx) + sizeof(MARU_Context_Base), 0, sizeof(MARU_Context_Windows) - sizeof(MARU_Context_Base));

  ctx->base.backend_type = MARU_BACKEND_WINDOWS;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }
  ctx->base.tuning = create_info->tuning;
  _maru_init_context_base(&ctx->base);

  ctx->base.pub.userdata = create_info->userdata;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = ctx->base.attrs_effective.diagnostic_cb;
  ctx->base.diagnostic_userdata = ctx->base.attrs_effective.diagnostic_userdata;
  ctx->base.event_mask = ctx->base.attrs_effective.event_mask;
  ctx->base.inhibit_idle = ctx->base.attrs_effective.inhibit_idle;

  ctx->base.tuning.idle_timeout_ms = ctx->base.attrs_effective.idle_timeout_ms;
  if (!_maru_windows_init_context_mouse_channels(ctx)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_Windows;
  ctx->base.backend = &maru_backend_Windows;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  ctx->user32_module = _maru_win32_get_user32();
  if (!ctx->user32_module) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to load user32.dll");
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->owner_thread_id = GetCurrentThreadId();

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  if (ctx->user32_module) {
  }

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  
  MARU_PumpContext pump_ctx = {
    .callback = callback,
    .userdata = userdata
  };
  ctx->base.pump_ctx = &pump_ctx;

  MSG msg;
  if (timeout_ms == 0) {
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  } else {
    DWORD start_time = GetTickCount();
    DWORD elapsed = 0;

    while (elapsed < timeout_ms) {
      if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
      } else {
        MsgWaitForMultipleObjects(0, NULL, FALSE, timeout_ms - elapsed, QS_ALLINPUT);
      }
      elapsed = GetTickCount() - start_time;
    }
  }
  
  ctx->base.pump_ctx = NULL;
  return MARU_SUCCESS;
}

MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    // TODO: Implement idle inhibition
  }

  return MARU_SUCCESS;
}

MARU_Status maru_createCursor_Windows(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  const bool system_cursor = (create_info->source == MARU_CURSOR_SOURCE_SYSTEM);
  HCURSOR hcursor = NULL;
  bool is_system = false;

  if (system_cursor) {
    LPCSTR id = NULL;
    switch (create_info->system_shape) {
      case MARU_CURSOR_SHAPE_DEFAULT: id = IDC_ARROW; break;
      case MARU_CURSOR_SHAPE_HELP: id = IDC_HELP; break;
      case MARU_CURSOR_SHAPE_HAND: id = IDC_HAND; break;
      case MARU_CURSOR_SHAPE_WAIT: id = IDC_WAIT; break;
      case MARU_CURSOR_SHAPE_CROSSHAIR: id = IDC_CROSS; break;
      case MARU_CURSOR_SHAPE_TEXT: id = IDC_IBEAM; break;
      case MARU_CURSOR_SHAPE_MOVE: id = IDC_SIZEALL; break;
      case MARU_CURSOR_SHAPE_NOT_ALLOWED: id = IDC_NO; break;
      case MARU_CURSOR_SHAPE_EW_RESIZE: id = IDC_SIZEWE; break;
      case MARU_CURSOR_SHAPE_NS_RESIZE: id = IDC_SIZENS; break;
      case MARU_CURSOR_SHAPE_NESW_RESIZE: id = IDC_SIZENESW; break;
      case MARU_CURSOR_SHAPE_NWSE_RESIZE: id = IDC_SIZENWSE; break;
      default: return MARU_FAILURE;
    }
    hcursor = LoadCursor(NULL, id);
    if (!hcursor) return MARU_FAILURE;
    is_system = true;
  } else {
    // Win32 cursors are BGRA; create a mask and color bitmap.
    int width = create_info->size.x;
    int height = create_info->size.y;
    if (!create_info->pixels || width <= 0 || height <= 0) {
      return MARU_FAILURE;
    }

    BITMAPV5HEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = width;
    bi.bV5Height = -height; // Top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hcolor = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, hdc);

    if (!hcolor) return MARU_FAILURE;

    memcpy(bits, create_info->pixels, (size_t)width * (size_t)height * 4);

    HBITMAP hmask = CreateBitmap(width, height, 1, 1, NULL);
    if (!hmask) {
      DeleteObject(hcolor);
      return MARU_FAILURE;
    }

    ICONINFO ii;
    memset(&ii, 0, sizeof(ii));
    ii.fIcon = FALSE; // It's a cursor
    ii.xHotspot = (DWORD)create_info->hot_spot.x;
    ii.yHotspot = (DWORD)create_info->hot_spot.y;
    ii.hbmMask = hmask;
    ii.hbmColor = hcolor;

    hcursor = CreateIconIndirect(&ii);

    DeleteObject(hcolor);
    DeleteObject(hmask);

    if (!hcursor) return MARU_FAILURE;
  }

  MARU_Cursor_Windows *cursor = (MARU_Cursor_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_Windows));
  if (!cursor) {
    if (!is_system) {
      DestroyCursor(hcursor);
    }
    return MARU_FAILURE;
  }
  memset(cursor, 0, sizeof(MARU_Cursor_Windows));

  cursor->base.ctx_base = &ctx->base;
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.userdata = create_info->userdata;
  cursor->base.pub.flags = is_system ? MARU_CURSOR_FLAG_SYSTEM : 0;
#ifdef MARU_INDIRECT_BACKEND
  cursor->base.backend = ctx->base.backend;
#endif
  cursor->hcursor = hcursor;
  cursor->is_system = is_system;

  ctx->base.metrics.cursor_create_count_total++;
  if (is_system) ctx->base.metrics.cursor_create_count_system++;
  else ctx->base.metrics.cursor_create_count_custom++;
  ctx->base.metrics.cursor_alive_current++;
  if (ctx->base.metrics.cursor_alive_current > ctx->base.metrics.cursor_alive_peak) {
    ctx->base.metrics.cursor_alive_peak = ctx->base.metrics.cursor_alive_current;
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_Windows(MARU_Cursor *cursor_handle) {
  MARU_Cursor_Windows *cursor = (MARU_Cursor_Windows *)cursor_handle;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)cursor->base.ctx_base;
  if (!cursor->is_system && cursor->hcursor) {
    DestroyCursor(cursor->hcursor);
  }
  ctx->base.metrics.cursor_destroy_count_total++;
  if (ctx->base.metrics.cursor_alive_current > 0) {
    ctx->base.metrics.cursor_alive_current--;
  }
  maru_context_free(cursor->base.ctx_base, cursor);
  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  PostThreadMessageA(ctx->owner_thread_id, WM_NULL, 0, 0);
  return MARU_SUCCESS;
}

static BOOL CALLBACK _maru_win32_monitor_enum_proc(HMONITOR hmonitor, HDC hdc, LPRECT rect, LPARAM dw_data) {
  (void)hdc;
  (void)rect;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)dw_data;

  // Check if we already have this monitor in cache
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; i++) {
    MARU_Monitor_Windows *mon = (MARU_Monitor_Windows *)ctx->base.monitor_cache[i];
    if (mon->hmonitor == hmonitor) {
      mon->base.is_active = true;
      return TRUE;
    }
  }

  // New monitor
  MARU_Monitor_Windows *mon = (MARU_Monitor_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Monitor_Windows));
  if (!mon) return FALSE;
  memset(mon, 0, sizeof(MARU_Monitor_Windows));

  mon->base.ctx_base = &ctx->base;
  mon->base.pub.context = (MARU_Context *)ctx;
  mon->base.pub.metrics = &mon->base.metrics;
  mon->base.is_active = true;
  atomic_init(&mon->base.ref_count, 1u); // 1 for being in the cache
#ifdef MARU_INDIRECT_BACKEND
  mon->base.backend = ctx->base.backend;
#endif
  mon->hmonitor = hmonitor;

  MONITORINFOEXA mi;
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoA(hmonitor, (LPMONITORINFO)&mi)) {
    mon->base.pub.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    mon->base.pub.logical_position = (MARU_Vec2Dip){ (MARU_Scalar)mi.rcMonitor.left, (MARU_Scalar)mi.rcMonitor.top };
    mon->base.pub.logical_size = (MARU_Vec2Dip){ (MARU_Scalar)(mi.rcMonitor.right - mi.rcMonitor.left), (MARU_Scalar)(mi.rcMonitor.bottom - mi.rcMonitor.top) };
    strncpy(mon->device_name, mi.szDevice, sizeof(mon->device_name));
    mon->device_name[sizeof(mon->device_name) - 1] = '\0';
    mon->base.pub.name = mon->device_name;
  }

  // Get current mode
  DEVMODEA dm;
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsA(mon->device_name, ENUM_CURRENT_SETTINGS, &dm)) {
    mon->base.pub.current_mode.size = (MARU_Vec2Px){ (int32_t)dm.dmPelsWidth, (int32_t)dm.dmPelsHeight };
    mon->base.pub.current_mode.refresh_rate_mhz = dm.dmDisplayFrequency * 1000;
  }

  // Get physical size (approximate)
  mon->base.pub.physical_size = (MARU_Vec2Dip){ 
    (MARU_Scalar)mon->base.pub.current_mode.size.x * 25.4f / 96.0f,
    (MARU_Scalar)mon->base.pub.current_mode.size.y * 25.4f / 96.0f 
  };

  // Get Scale
  mon->base.pub.scale = 1.0f;
  UINT dpi_x, dpi_y;
  HMODULE shcore = GetModuleHandleA("shcore.dll");
  if (shcore) {
    typedef HRESULT (WINAPI * GetDpiForMonitorProc)(HMONITOR, int, UINT*, UINT*);
    GetDpiForMonitorProc get_dpi = (GetDpiForMonitorProc)GetProcAddress(shcore, "GetDpiForMonitor");
    if (get_dpi) {
      if (SUCCEEDED(get_dpi(hmonitor, 0, &dpi_x, &dpi_y))) {
        mon->base.pub.scale = (MARU_Scalar)dpi_x / 96.0f;
      }
    }
  }

  // Add to cache
  if (ctx->base.monitor_cache_count >= ctx->base.monitor_cache_capacity) {
    uint32_t old_cap = ctx->base.monitor_cache_capacity;
    uint32_t new_cap = old_cap ? old_cap * 2 : 4;
    ctx->base.monitor_cache = (MARU_Monitor **)maru_context_realloc(&ctx->base, ctx->base.monitor_cache, old_cap * sizeof(MARU_Monitor *), new_cap * sizeof(MARU_Monitor *));
    ctx->base.monitor_cache_capacity = new_cap;
  }
  ctx->base.monitor_cache[ctx->base.monitor_cache_count++] = (MARU_Monitor *)mon;

  return TRUE;
}

MARU_Status maru_updateMonitors_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  // Mark all existing as inactive
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; i++) {
    ((MARU_Monitor_Base *)ctx->base.monitor_cache[i])->is_active = false;
  }

  if (!EnumDisplayMonitors(NULL, NULL, _maru_win32_monitor_enum_proc, (LPARAM)ctx)) {
    return MARU_FAILURE;
  }

  // Remove inactive ones from cache
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ) {
    MARU_Monitor_Base *mon = (MARU_Monitor_Base *)ctx->base.monitor_cache[i];
    if (!mon->is_active) {
      mon->pub.flags |= MARU_MONITOR_STATE_LOST;
      // Remove from cache list
      for (uint32_t j = i; j < ctx->base.monitor_cache_count - 1; j++) {
        ctx->base.monitor_cache[j] = ctx->base.monitor_cache[j+1];
      }
      ctx->base.monitor_cache_count--;
      maru_releaseMonitor((MARU_Monitor *)mon);
    } else {
      i++;
    }
  }

  return MARU_SUCCESS;
}

const MARU_VideoMode *maru_getMonitorModes_Windows(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_Monitor_Windows *mon = (MARU_Monitor_Windows *)monitor;
  
  if (!mon->modes) {
    uint32_t cap = 16;
    mon->modes = (MARU_VideoMode *)maru_context_alloc(mon->base.ctx_base, cap * sizeof(MARU_VideoMode));
    
    DEVMODEA dm;
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsA(mon->device_name, i, &dm); i++) {
      if (mon->mode_count >= cap) {
        uint32_t old_cap = cap;
        cap *= 2;
        mon->modes = (MARU_VideoMode *)maru_context_realloc(mon->base.ctx_base, mon->modes, old_cap * sizeof(MARU_VideoMode), cap * sizeof(MARU_VideoMode));
      }
      
      bool exists = false;
      for (uint32_t j = 0; j < mon->mode_count; j++) {
        if (mon->modes[j].size.x == (int32_t)dm.dmPelsWidth &&
            mon->modes[j].size.y == (int32_t)dm.dmPelsHeight &&
            mon->modes[j].refresh_rate_mhz == dm.dmDisplayFrequency * 1000) {
          exists = true;
          break;
        }
      }
      
      if (!exists) {
        mon->modes[mon->mode_count++] = (MARU_VideoMode){
          .size = { (int32_t)dm.dmPelsWidth, (int32_t)dm.dmPelsHeight },
          .refresh_rate_mhz = dm.dmDisplayFrequency * 1000
        };
      }
    }
  }

  *out_count = mon->mode_count;
  return mon->modes;
}

MARU_Status maru_setMonitorMode_Windows(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_Monitor_Windows *mon = (MARU_Monitor_Windows *)monitor;
  
  DEVMODEA dm;
  memset(&dm, 0, sizeof(dm));
  dm.dmSize = sizeof(dm);
  dm.dmPelsWidth = (DWORD)mode.size.x;
  dm.dmPelsHeight = (DWORD)mode.size.y;
  dm.dmDisplayFrequency = mode.refresh_rate_mhz / 1000;
  dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
  
  if (ChangeDisplaySettingsExA(mon->device_name, &dm, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
    return MARU_FAILURE;
  }
  
  mon->base.pub.current_mode = mode;
  return MARU_SUCCESS;
}

void *_maru_getContextNativeHandle_Windows(MARU_Context *context) {
  (void)context;
  return NULL;
}
