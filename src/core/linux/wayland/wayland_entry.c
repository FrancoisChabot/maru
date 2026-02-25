#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/cursors.h"
#include "maru/c/monitors.h"
#include "maru/c/native/linux.h"
#include <stdlib.h>
#include <string.h>

extern void *maru_getContextNativeHandle_WL(MARU_Context *context);
extern void *maru_getWindowNativeHandle_WL(MARU_Window *window);
extern const char **maru_getVkExtensions_WL(const MARU_Context *context,
                                            uint32_t *out_count);
extern MARU_Status maru_createVkSurface_WL(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

static MARU_Status maru_announceData_WL(MARU_Window *window,
                                        MARU_DataExchangeTarget target,
                                        const char **mime_types, uint32_t count,
                                        MARU_DropActionMask allowed_actions) {
  (void)window;
  (void)target;
  (void)mime_types;
  (void)count;
  (void)allowed_actions;
  return MARU_FAILURE;
}

static MARU_Status
maru_provideData_WL(const MARU_DataRequestEvent *request_event, const void *data,
                    size_t size, MARU_DataProvideFlags flags) {
  (void)request_event;
  (void)data;
  (void)size;
  (void)flags;
  return MARU_FAILURE;
}

static MARU_Status maru_requestData_WL(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char *mime_type, void *user_tag) {
  (void)window;
  (void)target;
  (void)mime_type;
  (void)user_tag;
  return MARU_FAILURE;
}

static MARU_Status maru_dataexchange_enable_WL(MARU_Context *context) {
  (void)context;
  return MARU_FAILURE;
}

static MARU_Status maru_getAvailableMIMETypes_WL(MARU_Window *window,
                                                 MARU_DataExchangeTarget target,
                                                 MARU_MIMETypeList *out_list) {
  (void)window;
  (void)target;
  out_list->mime_types = NULL;
  out_list->count = 0;
  return MARU_FAILURE;
}

#ifdef MARU_INDIRECT_BACKEND
static void maru_resetMonitorMetrics_WL(MARU_Monitor *monitor) {
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
}

const MARU_Backend maru_backend_WL = {
  .destroyContext = maru_destroyContext_WL,
  .updateContext = maru_updateContext_WL,
  .pumpEvents = maru_pumpEvents_WL,
  .createWindow = maru_createWindow_WL,
  .destroyWindow = maru_destroyWindow_WL,
  .getWindowGeometry = maru_getWindowGeometry_WL,
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
  .resetControllerMetrics = maru_resetControllerMetrics_WL,
  .getControllerInfo = maru_getControllerInfo_WL,
  .setControllerHapticLevels = maru_setControllerHapticLevels_WL,
  .announceData = maru_announceData_WL,
  .provideData = maru_provideData_WL,
  .requestData = maru_requestData_WL,
  .dataexchangeEnable = maru_dataexchange_enable_WL,
  .getAvailableMIMETypes = maru_getAvailableMIMETypes_WL,
  .wakeContext = maru_wakeContext_WL,
  .getMonitors = maru_getMonitors_WL,
  .retainMonitor = maru_retainMonitor_WL,
  .releaseMonitor = maru_releaseMonitor_WL,
  .getMonitorModes = maru_getMonitorModes_WL,
  .setMonitorMode = maru_setMonitorMode_WL,
  .resetMonitorMetrics = maru_resetMonitorMetrics_WL,
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

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_WL(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  return maru_updateContext_WL(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_WL(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_WL(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_WL(window);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_WL(window, out_geometry);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_WL(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                        const MARU_CursorCreateInfo *create_info,
                                        MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_WL(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_WL(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_WL(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_WL(image);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  return maru_getControllers_WL(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  return maru_retainController_WL(controller);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  return maru_releaseController_WL(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_WL(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_WL(controller, out_info);
}

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  return maru_setControllerHapticLevels_WL(controller, first_haptic, count,
                                           intensities);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char **mime_types, uint32_t count,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count,
                    allowed_actions);
  return maru_announceData_WL(window, target, mime_types, count,
                              allowed_actions);
}

MARU_API MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  return maru_provideData_WL(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  return maru_requestData_WL(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_dataexchange_enable(MARU_Context *context) {
  MARU_API_VALIDATE(dataexchange_enable, context);
  return maru_dataexchange_enable_WL(context);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_WL(window, target, out_list);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_WL(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  return maru_requestWindowFrame_WL(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_WL(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_WL(context);
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  if (maru_updateMonitors_WL(context) != MARU_SUCCESS) {
    *out_count = 0;
    return NULL;
  }
  return maru_getMonitors_WL(context, out_count);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_WL(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_WL(monitor);
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_WL(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_WL(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
}

MARU_API MARU_Status maru_getWaylandContextHandle(
    MARU_Context *context, MARU_WaylandContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->display = (wl_display *)maru_getContextNativeHandle_WL(context);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandWindowHandle(
    MARU_Window *window, MARU_WaylandWindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  out_handle->display = (wl_display *)maru_getContextNativeHandle_WL(context);
  out_handle->surface = (wl_surface *)maru_getWindowNativeHandle_WL(window);
  return (out_handle->display && out_handle->surface) ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11ContextHandle(MARU_Context *context,
                                              MARU_X11ContextHandle *out_handle) {
  (void)context;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11WindowHandle(MARU_Window *window,
                                             MARU_X11WindowHandle *out_handle) {
  (void)window;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  out_handle->window = 0;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getLinuxContextHandle(
    MARU_Context *context, MARU_LinuxContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_WAYLAND;
  return maru_getWaylandContextHandle(context, &out_handle->wayland);
}

MARU_API MARU_Status maru_getLinuxWindowHandle(
    MARU_Window *window, MARU_LinuxWindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_WAYLAND;
  return maru_getWaylandWindowHandle(window, &out_handle->wayland);
}
#endif
