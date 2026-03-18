// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_COCOA_INTERNAL_H_INCLUDED
#define MARU_COCOA_INTERNAL_H_INCLUDED

#ifndef MARU_ENABLE_BACKEND_COCOA
#define MARU_ENABLE_BACKEND_COCOA
#endif

#include "maru_internal.h"
#include "maru/maru.h"

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include "core_event_queue.h"
#include <pthread.h>
#import <Cocoa/Cocoa.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreHaptics/CoreHaptics.h>

static inline NSPasteboardType _maru_mime_to_ns_type(const char *mime_type) {
    if (strcmp(mime_type, "text/plain") == 0) return NSPasteboardTypeString;
    if (strcmp(mime_type, "text/html") == 0) return NSPasteboardTypeHTML;
    if (strcmp(mime_type, "image/png") == 0) return NSPasteboardTypePNG;
    if (strcmp(mime_type, "image/tiff") == 0) return NSPasteboardTypeTIFF;
    return [NSString stringWithUTF8String:mime_type];
}

static inline const char *_maru_ns_type_to_mime(NSPasteboardType type) {
    if ([type isEqualToString:NSPasteboardTypeString]) return "text/plain";
    if ([type isEqualToString:NSPasteboardTypeHTML]) return "text/html";
    if ([type isEqualToString:NSPasteboardTypePNG]) return "image/png";
    if ([type isEqualToString:NSPasteboardTypeTIFF]) return "image/tiff";
    return [type UTF8String];
}

static inline MARU_ModifierFlags _maru_cocoa_translate_modifiers(NSEventModifierFlags flags) {
    MARU_ModifierFlags mods = 0;
    if (flags & NSEventModifierFlagShift) mods |= MARU_MODIFIER_SHIFT;
    if (flags & NSEventModifierFlagControl) mods |= MARU_MODIFIER_CONTROL;
    if (flags & NSEventModifierFlagOption) mods |= MARU_MODIFIER_ALT;
    if (flags & NSEventModifierFlagCommand) mods |= MARU_MODIFIER_META;
    if (flags & NSEventModifierFlagCapsLock) mods |= MARU_MODIFIER_CAPS_LOCK;
    if (flags & NSEventModifierFlagNumericPad) mods |= MARU_MODIFIER_NUM_LOCK;
    return mods;
}

typedef struct MARU_Context_Cocoa {
  MARU_Context_Base base;

  id ns_app;
  uint64_t last_modifiers;
  id controller_observer;
  
  MARU_Controller_Base **controller_cache;
  uint32_t controller_cache_count;
  uint32_t controller_cache_capacity;
  uint32_t controller_snapshot_count;
  _Atomic bool controllers_dirty;
  bool controller_snapshot_dirty;

  id clipboard_delegate;
  char **clipboard_announced_mime_types;
  uint32_t clipboard_announced_mime_type_count;
  
  char **clipboard_available_mime_types;
  uint32_t clipboard_available_mime_type_count;

  IOPMAssertionID idle_assertion_id;

  uint64_t last_input_time_ms;
  bool is_idle;
} MARU_Context_Cocoa;

typedef struct MARU_DataRequest_Cocoa {
  MARU_DataRequestHandleBase base;
  id ns_pasteboard;
  id ns_type;
  id ns_delegate;
} MARU_DataRequest_Cocoa;

typedef struct MARU_Window_Cocoa {
  MARU_Window_Base base;

  id ns_window;
  id ns_view;
  id ns_layer;
  MARU_Vec2Dip dip_size;

  uint64_t text_input_session_id;
  bool ime_preedit_active;
  bool accept_drop;
  MARU_DropAction current_drop_action;
  void *drop_session_userdata;
  char **drop_available_mime_types;
  uint32_t drop_available_mime_type_count;
  id current_dragging_info;

  bool pending_frame_request;
  CVDisplayLinkRef display_link;
  _Atomic bool pending_frame_vblank;
  
  char *app_id;
  MARU_ContentType content_type;
  } MARU_Window_Cocoa;


typedef struct MARU_Image_Cocoa {
    MARU_Image_Base base;
    id ns_image;
} MARU_Image_Cocoa;

typedef struct MARU_Cursor_Cocoa {
    MARU_Cursor_Base base;
    id ns_cursor;
    id ns_image;
    id *ns_frame_cursors;
    id *ns_frame_images;
    uint32_t *anim_frame_delays_ms;
    uint32_t anim_frame_count;
} MARU_Cursor_Cocoa;

MARU_Status maru_createContext_Cocoa(const MARU_ContextCreateInfo *create_info,
                                      MARU_Context **out_context);
void maru_destroyContext_Cocoa(MARU_Context *context);
MARU_Status maru_updateContext_Cocoa(MARU_Context *context, uint64_t field_mask,
                                       const MARU_ContextAttributes *attributes);
MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms,
                                   MARU_EventMask mask,
                                   MARU_EventCallback callback,
                                   void *userdata);
MARU_Status maru_wakeContext_Cocoa(MARU_Context *context);
void *_maru_getContextNativeHandle_Cocoa(MARU_Context *context);

MARU_Status maru_createWindow_Cocoa(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window);
MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window);
MARU_Status maru_getWindowGeometry_Cocoa(MARU_Window *window,
                                           MARU_WindowGeometry *out_geometry);
MARU_Status maru_updateWindow_Cocoa(MARU_Window *window, uint64_t field_mask,
                                      const MARU_WindowAttributes *attributes);
MARU_Status maru_requestWindowFocus_Cocoa(MARU_Window *window);
MARU_Status maru_requestWindowFrame_Cocoa(MARU_Window *window);
MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window);
void *_maru_getWindowNativeHandle_Cocoa(MARU_Window *window);
void *_maru_getWindowNativeView_Cocoa(MARU_Window *window);
void *_maru_getWindowNativeLayer_Cocoa(MARU_Window *window);

MARU_Status maru_createCursor_Cocoa(MARU_Context *context,
                                     const MARU_CursorCreateInfo *create_info,
                                     MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_Cocoa(MARU_Cursor *cursor);
MARU_Status maru_getStandardCursor_Cocoa(MARU_Context *context,
                                          MARU_CursorShape shape,
                                          MARU_Cursor **out_cursor);

MARU_Status maru_createImage_Cocoa(MARU_Context *context,
                                    const MARU_ImageCreateInfo *create_info,
                                    MARU_Image **out_image);
MARU_Status maru_destroyImage_Cocoa(MARU_Image *image);

MARU_Status maru_getControllers_Cocoa(const MARU_Context *context, MARU_ControllerList *out_list);
void _maru_cocoa_cleanup_controller_observer(MARU_Context_Cocoa *ctx);
void _maru_cocoa_sync_controllers(MARU_Context_Base *ctx_base);
void maru_retainController_Cocoa(MARU_Controller *controller);
void maru_releaseController_Cocoa(MARU_Controller *controller);
MARU_Status maru_setControllerHapticLevels_Cocoa(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities);

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList mime_types, MARU_DropActionMask allowed_actions);
MARU_Status maru_provideData_Cocoa(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags);
MARU_Status maru_requestData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *userdata);
MARU_Status maru_getAvailableMIMETypes_Cocoa(const MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList *out_list);

MARU_Status maru_getMonitors_Cocoa(const MARU_Context *context,
                                   MARU_MonitorList *out_list);
void _maru_cocoa_refresh_monitors(MARU_Context_Base *ctx_base);
void maru_retainMonitor_Cocoa(MARU_Monitor *monitor);
void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor);
MARU_Status maru_getMonitorModes_Cocoa(const MARU_Monitor *monitor,
                                      MARU_VideoModeList *out_list);
MARU_Status maru_setMonitorMode_Cocoa(MARU_Monitor *monitor,
                                      MARU_VideoMode mode);

MARU_Status maru_getVkExtensions_Cocoa(const MARU_Context *context, MARU_StringList *out_list);
MARU_Status maru_createVkSurface_Cocoa(const MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

void _maru_cocoa_associate_window(id ns_window, MARU_Window_Cocoa *window);
MARU_Window_Cocoa *_maru_cocoa_window_from_ns_window(id ns_window);

#endif
