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
} MARU_Context_Windows;

typedef struct MARU_Window_Windows {
  MARU_Window_Base base;

  HWND hwnd;
  HDC hdc;

  MARU_Vec2Dip size;
} MARU_Window_Windows;

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context);
MARU_Status maru_destroyContext_Windows(MARU_Context *context);
MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms);
MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window);
MARU_Status maru_destroyWindow_Windows(MARU_Window *window);
MARU_Status maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);

#endif
