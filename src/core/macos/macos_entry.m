// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "macos_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/native/cocoa.h"
#include <stdatomic.h>
#include <string.h>

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_Cocoa = {
  .destroyContext = maru_destroyContext_Cocoa,
  .updateContext = maru_updateContext_Cocoa,
  .pumpEvents = maru_pumpEvents_Cocoa,
  .wakeContext = maru_wakeContext_Cocoa,

  .createWindow = maru_createWindow_Cocoa,
  .destroyWindow = maru_destroyWindow_Cocoa,
  .getWindowGeometry = maru_getWindowGeometry_Cocoa,
  .updateWindow = maru_updateWindow_Cocoa,
  .requestWindowFocus = maru_requestWindowFocus_Cocoa,
  .requestWindowFrame = maru_requestWindowFrame_Cocoa,
  .requestWindowAttention = maru_requestWindowAttention_Cocoa,

  .createCursor = maru_createCursor_Cocoa,
  .destroyCursor = maru_destroyCursor_Cocoa,
  .createImage = maru_createImage_Cocoa,
  .destroyImage = maru_destroyImage_Cocoa,

  .getControllers = maru_getControllers_Cocoa,
  .retainController = maru_retainController_Cocoa,
  .releaseController = maru_releaseController_Cocoa,
  .resetControllerMetrics = maru_resetControllerMetrics_Cocoa,
  .getControllerInfo = maru_getControllerInfo_Cocoa,
  .setControllerHapticLevels = maru_setControllerHapticLevels_Cocoa,

  .announceData = maru_announceData_Cocoa,
  .provideData = maru_provideData_Cocoa,
  .requestData = maru_requestData_Cocoa,
  .getAvailableMIMETypes = maru_getAvailableMIMETypes_Cocoa,
  
  .getMonitors = maru_getMonitors_Cocoa,
  .retainMonitor = maru_retainMonitor_Cocoa,
  .releaseMonitor = maru_releaseMonitor_Cocoa,
  .getMonitorModes = maru_getMonitorModes_Cocoa,
  .setMonitorMode = maru_setMonitorMode_Cocoa,
  .resetMonitorMetrics = maru_resetMonitorMetrics_Cocoa,

  .getContextNativeHandle = _maru_getContextNativeHandle_Cocoa,
  .getWindowNativeHandle = _maru_getWindowNativeHandle_Cocoa,

  .getVkExtensions = maru_getVkExtensions_Cocoa,
  .createVkSurface = maru_createVkSurface_Cocoa,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                         MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Cocoa(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_Cocoa(context);
}

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  return maru_updateContext_Cocoa(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_Cocoa(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_Cocoa(context);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                        const MARU_WindowCreateInfo *create_info,
                                        MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_Cocoa(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_Cocoa(window);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                             MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_Cocoa(window, out_geometry);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_Cocoa(window, field_mask, attributes);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_Cocoa(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  return maru_requestWindowFrame_Cocoa(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_Cocoa(window);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_Cocoa(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_Cocoa(cursor);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  return maru_resetCursorMetrics_Cocoa(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_Cocoa(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_Cocoa(image);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  return maru_getControllers_Cocoa(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  return maru_retainController_Cocoa(controller);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  return maru_releaseController_Cocoa(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_Cocoa(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_Cocoa(controller, out_info);
}

MARU_API MARU_Status maru_setControllerHapticLevels(
    MARU_Controller *controller, uint32_t first_haptic, uint32_t count,
    const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count, intensities);
  return maru_setControllerHapticLevels_Cocoa(controller, first_haptic, count, intensities);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                MARU_DataExchangeTarget target,
                                const char **mime_types,
                                uint32_t count,
                                MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count, allowed_actions);
  return maru_announceData_Cocoa(window, target, mime_types, count, allowed_actions);
}

MARU_API MARU_Status maru_provideData(
    const MARU_DataRequestEvent *request_event, const void *data, size_t size,
    MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  return maru_provideData_Cocoa(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                               MARU_DataExchangeTarget target,
                               const char *mime_type,
                               void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  return maru_requestData_Cocoa(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_Cocoa(window, target, out_list);
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  return maru_getMonitors_Cocoa(context, out_count);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_Cocoa(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_Cocoa(monitor);
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_Cocoa(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_Cocoa(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  maru_resetMonitorMetrics_Cocoa(monitor);
}

MARU_API const char **maru_getVkExtensions(const MARU_Context *context,
                                                 uint32_t *out_count) {
  MARU_API_VALIDATE(getVkExtensions, context, out_count);
  return maru_getVkExtensions_Cocoa(context, out_count);
}

MARU_API MARU_Status maru_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);
  return maru_createVkSurface_Cocoa(window, instance, vk_loader, out_surface);
}

MARU_API MARU_Status maru_getCocoaContextHandle(
    MARU_Context *context, MARU_CocoaContextHandle *out_handle) {
  MARU_API_VALIDATE(getCocoaContextHandle, context, out_handle);
  out_handle->ns_application = _maru_getContextNativeHandle_Cocoa(context);
  return out_handle->ns_application ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getCocoaWindowHandle(
    MARU_Window *window, MARU_CocoaWindowHandle *out_handle) {
  MARU_API_VALIDATE(getCocoaWindowHandle, window, out_handle);
  out_handle->ns_window = _maru_getWindowNativeHandle_Cocoa(window);
  return out_handle->ns_window ? MARU_SUCCESS : MARU_FAILURE;
}
#endif
