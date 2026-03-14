// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru/native/win32.h"
#include <stdatomic.h>
#include <string.h>

static MARU_Window_Windows *maru_getClipboardWindow_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)it;
    if (win->hwnd != NULL) {
      return win;
    }
  }
  return NULL;
}

// --- Contexts (contexts.h) ---

MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                         MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Windows(create_info, out_context);
}

MARU_API void maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  maru_destroyContext_Windows(context);
}

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_updateContext_Windows(context, field_mask, attributes);
}

// --- Events (events.h) ---

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_pumpEvents_Windows(context, timeout_ms, mask, callback, userdata);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_Windows(context);
}

// --- Windows (windows.h) ---

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                        const MARU_WindowCreateInfo *create_info,
                                        MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createWindow_Windows(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  return maru_destroyWindow_Windows(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(updateWindow, window, field_mask, attributes);
  const MARU_WindowAttributes *attrs_arg = attributes;
  MARU_WindowAttributes attrs_copy;
  if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
    attrs_copy = *attributes;
    _maru_window_attributes_apply_presentation_state(
        &attrs_copy, attrs_copy.presentation_state);
    attrs_arg = &attrs_copy;
  }
  return maru_updateWindow_Windows(window, field_mask, attrs_arg);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFocus, window);
  return maru_requestWindowFocus_Windows(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFrame, window);
  return maru_requestWindowFrame_Windows(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowAttention, window);
  return maru_requestWindowAttention_Windows(window);
}

// --- Images (images.h) ---

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createImage_Windows(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_RETURN_ON_ERROR(_maru_status_if_image_context_lost(image));
  return maru_destroyImage_Windows(image);
}

// --- Cursors (cursors.h) ---

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createCursor_Windows(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_cursor_context_lost(cursor));
  return maru_destroyCursor_Windows(cursor);
}

// --- Monitors (monitors.h) ---

MARU_API MARU_Status maru_getMonitors(const MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_Status res = maru_updateMonitors_Windows(context);
  if (res != MARU_SUCCESS) {
    out_list->monitors = NULL;
    out_list->count = 0;
    return res;
  }
  
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  out_list->monitors = ctx_base->monitor_cache;
  out_list->count = ctx_base->monitor_cache_count;
  return MARU_SUCCESS;
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  atomic_fetch_add_explicit(&mon_base->ref_count, 1u, memory_order_relaxed);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  uint32_t current = atomic_load_explicit(&mon_base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&mon_base->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u && !mon_base->is_active) {
        _maru_monitor_free(mon_base);
      }
      return;
    }
  }
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(getMonitorModes, monitor, out_list);
  return maru_getMonitorModes_Windows(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_lost(monitor));
  MARU_API_VALIDATE_LIVE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_Windows(monitor, mode);
}

// --- Controllers (controllers.h) ---

MARU_API MARU_Status maru_getControllers(const MARU_Context *context, MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_getControllers_Windows(context, out_list);
}

MARU_API void maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  maru_retainController_Windows(controller);
}

MARU_API void maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  maru_releaseController_Windows(controller);
}

MARU_API MARU_Status maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count, intensities);
  MARU_RETURN_ON_ERROR(_maru_status_if_controller_context_lost(controller));
  MARU_RETURN_ON_ERROR(_maru_status_if_controller_lost(controller));
  MARU_API_VALIDATE_LIVE(setControllerHapticLevels, controller, first_haptic, count, intensities);
  return maru_setControllerHapticLevels_Windows(controller, first_haptic, count, intensities);
}

// --- Data Exchange (data_exchange.h) ---

MARU_API MARU_Status maru_announceClipboardData(MARU_Context *context, MARU_StringList mime_types) {
  MARU_API_VALIDATE(announceClipboardData, context, mime_types);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(announceClipboardData, context, mime_types);
  MARU_Window_Windows *win = maru_getClipboardWindow_Windows(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_announceData_Windows((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, mime_types, 0);
}

MARU_API MARU_Status maru_announceDragData(MARU_Window *window, MARU_StringList mime_types, MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceDragData, window, mime_types, allowed_actions);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(announceDragData, window, mime_types, allowed_actions);
  return maru_announceData_Windows(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, mime_types, allowed_actions);
}

MARU_API MARU_Status maru_provideClipboardData(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideClipboardData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_Windows(request, data, size, flags);
}

MARU_API MARU_Status maru_provideDropData(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideDropData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_Windows(request, data, size, flags);
}

MARU_API MARU_Status maru_requestClipboardData(MARU_Context *context, const char *mime_type, void *userdata) {
  MARU_API_VALIDATE(requestClipboardData, context, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(requestClipboardData, context, mime_type, userdata);
  MARU_Window_Windows *win = maru_getClipboardWindow_Windows(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_requestData_Windows((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, mime_type, userdata);
}

MARU_API MARU_Status maru_requestDropData(MARU_Window *window, const char *mime_type, void *userdata) {
  MARU_API_VALIDATE(requestDropData, window, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestDropData, window, mime_type, userdata);
  return maru_requestData_Windows(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, mime_type, userdata);
}

MARU_API MARU_Status maru_getAvailableClipboardMIMETypes(const MARU_Context *context, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableClipboardMIMETypes, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(getAvailableClipboardMIMETypes, context, out_list);
  MARU_Window_Windows *win = maru_getClipboardWindow_Windows(context);
  if (!win) {
    out_list->strings = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return maru_getAvailableMIMETypes_Windows((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, out_list);
}

MARU_API MARU_Status maru_getAvailableDropMIMETypes(const MARU_Window *window, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableDropMIMETypes, window, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(getAvailableDropMIMETypes, window, out_list);
  return maru_getAvailableMIMETypes_Windows(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, out_list);
}

// --- Vulkan (vulkan.h) ---

MARU_API MARU_Status maru_getVkExtensions(const MARU_Context *context, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_getVkExtensions_Windows(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(const MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(createVkSurface, window, instance, vk_loader, out_surface);
  return maru_createVkSurface_Windows(window, instance, vk_loader, out_surface);
}

// --- Win32 Native Handles ---

MARU_API MARU_Win32ContextHandle
maru_getWin32ContextHandle(const MARU_Context *context) {
  MARU_API_VALIDATE(getWin32ContextHandle, context);
  MARU_Win32ContextHandle handle = {0};
  handle.instance =
      (HINSTANCE)_maru_getContextNativeHandle_Windows((MARU_Context *)context);
  return handle;
}

MARU_API MARU_Win32WindowHandle
maru_getWin32WindowHandle(const MARU_Window *window) {
  MARU_API_VALIDATE(getWin32WindowHandle, window);
  MARU_Win32WindowHandle handle = {0};
  const MARU_Context *context = maru_getWindowContext(window);
  handle.instance = (HINSTANCE)_maru_getContextNativeHandle_Windows((MARU_Context *)context);
  handle.hwnd =
      (HWND)_maru_getWindowNativeHandle_Windows((MARU_Window *)window);
  return handle;
}
