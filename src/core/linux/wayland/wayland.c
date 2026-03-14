#include "wayland_internal.h"
#include "../linux_native_stubs.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/maru.h"
#include <stdlib.h>
#include <string.h>

MARU_Status maru_getVkExtensions_WL(const MARU_Context *context,
                                   MARU_StringList *out_list);
extern MARU_Status maru_createVkSurface_WL(
    const MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

void *maru_getContextNativeHandle_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return ctx->wl.display;
}

void *maru_getWindowNativeHandle_WL(MARU_Window *window) {
  MARU_Window_WL *window_wl = (MARU_Window_WL *)window;
  return window_wl->wl.surface;
}

static MARU_Window_WL *maru_getClipboardWindow_WL(const MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *win = (MARU_Window_WL *)it;
    if (win->wl.surface != NULL) {
      return win;
    }
  }
  return NULL;
}

static MARU_Status maru_announceClipboardData_WL_ctx(MARU_Context *context,
                                                     MARU_StringList mime_types) {
  MARU_Window_WL *win = maru_getClipboardWindow_WL(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_announceData_WL((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                              mime_types, 0);
}

static MARU_Status maru_requestClipboardData_WL_ctx(MARU_Context *context,
                                                    const char *mime_type,
                                                    void *userdata) {
  MARU_Window_WL *win = maru_getClipboardWindow_WL(context);
  if (!win) {
    return MARU_FAILURE;
  }
  return maru_requestData_WL((MARU_Window *)win, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                             mime_type, userdata);
}

static MARU_Status
maru_getAvailableClipboardMIMETypes_WL_ctx(const MARU_Context *context,
                                           MARU_StringList *out_list) {
  MARU_Window_WL *win = maru_getClipboardWindow_WL(context);
  if (!win) {
    out_list->strings = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  return maru_getAvailableMIMETypes_WL((MARU_Window *)win,
                                       MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                       out_list);
}

static MARU_Status maru_announceDragData_WL_win(MARU_Window *window,
                                                MARU_StringList mime_types,
                                                MARU_DropActionMask allowed_actions) {
  return maru_announceData_WL(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                              mime_types, allowed_actions);
}

static MARU_Status maru_requestDropData_WL_win(MARU_Window *window,
                                               const char *mime_type,
                                               void *userdata) {
  return maru_requestData_WL(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                             mime_type, userdata);
}

static MARU_Status
maru_getAvailableDropMIMETypes_WL_win(const MARU_Window *window,
                                      MARU_StringList *out_list) {
  return maru_getAvailableMIMETypes_WL(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                                       out_list);
}


#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_WL = {
  .destroyContext = maru_destroyContext_WL,
  .updateContext = maru_updateContext_WL,
  .pumpEvents = maru_pumpEvents_WL,
  .createWindow = maru_createWindow_WL,
  .destroyWindow = maru_destroyWindow_WL,
  .updateWindow = maru_updateWindow_WL,
  .requestWindowFocus = maru_requestWindowFocus_WL,
  .requestWindowFrame = maru_requestWindowFrame_WL,
  .requestWindowAttention = maru_requestWindowAttention_WL,
  .createCursor = maru_createCursor_WL,
  .destroyCursor = maru_destroyCursor_WL,
  .createImage = maru_createImage_WL,
  .destroyImage = maru_destroyImage_WL,
  .getControllers = maru_getControllers_WL,
  .retainController = maru_retainController_WL,
  .releaseController = maru_releaseController_WL,
  .setControllerHapticLevels = maru_setControllerHapticLevels_WL,
  .announceClipboardData = maru_announceClipboardData_WL_ctx,
  .announceDragData = maru_announceDragData_WL_win,
  .provideData = maru_provideData_WL,
  .requestClipboardData = maru_requestClipboardData_WL_ctx,
  .requestDropData = maru_requestDropData_WL_win,
  .getAvailableClipboardMIMETypes = maru_getAvailableClipboardMIMETypes_WL_ctx,
  .getAvailableDropMIMETypes = maru_getAvailableDropMIMETypes_WL_win,
  .wakeContext = maru_wakeContext_WL,
  .getMonitors = maru_getMonitors_WL,
  .retainMonitor = maru_retainMonitor_WL,
  .releaseMonitor = maru_releaseMonitor_WL,
  .getMonitorModes = maru_getMonitorModes_WL,
  .setMonitorMode = maru_setMonitorMode_WL,
  .getContextNativeHandle = maru_getContextNativeHandle_WL,
  .getWindowNativeHandle = maru_getWindowNativeHandle_WL,
  .getVkExtensions = maru_getVkExtensions_WL,
  .createVkSurface = maru_createVkSurface_WL,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_WL(create_info, out_context);
}

MARU_API void maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  maru_destroyContext_WL(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_updateContext_WL(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, mask, callback, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_pumpEvents_WL(context, timeout_ms, mask, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createWindow_WL(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  return maru_destroyWindow_WL(window);
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
  return maru_updateWindow_WL(window, field_mask, attrs_arg);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                        const MARU_CursorCreateInfo *create_info,
                                        MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createCursor_WL(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_RETURN_ON_ERROR(_maru_status_if_cursor_context_lost(cursor));
  return maru_destroyCursor_WL(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_createImage_WL(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  MARU_RETURN_ON_ERROR(_maru_status_if_image_context_lost(image));
  return maru_destroyImage_WL(image);
}

MARU_API MARU_Status maru_getControllers(const MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_getControllers_WL(context, out_list);
}

MARU_API void maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  maru_retainController_WL(controller);
}

MARU_API void maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  maru_releaseController_WL(controller);
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
  return maru_setControllerHapticLevels_WL(controller, first_haptic, count,
                                           intensities);
}

MARU_API MARU_Status maru_announceClipboardData(MARU_Context *context,
                                                MARU_StringList mime_types) {
  MARU_API_VALIDATE(announceClipboardData, context, mime_types);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(announceClipboardData, context, mime_types);
  return maru_announceClipboardData_WL_ctx(context, mime_types);
}

MARU_API MARU_Status maru_announceDragData(MARU_Window *window,
                                           MARU_StringList mime_types,
                                           MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceDragData, window, mime_types, allowed_actions);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(announceDragData, window, mime_types,
                         allowed_actions);
  return maru_announceDragData_WL_win(window, mime_types, allowed_actions);
}

MARU_API MARU_Status maru_provideClipboardData(MARU_DataRequest *request,
                                               const void *data, size_t size,
                                               MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideClipboardData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_WL(request, data, size, flags);
}

MARU_API MARU_Status maru_provideDropData(MARU_DataRequest *request,
                                          const void *data, size_t size,
                                          MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideDropData, request, data, size, flags);
  MARU_RETURN_ON_ERROR(_maru_status_if_request_context_lost(request));
  return maru_provideData_WL(request, data, size, flags);
}

MARU_API MARU_Status maru_requestClipboardData(MARU_Context *context,
                                               const char *mime_type,
                                               void *userdata) {
  MARU_API_VALIDATE(requestClipboardData, context, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(requestClipboardData, context, mime_type, userdata);
  return maru_requestClipboardData_WL_ctx(context, mime_type, userdata);
}

MARU_API MARU_Status maru_requestDropData(MARU_Window *window,
                                          const char *mime_type,
                                          void *userdata) {
  MARU_API_VALIDATE(requestDropData, window, mime_type, userdata);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestDropData, window, mime_type, userdata);
  return maru_requestDropData_WL_win(window, mime_type, userdata);
}

MARU_API MARU_Status maru_getAvailableClipboardMIMETypes(
    const MARU_Context *context, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableClipboardMIMETypes, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_API_VALIDATE_LIVE(getAvailableClipboardMIMETypes, context, out_list);
  return maru_getAvailableClipboardMIMETypes_WL_ctx(context, out_list);
}

MARU_API MARU_Status
maru_getAvailableDropMIMETypes(const MARU_Window *window, MARU_StringList *out_list) {
  MARU_API_VALIDATE(getAvailableDropMIMETypes, window, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(getAvailableDropMIMETypes, window, out_list);
  return maru_getAvailableDropMIMETypes_WL_win(window, out_list);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFocus, window);
  return maru_requestWindowFocus_WL(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowFrame, window);
  return maru_requestWindowFrame_WL(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(requestWindowAttention, window);
  return maru_requestWindowAttention_WL(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_WL(context);
}

MARU_API MARU_Status maru_getMonitors(const MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_Status res = maru_updateMonitors_WL(context);
  if (res != MARU_SUCCESS) {
    out_list->monitors = NULL;
    out_list->count = 0;
    return res;
  }
  return maru_getMonitors_WL(context, out_list);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_WL(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_WL(monitor);
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_API_VALIDATE_LIVE(getMonitorModes, monitor, out_list);
  return maru_getMonitorModes_WL(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_context_lost(monitor));
  MARU_RETURN_ON_ERROR(_maru_status_if_monitor_lost(monitor));
  MARU_API_VALIDATE_LIVE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_WL(monitor, mode);
}

MARU_API MARU_WaylandContextHandle
maru_getWaylandContextHandle(const MARU_Context *context) {
  MARU_API_VALIDATE(getWaylandContextHandle, context);
  MARU_WaylandContextHandle handle = {0};
  handle.display =
      (wl_display *)maru_getContextNativeHandle_WL((MARU_Context *)context);
  return handle;
}

MARU_API MARU_WaylandWindowHandle
maru_getWaylandWindowHandle(const MARU_Window *window) {
  MARU_API_VALIDATE(getWaylandWindowHandle, window);
  MARU_WaylandWindowHandle handle = {0};
  const MARU_Context *context = maru_getWindowContext(window);
  handle.display = (wl_display *)maru_getContextNativeHandle_WL((MARU_Context *)context);
  handle.surface =
      (wl_surface *)maru_getWindowNativeHandle_WL((MARU_Window *)window);
  return handle;
}

MARU_API MARU_X11ContextHandle
maru_getX11ContextHandle(const MARU_Context *context) {
  MARU_API_VALIDATE(getX11ContextHandle, context);
  (void)context;
  return _maru_stub_x11_context_handle_unsupported();
}

MARU_API MARU_X11WindowHandle
maru_getX11WindowHandle(const MARU_Window *window) {
  MARU_API_VALIDATE(getX11WindowHandle, window);
  (void)window;
  return _maru_stub_x11_window_handle_unsupported();
}

#endif
