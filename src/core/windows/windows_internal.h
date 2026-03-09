// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WINDOWS_INTERNAL_H_INCLUDED
#define MARU_WINDOWS_INTERNAL_H_INCLUDED

#include "maru_internal.h"

#include <windows.h>
#include <hidusage.h>
#include <hidpi.h>

typedef struct MARU_Context_Windows {
  MARU_Context_Base base;

  HINSTANCE instance;
  ATOM window_class;
  HANDLE wake_event;

  HMODULE user32_module;
  HMODULE shcore_module;
  bool event_loop_started;
  bool cursor_animation_fallback_reported;
  DWORD owner_thread_id;

  struct MARU_Controller_Windows *controller_list_head;
  uint32_t controller_count;
  
  MARU_Controller **controller_list_storage;
  uint32_t controller_list_capacity;

  HMODULE xinput_module;
  HMODULE runtime_object_module;

  // Dynamic function pointers
  DWORD (WINAPI *XInputGetState)(DWORD, void*); // Using void* for XINPUT_STATE to avoid including it here
  DWORD (WINAPI *XInputSetState)(DWORD, void*); // Using void* for XINPUT_VIBRATION
  
  HRESULT (WINAPI *RoInitialize)(int);
  void (WINAPI *RoUninitialize)(void);
  HRESULT (WINAPI *RoGetActivationFactory)(void*, REFIID, void**);
  HRESULT (WINAPI *WindowsCreateString)(LPCWSTR, UINT32, void**);
  HRESULT (WINAPI *WindowsDeleteString)(void*);

  // DPI Awareness
  BOOL (WINAPI *SetProcessDpiAwarenessContext)(HANDLE);
  UINT (WINAPI *GetDpiForWindow)(HWND);
  HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR, int, UINT*, UINT*);
  BOOL (WINAPI *EnableNonClientDpiScaling)(HWND);
} MARU_Context_Windows;

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

#ifndef MDT_EFFECTIVE_DPI
#define MDT_EFFECTIVE_DPI 0
#endif

typedef enum MARU_WindowsHidControllerKind {
  MARU_WINDOWS_HID_KIND_GENERIC = 0,
  MARU_WINDOWS_HID_KIND_DUALSHOCK4 = 1,
  MARU_WINDOWS_HID_KIND_DUALSENSE = 2,
} MARU_WindowsHidControllerKind;

typedef struct MARU_Controller_Windows {
  MARU_Controller_Base base;
  struct MARU_Controller_Windows *next;

  uint32_t xinput_index; // 0-3 for XInput, 0xFFFFFFFF for others
  void *wgi_gamepad; // IGamepad* (WinRT)
  HANDLE raw_input_device;
  HANDLE hid_handle;
  wchar_t *raw_device_path;
  PHIDP_PREPARSED_DATA hid_preparsed_data;
  MARU_WindowsHidControllerKind hid_kind;

  char *name;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  uint8_t guid[16];
  bool is_standardized;

  MARU_ButtonState8 *button_states;
  MARU_ButtonChannelInfo *button_channels;
  uint32_t button_count;

  MARU_AnalogInputState *analog_states;
  MARU_AnalogChannelInfo *analog_channels;
  uint32_t analog_count;

  MARU_HapticChannelInfo *haptic_channels;
  uint32_t haptic_count;
  MARU_Scalar last_haptic_levels[MARU_CONTROLLER_HAPTIC_STANDARD_COUNT];
  bool haptics_dirty;

  struct MARU_WindowsHidButtonMapping *hid_button_mappings;
  uint32_t hid_button_mapping_count;
  struct MARU_WindowsHidValueMapping *hid_value_mappings;
  uint32_t hid_value_mapping_count;
  USAGE_AND_PAGE *hid_pressed_usages;
  ULONG hid_pressed_usage_capacity;
  bool hid_has_hat_switch;
  USAGE hid_hat_usage;
  LONG hid_hat_logical_min;
  LONG hid_hat_logical_max;
  uint32_t hid_hat_button_base;
} MARU_Controller_Windows;

typedef struct MARU_Cursor_Windows {
  MARU_Cursor_Base base;
  HCURSOR hcursor; // Active frame handle
  HCURSOR *hcursors; // All frame handles (for animation)
  uint32_t *anim_frame_delays_ms;
  uint32_t anim_frame_count;
  bool is_system;
} MARU_Cursor_Windows;

typedef struct MARU_Image_Windows {
  MARU_Image_Base base;
} MARU_Image_Windows;

typedef struct MARU_Monitor_Windows {
  MARU_Monitor_Base base;
  HMONITOR hmonitor;
  WCHAR device_name[32]; // From MONITORINFOEX

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

  bool pending_frame_request;
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
HICON _maru_windows_create_hicon_from_image(const MARU_Image *image, bool is_icon, int hot_x, int hot_y);

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

void _maru_windows_cleanup_controllers(MARU_Context_Windows *ctx);
void _maru_windows_update_controllers(MARU_Context_Windows *ctx);
void _maru_windows_resync_controllers(MARU_Context_Windows *ctx);
void _maru_windows_process_raw_input(MARU_Context_Windows *ctx,
                                     const RAWINPUT *raw);

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

uint64_t _maru_windows_get_time_ms(void);
uint64_t _maru_windows_get_time_ns(void);

#endif
