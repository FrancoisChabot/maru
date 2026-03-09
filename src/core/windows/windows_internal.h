// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WINDOWS_INTERNAL_H_INCLUDED
#define MARU_WINDOWS_INTERNAL_H_INCLUDED

#include "maru_internal.h"

#include <windows.h>

typedef struct MARU_Context_Windows {
  MARU_Context_Base base;

  HINSTANCE instance;
  ATOM window_class;
  HANDLE wake_event;

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

  HICON icon_small;
  HICON icon_big;

  bool show_on_first_pump;
  MARU_Vec2Dip last_mouse_pos;
  bool last_mouse_pos_valid;

  wchar_t high_surrogate;

  MARU_CursorMode cursor_mode;
  bool is_cursor_in_client_area;
  MARU_Vec2Dip virtual_cursor_pos;
} MARU_Window_Windows;

// contexts.h
MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context);
MARU_Status maru_destroyContext_Windows(MARU_Context *context);
MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes);

// events.h
MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_wakeContext_Windows(MARU_Context *context);

// windows.h
MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window);
MARU_Status maru_destroyWindow_Windows(MARU_Window *window);
void maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);
MARU_Status maru_updateWindow_Windows(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes);
MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window);
MARU_Status maru_requestWindowFrame_Windows(MARU_Window *window);
MARU_Status maru_requestWindowAttention_Windows(MARU_Window *window);

// images.h
MARU_Status maru_createImage_Windows(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image);
MARU_Status maru_destroyImage_Windows(MARU_Image *image);

// cursors.h
MARU_Status maru_createCursor_Windows(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_Windows(MARU_Cursor *cursor);
MARU_Status maru_resetCursorMetrics_Windows(MARU_Cursor *cursor);

// monitors.h
MARU_Status maru_updateMonitors_Windows(MARU_Context *context);
const MARU_VideoMode *maru_getMonitorModes_Windows(const MARU_Monitor *monitor,
                                         uint32_t *out_count);
MARU_Status maru_setMonitorMode_Windows(const MARU_Monitor *monitor,
                                        MARU_VideoMode mode);
void maru_resetMonitorMetrics_Windows(MARU_Monitor *monitor);

// controllers.h
MARU_Status maru_getControllers_Windows(MARU_Context *context, MARU_ControllerList *out_list);
MARU_Status maru_retainController_Windows(MARU_Controller *controller);
MARU_Status maru_releaseController_Windows(MARU_Controller *controller);
MARU_Status maru_resetControllerMetrics_Windows(MARU_Controller *controller);
MARU_Status maru_getControllerInfo_Windows(MARU_Controller *controller, MARU_ControllerInfo *out_info);
MARU_Status maru_setControllerHapticLevels_Windows(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities);

// data_exchange.h
MARU_Status maru_announceData_Windows(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions);
MARU_Status maru_provideData_Windows(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags);
MARU_Status maru_requestData_Windows(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag);
MARU_Status maru_getAvailableMIMETypes_Windows(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list);

// vulkan.h
const char **maru_getVkExtensions_Windows(const MARU_Context *context, uint32_t *out_count);
MARU_Status maru_createVkSurface_Windows(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

// Native handles
void *_maru_getContextNativeHandle_Windows(MARU_Context *context);
void *_maru_getWindowNativeHandle_Windows(MARU_Window *window);

MARU_Key _maru_translate_key_windows(WPARAM wParam, LPARAM lParam);
MARU_ModifierFlags _maru_get_modifiers_windows(void);

LRESULT CALLBACK _maru_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam);

#endif
