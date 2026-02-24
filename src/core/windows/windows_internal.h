// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WINDOWS_INTERNAL_H_INCLUDED
#define MARU_WINDOWS_INTERNAL_H_INCLUDED

#include "maru_internal.h"

#include <windows.h>

typedef struct MARU_Context_Windows {
  MARU_Context_Base base;

  HMODULE user32_module;
  bool event_loop_started;
  bool cursor_animation_fallback_reported;
  DWORD owner_thread_id;
} MARU_Context_Windows;

typedef struct MARU_Cursor_Windows {
  MARU_Cursor_Base base;
  HCURSOR hcursor;
  bool is_system;
} MARU_Cursor_Windows;

typedef struct MARU_Monitor_Windows {
  MARU_Monitor_Base base;
  HMONITOR hmonitor;
  char device_name[32]; // From MONITORINFOEX

  MARU_VideoMode *modes;
  uint32_t mode_count;
} MARU_Monitor_Windows;

typedef struct MARU_Window_Windows {
  MARU_Window_Base base;

  HWND hwnd;
  HDC hdc;

  MARU_Vec2Dip size;

  bool is_fullscreen;
  DWORD saved_style;
  DWORD saved_ex_style;
  RECT saved_rect;

  MARU_Cursor_Windows *current_cursor;
  MARU_CursorMode cursor_mode;

  MARU_Vec2Dip min_size;
  MARU_Vec2Dip max_size;
  MARU_Fraction aspect_ratio;
} MARU_Window_Windows;

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context);
MARU_Status maru_destroyContext_Windows(MARU_Context *context);
MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes);

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window);
MARU_Status maru_destroyWindow_Windows(MARU_Window *window);
MARU_Status maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);
MARU_Status maru_updateWindow_Windows(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes);
MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window);
MARU_Status maru_createCursor_Windows(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_Windows(MARU_Cursor *cursor);
MARU_Status maru_createImage_Windows(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image);
MARU_Status maru_destroyImage_Windows(MARU_Image *image);
MARU_Status maru_wakeContext_Windows(MARU_Context *context);

MARU_Status maru_updateMonitors_Windows(MARU_Context *context);
const MARU_VideoMode *maru_getMonitorModes_Windows(const MARU_Monitor *monitor,
                                         uint32_t *out_count);
MARU_Status maru_setMonitorMode_Windows(const MARU_Monitor *monitor,
                                        MARU_VideoMode mode);

#endif
