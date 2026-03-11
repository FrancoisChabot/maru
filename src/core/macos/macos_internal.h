// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_COCOA_INTERNAL_H_INCLUDED
#define MARU_COCOA_INTERNAL_H_INCLUDED

#include "maru_internal.h"
#include "maru/c/vulkan.h"

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include "core_event_queue.h"
#include <pthread.h>

typedef struct MARU_Context_Cocoa {
  MARU_Context_Base base;

  id ns_app;
  uint64_t last_modifiers;
  id controller_observer;
  
  MARU_Controller_Base **controller_cache;
  uint32_t controller_cache_count;
  uint32_t controller_cache_capacity;
  _Atomic bool controllers_dirty;
} MARU_Context_Cocoa;

typedef struct MARU_Window_Cocoa {
  MARU_Window_Base base;

  id ns_window;
  id ns_view;
  id ns_layer;
  MARU_Vec2Dip size;
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
MARU_Status maru_destroyContext_Cocoa(MARU_Context *context);
MARU_Status maru_updateContext_Cocoa(MARU_Context *context, uint64_t field_mask,
                                       const MARU_ContextAttributes *attributes);
MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms,
                                   MARU_EventCallback callback, void *userdata);
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

MARU_Status maru_createCursor_Cocoa(MARU_Context *context,
                                     const MARU_CursorCreateInfo *create_info,
                                     MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_Cocoa(MARU_Cursor *cursor);
MARU_Status maru_resetCursorMetrics_Cocoa(MARU_Cursor *cursor);
MARU_Status maru_getStandardCursor_Cocoa(MARU_Context *context,
                                          MARU_CursorShape shape,
                                          MARU_Cursor **out_cursor);

MARU_Status maru_createImage_Cocoa(MARU_Context *context,
                                    const MARU_ImageCreateInfo *create_info,
                                    MARU_Image **out_image);
MARU_Status maru_destroyImage_Cocoa(MARU_Image *image);

MARU_Status maru_getControllers_Cocoa(MARU_Context *context, MARU_ControllerList *out_list);
void _maru_cocoa_cleanup_controller_observer(MARU_Context_Cocoa *ctx);
void _maru_cocoa_sync_controllers(MARU_Context_Base *ctx_base);
MARU_Status maru_retainController_Cocoa(MARU_Controller *controller);
MARU_Status maru_releaseController_Cocoa(MARU_Controller *controller);
MARU_Status maru_resetControllerMetrics_Cocoa(MARU_Controller *controller);
MARU_Status maru_getControllerInfo_Cocoa(MARU_Controller *controller, MARU_ControllerInfo *out_info);
MARU_Status maru_setControllerHapticLevels_Cocoa(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities);

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions);
MARU_Status maru_provideData_Cocoa(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags);
MARU_Status maru_requestData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag);
MARU_Status maru_getAvailableMIMETypes_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list);

MARU_Status maru_getMonitors_Cocoa(MARU_Context *context,
                                   MARU_MonitorList *out_list);
void maru_retainMonitor_Cocoa(MARU_Monitor *monitor);
void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor);
const MARU_VideoMode *maru_getMonitorModes_Cocoa(const MARU_Monitor *monitor,
                                                  uint32_t *out_count);
MARU_Status maru_setMonitorMode_Cocoa(const MARU_Monitor *monitor,
                                       MARU_VideoMode mode);
void maru_resetMonitorMetrics_Cocoa(MARU_Monitor *monitor);

const char **maru_getVkExtensions_Cocoa(const MARU_Context *context, uint32_t *out_count);
MARU_Status maru_createVkSurface_Cocoa(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

void _maru_cocoa_associate_window(id ns_window, MARU_Window_Cocoa *window);
MARU_Window_Cocoa *_maru_cocoa_window_from_ns_window(id ns_window);

#endif
