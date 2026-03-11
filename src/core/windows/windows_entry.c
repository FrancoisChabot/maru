// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/native/win32.h"
#include <stdatomic.h>
#include <string.h>

// --- Contexts (contexts.h) ---

MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                         MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Windows(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_Windows(context);
}

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  return maru_updateContext_Windows(context, field_mask, attributes);
}

// --- Events (events.h) ---

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
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
  return maru_createWindow_Windows(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_Windows(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_Windows(window, field_mask, attributes);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_Windows(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  return maru_requestWindowFrame_Windows(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_Windows(window);
}

// --- Images (images.h) ---

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_Windows(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_Windows(image);
}

// --- Cursors (cursors.h) ---

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_Windows(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_Windows(cursor);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  return maru_resetCursorMetrics_Windows(cursor);
}

// --- Monitors (monitors.h) ---

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
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

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_Windows(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_Windows(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  maru_resetMonitorMetrics_Windows(monitor);
}

// --- Controllers (controllers.h) ---

MARU_API MARU_Status maru_getControllers(MARU_Context *context, MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  return maru_getControllers_Windows(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  return maru_retainController_Windows(controller);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  return maru_releaseController_Windows(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_Windows(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller, MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_Windows(controller, out_info);
}

MARU_API MARU_Status maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count, intensities);
  return maru_setControllerHapticLevels_Windows(controller, first_haptic, count, intensities);
}

// --- Data Exchange (data_exchange.h) ---

MARU_API MARU_Status maru_announceData(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count, allowed_actions);
  return maru_announceData_Windows(window, target, mime_types, count, allowed_actions);
}

MARU_API MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  return maru_provideData_Windows(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  return maru_requestData_Windows(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_Windows(window, target, out_list);
}

// --- Vulkan (vulkan.h) ---

MARU_API const char **maru_getVkExtensions(const MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getVkExtensions, context, out_count);
  return maru_getVkExtensions_Windows(context, out_count);
}

MARU_API MARU_Status maru_createVkSurface(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);
  return maru_createVkSurface_Windows(window, instance, vk_loader, out_surface);
}

// --- Win32 Native Handles ---

MARU_API MARU_Status maru_getWin32ContextHandle(
    MARU_Context *context, MARU_Win32ContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandle_Windows(context);
  return out_handle->instance ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWin32WindowHandle(
    MARU_Window *window, MARU_Win32WindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandle_Windows(context);
  out_handle->hwnd = (HWND)_maru_getWindowNativeHandle_Windows(window);
  return (out_handle->instance && out_handle->hwnd) ? MARU_SUCCESS : MARU_FAILURE;
}
