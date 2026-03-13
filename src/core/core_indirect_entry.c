#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "maru/native/cocoa.h"
#include "maru/native/linux.h"
#include "maru/native/wayland.h"
#include "maru/native/win32.h"
#include "maru/native/x11.h"

#ifdef MARU_INDIRECT_BACKEND
MARU_API void maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  ctx_base->backend->destroyContext(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->updateContext(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->pumpEvents(context, timeout_ms, mask, callback,
                                       userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createWindow(context, create_info, out_window);
}

MARU_API bool maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  if (maru_isContextLost(context)) {
    return false;
  }
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->wakeContext(context);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  if (!ctx_base->backend->getControllers) {
    out_list->controllers = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return ctx_base->backend->getControllers(context, out_list);
}

MARU_API void maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  MARU_Controller_Base *ctrl_base = (MARU_Controller_Base *)controller;
  if (ctrl_base->backend->retainController) {
    ctrl_base->backend->retainController(controller);
    return;
  }
  atomic_fetch_add_explicit(&ctrl_base->ref_count, 1u, memory_order_relaxed);
}

MARU_API MARU_Status maru_getControllerInfo(const MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  MARU_RETURN_ON_ERROR(
      _maru_status_if_controller_context_lost(controller));
  MARU_API_VALIDATE_LIVE(getControllerInfo, controller, out_info);
  MARU_ControllerPrefix *ctrl = (MARU_ControllerPrefix *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->getControllerInfo) {
    memset(out_info, 0, sizeof(*out_info));
    return MARU_FAILURE;
  }
  return ctx_base->backend->getControllerInfo(controller, out_info);
}

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  MARU_RETURN_ON_ERROR(
      _maru_status_if_controller_context_lost(controller));
  MARU_API_VALIDATE_LIVE(setControllerHapticLevels, controller, first_haptic,
                         count, intensities);
  MARU_ControllerPrefix *ctrl = (MARU_ControllerPrefix *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->setControllerHapticLevels) return MARU_FAILURE;
  return ctx_base->backend->setControllerHapticLevels(controller, first_haptic,
                                                      count, intensities);
}

MARU_API void maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  MARU_Controller_Base *ctrl_base = (MARU_Controller_Base *)controller;
  if (ctrl_base->backend->releaseController) {
    ctrl_base->backend->releaseController(controller);
    return;
  }
  uint32_t current =
      atomic_load_explicit(&ctrl_base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(
            &ctrl_base->ref_count, &current, current - 1u,
            memory_order_acq_rel, memory_order_acquire)) {
      break;
    }
  }
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       MARU_MIMETypeList mime_types,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, allowed_actions);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(announceData, window, target, mime_types,
                         allowed_actions);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->announceData) return MARU_FAILURE;
  return win_base->backend->announceData(window, target, mime_types,
                                         allowed_actions);
}

MARU_API MARU_Status maru_provideData(MARU_DataRequest *request,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request;
  if (!handle || !handle->ctx_base || !handle->ctx_base->backend ||
      !handle->ctx_base->backend->provideData) {
    return MARU_FAILURE;
  }
  return handle->ctx_base->backend->provideData(request, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *userdata) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestData, window, target, mime_type, userdata);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->requestData) return MARU_FAILURE;
  return win_base->backend->requestData(window, target, mime_type, userdata);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(getAvailableMIMETypes, window, target, out_list);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->getAvailableMIMETypes) {
    out_list->mime_types = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return win_base->backend->getAvailableMIMETypes(window, target, out_list);
}

MARU_API MARU_Status maru_getVkExtensions(const MARU_Context *context,
                                          MARU_VkExtensionList *out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  if (!context || !out_list) {
    return MARU_FAILURE;
  }
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  if (!ctx_base->backend->getVkExtensions) {
    out_list->count = 0;
    out_list->names = NULL;
    return MARU_SUCCESS;
  }
  return ctx_base->backend->getVkExtensions(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(createVkSurface, window, instance, vk_loader,
                         out_surface);
  if (!window || !instance || !out_surface) {
    return MARU_FAILURE;
  }
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->createVkSurface) {
    return MARU_FAILURE;
  }
  return win_base->backend->createVkSurface(window, instance, vk_loader,
                                            out_surface);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  return win_base->backend->destroyWindow(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(updateWindow, window, field_mask, attributes);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->updateWindow(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createCursor(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_cursor_context_lost(cursor));
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  return cur_base->backend->destroyCursor(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createImage(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_RETURN_ON_ERROR(_maru_status_if_image_context_lost(image));
  MARU_Image_Base *image_base = (MARU_Image_Base *)image;
  return image_base->backend->destroyImage(image);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFocus, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->requestWindowFocus(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFrame, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->requestWindowFrame) {
    return win_base->backend->requestWindowFrame(window);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowAttention, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->requestWindowAttention) {
    return win_base->backend->requestWindowAttention(window);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->getMonitors(context, out_list);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  if (mon_base->backend->retainMonitor) {
    mon_base->backend->retainMonitor(monitor);
    return;
  }
  atomic_fetch_add_explicit(&mon_base->ref_count, 1u, memory_order_relaxed);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  if (mon_base->backend->releaseMonitor) {
    mon_base->backend->releaseMonitor(monitor);
    return;
  }
  uint32_t current = atomic_load_explicit(&mon_base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&mon_base->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      break;
    }
  }
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(getMonitorModes, monitor, out_list);
  if (!monitor || !out_list) {
    return MARU_FAILURE;
  }
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  return mon_base->backend->getMonitorModes(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(setMonitorMode, monitor, mode);
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  return mon_base->backend->setMonitorMode(monitor, mode);
}

static void *_maru_getContextNativeHandleRaw(MARU_Context *context) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->getContextNativeHandle(context);
}

static void *_maru_getWindowNativeHandleRaw(MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->getWindowNativeHandle(window);
}

MARU_API MARU_WaylandContextHandle
maru_getWaylandContextHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getWaylandContextHandle, context);
  MARU_WaylandContextHandle handle = {0};
  handle.display = (wl_display *)_maru_getContextNativeHandleRaw(context);
  return handle;
}

MARU_API MARU_WaylandWindowHandle
maru_getWaylandWindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getWaylandWindowHandle, window);
  MARU_WaylandWindowHandle handle = {0};
  MARU_Context *context = maru_getWindowContext(window);
  handle.display = (wl_display *)_maru_getContextNativeHandleRaw(context);
  handle.surface = (wl_surface *)_maru_getWindowNativeHandleRaw(window);
  return handle;
}

MARU_API MARU_X11ContextHandle maru_getX11ContextHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getX11ContextHandle, context);
  MARU_X11ContextHandle handle = {0};
  handle.display = (Display *)_maru_getContextNativeHandleRaw(context);
  return handle;
}

MARU_API MARU_X11WindowHandle maru_getX11WindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getX11WindowHandle, window);
  MARU_X11WindowHandle handle = {0};
  MARU_Context *context = maru_getWindowContext(window);
  handle.display = (Display *)_maru_getContextNativeHandleRaw(context);
  handle.window = (Window)(uintptr_t)_maru_getWindowNativeHandleRaw(window);
  return handle;
}

#ifdef MARU_ENABLE_BACKEND_WINDOWS
MARU_API MARU_Win32ContextHandle
maru_getWin32ContextHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getWin32ContextHandle, context);
  MARU_Win32ContextHandle handle = {0};
  handle.instance = (HINSTANCE)_maru_getContextNativeHandleRaw(context);
  return handle;
}

MARU_API MARU_Win32WindowHandle
maru_getWin32WindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getWin32WindowHandle, window);
  MARU_Win32WindowHandle handle = {0};
  MARU_Context *context = maru_getWindowContext(window);
  handle.instance = (HINSTANCE)_maru_getContextNativeHandleRaw(context);
  handle.hwnd = (HWND)_maru_getWindowNativeHandleRaw(window);
  return handle;
}
#endif

#ifdef MARU_ENABLE_BACKEND_COCOA
MARU_API MARU_CocoaContextHandle maru_getCocoaContextHandle(
    MARU_Context *context) {
  MARU_API_VALIDATE(getCocoaContextHandle, context);
  MARU_CocoaContextHandle handle = {0};
  handle.ns_application = _maru_getContextNativeHandleRaw(context);
  return handle;
}

MARU_API MARU_CocoaWindowHandle maru_getCocoaWindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getCocoaWindowHandle, window);
  MARU_CocoaWindowHandle handle = {0};
  handle.ns_window = _maru_getWindowNativeHandleRaw(window);
  return handle;
}
#endif

MARU_API MARU_LinuxContextHandle maru_getLinuxContextHandle(
    MARU_Context *context) {
  MARU_API_VALIDATE(getLinuxContextHandle, context);
  MARU_LinuxContextHandle handle = {0};
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  handle.backend = ctx_base->pub.backend_type;
  switch (ctx_base->pub.backend_type) {
    case MARU_BACKEND_WAYLAND:
      handle.wayland = maru_getWaylandContextHandle(context);
      break;
    case MARU_BACKEND_X11:
      handle.x11 = maru_getX11ContextHandle(context);
      break;
    default:
      break;
  }
  return handle;
}

MARU_API MARU_LinuxWindowHandle maru_getLinuxWindowHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getLinuxWindowHandle, window);
  MARU_LinuxWindowHandle handle = {0};
  MARU_Context *context = maru_getWindowContext(window);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  handle.backend = ctx_base->pub.backend_type;
  switch (ctx_base->pub.backend_type) {
    case MARU_BACKEND_WAYLAND:
      handle.wayland = maru_getWaylandWindowHandle(window);
      break;
    case MARU_BACKEND_X11:
      handle.x11 = maru_getX11WindowHandle(window);
      break;
    default:
      break;
  }
  return handle;
}
#endif
