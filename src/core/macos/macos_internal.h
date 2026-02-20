// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_COCOA_INTERNAL_H_INCLUDED
#define MARU_COCOA_INTERNAL_H_INCLUDED

#include "maru_internal.h"

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>

typedef struct MARU_Context_Cocoa {
  MARU_Context_Base base;

  id ns_app;
  id ns_run_loop;
  bool event_loop_started;
} MARU_Context_Cocoa;

typedef struct MARU_Window_Cocoa {
  MARU_Window_Base base;

  id ns_window;
  id ns_view;

  MARU_Vec2Dip size;
  bool waiting_for_configure;
} MARU_Window_Cocoa;

MARU_Status maru_createContext_Cocoa(const MARU_ContextCreateInfo *create_info,
                                      MARU_Context **out_context);
MARU_Status maru_destroyContext_Cocoa(MARU_Context *context);
MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_createWindow_Cocoa(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window);
MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window);
MARU_Status maru_getWindowGeometry_Cocoa(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);

#endif
