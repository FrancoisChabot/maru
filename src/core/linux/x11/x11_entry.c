// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "../linux_native_stubs.h"
#include "maru_backend.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/maru.h"
#include "linux_internal.h"
#include "x11_internal.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static MARU_Status maru_getControllers_X11(const MARU_Context *context,
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

static MARU_Status
maru_setControllerHapticLevels_X11(MARU_Controller *controller,
                                   uint32_t first_haptic, uint32_t count,
                                   const MARU_Scalar *intensities) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)ctrl->base.context;
  return _maru_linux_common_set_haptic_levels(&ctx->linux_common, ctrl, first_haptic, count, intensities);
}

static MARU_Window_X11 *maru_getClipboardWindow_X11(const MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)it;
    if (win->handle != None) {
      return win;
    }
  }
  return NULL;
}

static MARU_Status maru_announceClipboardData_X11_ctx(MARU_Context *context,
                                                      MARU_StringList mime_types) {
  MARU_Window_X11 *win = maru_getClipboardWindow_X11(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_announceData_X11((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                               mime_types, 0);
}

static MARU_Status maru_requestClipboardData_X11_ctx(MARU_Context *context,
                                                     const char *mime_type,
                                                     void *userdata) {
  MARU_Window_X11 *win = maru_getClipboardWindow_X11(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_requestData_X11((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                              mime_type, userdata);
}

static MARU_Status
maru_getAvailableClipboardMIMETypes_X11_ctx(const MARU_Context *context,
                                            MARU_StringList *out_list) {
  MARU_Window_X11 *win = maru_getClipboardWindow_X11(context);
  if (!win) {
    out_list->strings = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return maru_getAvailableMIMETypes_X11((MARU_Window *)win,
                                        MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                        out_list);
}

static MARU_Status maru_announceDragData_X11_win(MARU_Window *window,
                                                 MARU_StringList mime_types,
                                                 MARU_DropActionMask allowed_actions) {
  return maru_announceData_X11(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                               mime_types, allowed_actions);
}

static MARU_Status maru_requestDropData_X11_win(MARU_Window *window,
                                                const char *mime_type,
                                                void *userdata) {
  return maru_requestData_X11(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                              mime_type, userdata);
}

static MARU_Status
maru_getAvailableDropMIMETypes_X11_win(const MARU_Window *window,
                                       MARU_StringList *out_list) {
  return maru_getAvailableMIMETypes_X11(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                                        out_list);
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
  .setControllerHapticLevels = maru_setControllerHapticLevels_X11,
  .announceClipboardData = maru_announceClipboardData_X11_ctx,
  .announceDragData = maru_announceDragData_X11_win,
  .provideData = maru_provideData_X11,
  .requestClipboardData = maru_requestClipboardData_X11_ctx,
  .requestDropData = maru_requestDropData_X11_win,
  .getAvailableClipboardMIMETypes = maru_getAvailableClipboardMIMETypes_X11_ctx,
  .getAvailableDropMIMETypes = maru_getAvailableDropMIMETypes_X11_win,
  .getMonitors = maru_getMonitors_X11,
  .retainMonitor = maru_retainMonitor_X11,
  .releaseMonitor = maru_releaseMonitor_X11,
  .getMonitorModes = maru_getMonitorModes_X11,
  .setMonitorMode = maru_setMonitorMode_X11,
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

MARU_API void maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  maru_destroyContext_X11(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_updateContext_X11(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_pumpEvents_X11(context, timeout_ms, mask, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createWindow_X11(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  return maru_destroyWindow_X11(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_X11(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createCursor_X11(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_cursor_context_lost(cursor));
  return maru_destroyCursor_X11(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createImage_X11(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_RETURN_ON_ERROR(_maru_status_if_image_context_lost(image));
  return maru_destroyImage_X11(image);
}

MARU_API MARU_Status maru_getControllers(const MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
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

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  MARU_RETURN_ON_ERROR(_maru_status_if_controller_context_lost(controller));
  MARU_RETURN_ON_ERROR(_maru_status_if_controller_lost(controller));
  MARU_API_VALIDATE_LIVE(setControllerHapticLevels, controller, first_haptic,
                         count, intensities);
  return maru_setControllerHapticLevels_X11(controller, first_haptic, count,
                                            intensities);
}

MARU_API MARU_Status maru_announceClipboardData(MARU_Context *context,
                                                MARU_StringList mime_types) {
  MARU_API_VALIDATE(announceClipboardData, context, mime_types);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(announceClipboardData, context, mime_types);
  return maru_announceClipboardData_X11_ctx(context, mime_types);
}

MARU_API MARU_Status maru_announceDragData(MARU_Window *window,
                                           MARU_StringList mime_types,
                                           MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceDragData, window, mime_types, allowed_actions);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(announceDragData, window, mime_types,
                         allowed_actions);
  return maru_announceDragData_X11_win(window, mime_types, allowed_actions);
}

MARU_API MARU_Status maru_provideClipboardData(MARU_DataRequest *request,
                                               const void *data, size_t size,
                                               MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideClipboardData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_X11(request, data, size, flags);
}

MARU_API MARU_Status maru_provideDropData(MARU_DataRequest *request,
                                          const void *data, size_t size,
                                          MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideDropData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_X11(request, data, size, flags);
}

MARU_API MARU_Status maru_requestClipboardData(MARU_Context *context,
                                               const char *mime_type,
                                               void *userdata) {
  MARU_API_VALIDATE(requestClipboardData, context, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(requestClipboardData, context, mime_type, userdata);
  return maru_requestClipboardData_X11_ctx(context, mime_type, userdata);
}

MARU_API MARU_Status maru_requestDropData(MARU_Window *window,
                                          const char *mime_type,
                                          void *userdata) {
  MARU_API_VALIDATE(requestDropData, window, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestDropData, window, mime_type, userdata);
  return maru_requestDropData_X11_win(window, mime_type, userdata);
}

MARU_API MARU_Status maru_getAvailableClipboardMIMETypes(
    const MARU_Context *context, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableClipboardMIMETypes, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(getAvailableClipboardMIMETypes, context, out_list);
  return maru_getAvailableClipboardMIMETypes_X11_ctx(context, out_list);
}

MARU_API MARU_Status
maru_getAvailableDropMIMETypes(const MARU_Window *window, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableDropMIMETypes, window, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(getAvailableDropMIMETypes, window, out_list);
  return maru_getAvailableDropMIMETypes_X11_win(window, out_list);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFocus, window);
  return maru_requestWindowFocus_X11(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFrame, window);
  return maru_requestWindowFrame_X11(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowAttention, window);
  return maru_requestWindowAttention_X11(window);
}

MARU_API bool maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_X11(context);
}

MARU_API MARU_Status maru_getMonitors(const MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
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
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(getMonitorModes, monitor, out_list);
  return maru_getMonitorModes_X11(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_lost(monitor));
  MARU_API_VALIDATE_LIVE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_X11(monitor, mode);
}

MARU_API MARU_X11ContextHandle
maru_getX11ContextHandle(const MARU_Context *context) {
  MARU_API_VALIDATE(getX11ContextHandle, context);
  MARU_X11ContextHandle handle = {0};
  handle.display =
      (Display *)maru_getContextNativeHandle_X11((MARU_Context *)context);
  return handle;
}

MARU_API MARU_X11WindowHandle
maru_getX11WindowHandle(const MARU_Window *window) {
  MARU_API_VALIDATE(getX11WindowHandle, window);
  MARU_X11WindowHandle handle = {0};
  const MARU_Context *context = maru_getWindowContext(window);
  handle.display = (Display *)maru_getContextNativeHandle_X11((MARU_Context *)context);
  handle.window =
      (Window)(uintptr_t)maru_getWindowNativeHandle_X11((MARU_Window *)window);
  return handle;
}

MARU_API MARU_WaylandContextHandle
maru_getWaylandContextHandle(const MARU_Context *context) {
  MARU_API_VALIDATE(getWaylandContextHandle, context);
  (void)context;
  return _maru_stub_wayland_context_handle_unsupported();
}

MARU_API MARU_WaylandWindowHandle
maru_getWaylandWindowHandle(const MARU_Window *window) {
  MARU_API_VALIDATE(getWaylandWindowHandle, window);
  (void)window;
  return _maru_stub_wayland_window_handle_unsupported();
}

#endif
