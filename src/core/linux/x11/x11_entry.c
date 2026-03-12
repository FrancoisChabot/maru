// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "../linux_native_stubs.h"
#include "maru_backend.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/cursors.h"
#include "maru/c/monitors.h"
#include "maru/c/native/linux.h"
#include "linux_internal.h"
#include "x11_internal.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static MARU_Status maru_getControllers_X11(MARU_Context *context,
                                           MARU_ControllerList *out_list) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  MARU_Context_Linux_Common *common = &ctx->linux_common;

  if (common->controller_count > ctx->controller_list_capacity) {
    uint32_t new_capacity = common->controller_count;
    if (new_capacity < 8) new_capacity = 8;
    MARU_Controller **new_list = (MARU_Controller **)maru_context_realloc(
        &ctx->base, ctx->controller_list_storage,
        ctx->controller_list_capacity * sizeof(MARU_Controller *),
        new_capacity * sizeof(MARU_Controller *));
    if (!new_list) return MARU_FAILURE;
    ctx->controller_list_storage = new_list;
    ctx->controller_list_capacity = new_capacity;
  }

  uint32_t i = 0;
  for (MARU_LinuxController * it = common->controllers; it; it = it->next) {
    ctx->controller_list_storage[i++] = (MARU_Controller *)it;
  }

  out_list->controllers = ctx->controller_list_storage;
  out_list->count = common->controller_count;
  return MARU_SUCCESS;
}

static void maru_retainController_X11(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
}

static void maru_releaseController_X11(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u) {
        MARU_Context_X11 *ctx = (MARU_Context_X11 *)ctrl->base.context;
        _maru_linux_controller_destroy(&ctx->base, ctrl);
      }
      break;
    }
  }
}

static MARU_Status maru_resetControllerMetrics_X11(MARU_Controller *controller) {
  (void)controller;
  return MARU_SUCCESS;
}

static MARU_Status maru_getControllerInfo_X11(const MARU_Controller *controller,
                                              MARU_ControllerInfo *out_info) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  memset(out_info, 0, sizeof(*out_info));
  out_info->name = ctrl->base.name;
  out_info->vendor_id = ctrl->base.vendor_id;
  out_info->product_id = ctrl->base.product_id;
  out_info->version = ctrl->base.version;
  memcpy(out_info->guid, ctrl->base.guid, 16);
  out_info->is_standardized = ctrl->is_standardized;
  return MARU_SUCCESS;
}

static MARU_Status
maru_setControllerHapticLevels_X11(MARU_Controller *controller,
                                   uint32_t first_haptic, uint32_t count,
                                   const MARU_Scalar *intensities) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)ctrl->base.context;
  return _maru_linux_common_set_haptic_levels(&ctx->linux_common, ctrl, first_haptic, count, intensities);
}

MARU_API bool maru_x11SupportsExtendedFrameSync(MARU_Context *context) {
  if (!context) return false;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_X11) {
    return false;
  }
  return ((MARU_Context_X11 *)context)->compositor_supports_extended_frame_sync;
}

#ifdef MARU_INDIRECT_BACKEND
static void *maru_getWindowNativeHandle_X11_internal(MARU_Window *window) {
  return maru_getWindowNativeHandle_X11(window);
}

const MARU_Backend maru_backend_X11 = {
  .destroyContext = maru_destroyContext_X11,
  .updateContext = maru_updateContext_X11,
  .pumpEvents = maru_pumpEvents_X11,
  .wakeContext = maru_wakeContext_X11,
  .createWindow = maru_createWindow_X11,
  .destroyWindow = maru_destroyWindow_X11,
  .updateWindow = maru_updateWindow_X11,
  .requestWindowFocus = maru_requestWindowFocus_X11,
  .requestWindowFrame = maru_requestWindowFrame_X11,
  .requestWindowAttention = maru_requestWindowAttention_X11,
  .createCursor = maru_createCursor_X11,
  .destroyCursor = maru_destroyCursor_X11,
  .createImage = maru_createImage_X11,
  .destroyImage = maru_destroyImage_X11,
  .getControllers = maru_getControllers_X11,
  .retainController = maru_retainController_X11,
  .releaseController = maru_releaseController_X11,
  .resetControllerMetrics = maru_resetControllerMetrics_X11,
  .getControllerInfo = maru_getControllerInfo_X11,
  .setControllerHapticLevels = maru_setControllerHapticLevels_X11,
  .announceData = maru_announceData_X11,
  .provideData = maru_provideData_X11,
  .requestData = maru_requestData_X11,
  .getAvailableMIMETypes = maru_getAvailableMIMETypes_X11,
  .getMonitors = maru_getMonitors_X11,
  .retainMonitor = maru_retainMonitor_X11,
  .releaseMonitor = maru_releaseMonitor_X11,
  .getMonitorModes = maru_getMonitorModes_X11,
  .setMonitorMode = maru_setMonitorMode_X11,
  .resetMonitorMetrics = maru_resetMonitorMetrics_X11,
  .getContextNativeHandle = maru_getContextNativeHandle_X11,
  .getWindowNativeHandle = maru_getWindowNativeHandle_X11_internal,
  .getVkExtensions = maru_getVkExtensions_X11,
  .createVkSurface = maru_createVkSurface_X11,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_X11(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_X11(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_updateContext_X11(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_pumpEvents_X11(context, timeout_ms, mask, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_createWindow_X11(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  return maru_destroyWindow_X11(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_X11(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_createCursor_X11(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_cursor_context_lost(cursor));
  return maru_destroyCursor_X11(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_createImage_X11(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_image_context_lost(image));
  return maru_destroyImage_X11(image);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_getControllers_X11(context, out_list);
}

MARU_API void maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  maru_retainController_X11(controller);
}

MARU_API void maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  maru_releaseController_X11(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_controller_context_lost(controller));
  MARU_API_VALIDATE_LIVE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_X11(controller);
}

MARU_API MARU_Status maru_getControllerInfo(const MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_controller_context_lost(controller));
  MARU_API_VALIDATE_LIVE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_X11(controller, out_info);
}

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_controller_context_lost(controller));
  MARU_API_VALIDATE_LIVE(setControllerHapticLevels, controller, first_haptic,
                         count, intensities);
  return maru_setControllerHapticLevels_X11(controller, first_haptic, count,
                                            intensities);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char **mime_types, uint32_t count,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count,
                    allowed_actions);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(announceData, window, target, mime_types, count,
                         allowed_actions);
  return maru_announceData_X11(window, target, mime_types, count,
                               allowed_actions);
}

MARU_API MARU_Status maru_provideData(MARU_DataRequest *request,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request, data, size, flags);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_request_context_lost(request));
  return maru_provideData_X11(request, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *userdata) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, userdata);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestData, window, target, mime_type, userdata);
  return maru_requestData_X11(window, target, mime_type, userdata);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_X11(window, target, out_list);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_cursor_context_lost(cursor));
  return maru_resetCursorMetrics_X11(cursor);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFocus, window);
  return maru_requestWindowFocus_X11(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFrame, window);
  return maru_requestWindowFrame_X11(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowAttention, window);
  return maru_requestWindowAttention_X11(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_wakeContext_X11(context);
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_context_lost(context));
  return maru_getMonitors_X11(context, out_list);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_X11(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_X11(monitor);
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(getMonitorModes, monitor, out_list);
  return maru_getMonitorModes_X11(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_X11(monitor, mode);
}

MARU_API MARU_Status maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_RETURN_IF_CONTEXT_LOST(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(resetMonitorMetrics, monitor);
  return maru_resetMonitorMetrics_X11(monitor);
}

MARU_API MARU_X11ContextHandle maru_getX11ContextHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getX11ContextHandle, context);
  MARU_X11ContextHandle handle = {0};
  handle.display = (Display *)maru_getContextNativeHandle_X11(context);
  return handle;
}

MARU_API MARU_X11WindowHandle maru_getX11WindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getX11WindowHandle, window);
  MARU_X11WindowHandle handle = {0};
  MARU_Context *context = maru_getWindowContext(window);
  handle.display = (Display *)maru_getContextNativeHandle_X11(context);
  handle.window = (Window)(uintptr_t)maru_getWindowNativeHandle_X11(window);
  return handle;
}

MARU_API MARU_WaylandContextHandle
maru_getWaylandContextHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getWaylandContextHandle, context);
  (void)context;
  return _maru_stub_wayland_context_handle_unsupported();
}

MARU_API MARU_WaylandWindowHandle maru_getWaylandWindowHandle(
    MARU_Window *window) {
  MARU_API_VALIDATE(getWaylandWindowHandle, window);
  (void)window;
  return _maru_stub_wayland_window_handle_unsupported();
}

MARU_API MARU_LinuxContextHandle maru_getLinuxContextHandle(
    MARU_Context *context) {
  MARU_API_VALIDATE(getLinuxContextHandle, context);
  MARU_LinuxContextHandle handle = {0};
  handle.backend = MARU_BACKEND_X11;
  handle.x11 = maru_getX11ContextHandle(context);
  return handle;
}

MARU_API MARU_LinuxWindowHandle maru_getLinuxWindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getLinuxWindowHandle, window);
  MARU_LinuxWindowHandle handle = {0};
  handle.backend = MARU_BACKEND_X11;
  handle.x11 = maru_getX11WindowHandle(window);
  return handle;
}
#endif
