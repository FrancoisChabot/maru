#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/details/contexts.h"
#include "maru/c/details/controllers.h"

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "maru/c/native/cocoa.h"
#include "maru/c/native/linux.h"
#include "maru/c/native/wayland.h"
#include "maru/c/native/win32.h"
#include "maru/c/native/x11.h"

#ifdef MARU_INDIRECT_BACKEND
MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->destroyContext(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->updateContext(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->pumpEvents(context, timeout_ms, mask, callback,
                                       userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createWindow(context, create_info, out_window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->wakeContext(context);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  if (!ctx_base->backend->getControllers) {
    out_list->controllers = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return ctx_base->backend->getControllers(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  MARU_ControllerExposed *ctrl = (MARU_ControllerExposed *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->retainController) return MARU_FAILURE;
  return ctx_base->backend->retainController(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  MARU_ControllerExposed *ctrl = (MARU_ControllerExposed *)controller;
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
  MARU_ControllerExposed *ctrl = (MARU_ControllerExposed *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->setControllerHapticLevels) return MARU_FAILURE;
  return ctx_base->backend->setControllerHapticLevels(controller, first_haptic,
                                                      count, intensities);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  MARU_ControllerExposed *ctrl = (MARU_ControllerExposed *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->releaseController) return MARU_FAILURE;
  return ctx_base->backend->releaseController(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  MARU_ControllerExposed *ctrl = (MARU_ControllerExposed *)controller;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->context;
  if (!ctx_base->backend->resetControllerMetrics) return MARU_FAILURE;
  return ctx_base->backend->resetControllerMetrics(controller);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char **mime_types, uint32_t count,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count,
                    allowed_actions);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->announceData) return MARU_FAILURE;
  return win_base->backend->announceData(window, target, mime_types, count,
                                         allowed_actions);
}

MARU_API MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request_event->internal_handle;
  if (!handle || !handle->ctx_base || !handle->ctx_base->backend ||
      !handle->ctx_base->backend->provideData) {
    return MARU_FAILURE;
  }
  return handle->ctx_base->backend->provideData(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->requestData) return MARU_FAILURE;
  return win_base->backend->requestData(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  if (!win_base->backend->getAvailableMIMETypes) {
    out_list->mime_types = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return win_base->backend->getAvailableMIMETypes(window, target, out_list);
}

MARU_API const char **maru_getVkExtensions(const MARU_Context *context,
                                                  uint32_t *out_count) {
  MARU_API_VALIDATE(getVkExtensions, context, out_count);
  if (!context || !out_count) {
    return NULL;
  }
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  if (!ctx_base->backend->getVkExtensions) {
    *out_count = 0;
    return NULL;
  }
  return ctx_base->backend->getVkExtensions(context, out_count);
}

MARU_API MARU_Status maru_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);
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
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  return win_base->backend->destroyWindow(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->updateWindow(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createCursor(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  return cur_base->backend->destroyCursor(cursor);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createImage(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_Image_Base *image_base = (MARU_Image_Base *)image;
  return image_base->backend->destroyImage(image);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->requestWindowFocus(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->requestWindowFrame) {
    return win_base->backend->requestWindowFrame(window);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->requestWindowAttention) {
    return win_base->backend->requestWindowAttention(window);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
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

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  return mon_base->backend->getMonitorModes(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  return mon_base->backend->setMonitorMode(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  mon_base->backend->resetMonitorMetrics(monitor);
}

static void *_maru_getContextNativeHandleRaw(MARU_Context *context) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->getContextNativeHandle(context);
}

static void *_maru_getWindowNativeHandleRaw(MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->getWindowNativeHandle(window);
}

MARU_API MARU_Status maru_getWaylandContextHandle(
    MARU_Context *context, MARU_WaylandContextHandle *out_handle) {
  MARU_API_VALIDATE(getWaylandContextHandle, context, out_handle);
  if (!context || !out_handle) return MARU_FAILURE;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_WAYLAND) return MARU_FAILURE;
  out_handle->display = (wl_display *)_maru_getContextNativeHandleRaw(context);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandWindowHandle(
    MARU_Window *window, MARU_WaylandWindowHandle *out_handle) {
  MARU_API_VALIDATE(getWaylandWindowHandle, window, out_handle);
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_WAYLAND) return MARU_FAILURE;

  out_handle->display = (wl_display *)_maru_getContextNativeHandleRaw(context);
  out_handle->surface = (wl_surface *)_maru_getWindowNativeHandleRaw(window);
  return (out_handle->display && out_handle->surface) ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11ContextHandle(MARU_Context *context,
                                              MARU_X11ContextHandle *out_handle) {
  MARU_API_VALIDATE(getX11ContextHandle, context, out_handle);
  if (!context || !out_handle) return MARU_FAILURE;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_X11) return MARU_FAILURE;
  out_handle->display = (Display *)_maru_getContextNativeHandleRaw(context);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11WindowHandle(MARU_Window *window,
                                             MARU_X11WindowHandle *out_handle) {
  MARU_API_VALIDATE(getX11WindowHandle, window, out_handle);
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_X11) return MARU_FAILURE;

  out_handle->display = (Display *)_maru_getContextNativeHandleRaw(context);
  out_handle->window = (Window)(uintptr_t)_maru_getWindowNativeHandleRaw(window);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

#ifdef MARU_ENABLE_BACKEND_WINDOWS
MARU_API MARU_Status maru_getWin32ContextHandle(
    MARU_Context *context, MARU_Win32ContextHandle *out_handle) {
  MARU_API_VALIDATE(getWin32ContextHandle, context, out_handle);
  if (!context || !out_handle) return MARU_FAILURE;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_WINDOWS) return MARU_FAILURE;
  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandleRaw(context);
  return out_handle->instance ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWin32WindowHandle(
    MARU_Window *window, MARU_Win32WindowHandle *out_handle) {
  MARU_API_VALIDATE(getWin32WindowHandle, window, out_handle);
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_WINDOWS) return MARU_FAILURE;

  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandleRaw(context);
  out_handle->hwnd = (HWND)_maru_getWindowNativeHandleRaw(window);
  return (out_handle->instance && out_handle->hwnd) ? MARU_SUCCESS : MARU_FAILURE;
}
#endif

#ifdef MARU_ENABLE_BACKEND_COCOA
MARU_API MARU_Status maru_getCocoaContextHandle(
    MARU_Context *context, MARU_CocoaContextHandle *out_handle) {
  MARU_API_VALIDATE(getCocoaContextHandle, context, out_handle);
  if (!context || !out_handle) return MARU_FAILURE;
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_COCOA) return MARU_FAILURE;
  out_handle->ns_application = _maru_getContextNativeHandleRaw(context);
  return out_handle->ns_application ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getCocoaWindowHandle(
    MARU_Window *window, MARU_CocoaWindowHandle *out_handle) {
  MARU_API_VALIDATE(getCocoaWindowHandle, window, out_handle);
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->pub.backend_type != MARU_BACKEND_COCOA) return MARU_FAILURE;
  out_handle->ns_window = _maru_getWindowNativeHandleRaw(window);
  return out_handle->ns_window ? MARU_SUCCESS : MARU_FAILURE;
}
#endif

MARU_API MARU_Status maru_getLinuxContextHandle(
    MARU_Context *context, MARU_LinuxContextHandle *out_handle) {
  MARU_API_VALIDATE(getLinuxContextHandle, context, out_handle);
  if (!context || !out_handle) return MARU_FAILURE;
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  out_handle->backend = ctx_base->pub.backend_type;
  switch (ctx_base->pub.backend_type) {
    case MARU_BACKEND_WAYLAND:
      return maru_getWaylandContextHandle(context, &out_handle->wayland);
    case MARU_BACKEND_X11:
      return maru_getX11ContextHandle(context, &out_handle->x11);
    default:
      return MARU_FAILURE;
  }
}

MARU_API MARU_Status maru_getLinuxWindowHandle(
    MARU_Window *window, MARU_LinuxWindowHandle *out_handle) {
  MARU_API_VALIDATE(getLinuxWindowHandle, window, out_handle);
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  out_handle->backend = ctx_base->pub.backend_type;
  switch (ctx_base->pub.backend_type) {
    case MARU_BACKEND_WAYLAND:
      return maru_getWaylandWindowHandle(window, &out_handle->wayland);
    case MARU_BACKEND_X11:
      return maru_getX11WindowHandle(window, &out_handle->x11);
    default:
      return MARU_FAILURE;
  }
}
#endif
