#include "maru_internal.h"
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

extern MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context);
extern MARU_Status maru_destroyContext_X11(MARU_Context *context);
extern MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                    const MARU_ContextAttributes *attributes);
extern MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, 
                                        MARU_EventCallback callback, void *userdata);
extern MARU_Status maru_wakeContext_X11(MARU_Context *context);
extern void *maru_getContextNativeHandle_X11(MARU_Context *context);
extern void *maru_getWindowNativeHandle_X11(MARU_Window *window);

extern MARU_Status maru_createWindow_X11(MARU_Context *context,
                                 const MARU_WindowCreateInfo *create_info,
                                 MARU_Window **out_window);
extern MARU_Status maru_destroyWindow_X11(MARU_Window *window);
extern void maru_getWindowGeometry_X11(MARU_Window *window,
                               MARU_WindowGeometry *out_geometry);

extern MARU_Status maru_createCursor_X11(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor);
extern MARU_Status maru_destroyCursor_X11(MARU_Cursor *cursor);
extern MARU_Status maru_createImage_X11(MARU_Context *context,
                                        const MARU_ImageCreateInfo *create_info,
                                        MARU_Image **out_image);
extern MARU_Status maru_destroyImage_X11(MARU_Image *image);

extern MARU_Monitor *const *maru_getMonitors_X11(MARU_Context *context, uint32_t *out_count);
extern void maru_retainMonitor_X11(MARU_Monitor *monitor);
extern void maru_releaseMonitor_X11(MARU_Monitor *monitor);
extern const MARU_VideoMode *maru_getMonitorModes_X11(const MARU_Monitor *monitor, uint32_t *out_count);
extern MARU_Status maru_setMonitorMode_X11(const MARU_Monitor *monitor, MARU_VideoMode mode);
extern void maru_resetMonitorMetrics_X11(MARU_Monitor *monitor);

MARU_Status maru_updateWindow_X11(MARU_Window *window, uint64_t field_mask,
                                  const MARU_WindowAttributes *attributes) {
    (void)window;
    (void)field_mask;
    (void)attributes;
    return MARU_FAILURE;
}

MARU_Status maru_requestWindowFocus_X11(MARU_Window *window) {
    (void)window;
    return MARU_FAILURE;
}

MARU_Status maru_requestWindowFrame_X11(MARU_Window *window) {
    (void)window;
    return MARU_FAILURE;
}

MARU_Status maru_requestWindowAttention_X11(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

extern const char **maru_getVkExtensions_X11(const MARU_Context *context,
                                             uint32_t *out_count);
extern MARU_Status maru_createVkSurface_X11(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

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

static MARU_Status maru_retainController_X11(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
  return MARU_SUCCESS;
}

static MARU_Status maru_releaseController_X11(MARU_Controller *controller) {
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
  return MARU_SUCCESS;
}

static MARU_Status maru_resetControllerMetrics_X11(MARU_Controller *controller) {
  (void)controller;
  return MARU_SUCCESS;
}

static MARU_Status maru_getControllerInfo_X11(MARU_Controller *controller,
                                              MARU_ControllerInfo *out_info) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  memset(out_info, 0, sizeof(*out_info));
  out_info->name = ctrl->name;
  out_info->vendor_id = ctrl->vendor_id;
  out_info->product_id = ctrl->product_id;
  out_info->version = ctrl->version;
  memcpy(out_info->guid, ctrl->guid, 16);
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

static MARU_Status maru_announceData_X11(MARU_Window *window,
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
maru_provideData_X11(const MARU_DataRequestEvent *request_event, const void *data,
                     size_t size, MARU_DataProvideFlags flags) {
  (void)request_event;
  (void)data;
  (void)size;
  (void)flags;
  return MARU_FAILURE;
}

static MARU_Status maru_requestData_X11(MARU_Window *window,
                                        MARU_DataExchangeTarget target,
                                        const char *mime_type, void *user_tag) {
  (void)window;
  (void)target;
  (void)mime_type;
  (void)user_tag;
  return MARU_FAILURE;
}

static MARU_Status maru_getAvailableMIMETypes_X11(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
  (void)window;
  (void)target;
  out_list->mime_types = NULL;
  out_list->count = 0;
  return MARU_FAILURE;
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
  .getWindowGeometry = maru_getWindowGeometry_X11,
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
  return maru_updateContext_X11(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_X11(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_X11(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_X11(window);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_X11(window, out_geometry);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_X11(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_X11(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_X11(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_X11(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_X11(image);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  return maru_getControllers_X11(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  return maru_retainController_X11(controller);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  return maru_releaseController_X11(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_X11(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_X11(controller, out_info);
}

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  return maru_setControllerHapticLevels_X11(controller, first_haptic, count,
                                            intensities);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char **mime_types, uint32_t count,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count,
                    allowed_actions);
  return maru_announceData_X11(window, target, mime_types, count,
                               allowed_actions);
}

MARU_API MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  return maru_provideData_X11(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  return maru_requestData_X11(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_X11(window, target, out_list);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_X11(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  return maru_requestWindowFrame_X11(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_X11(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_X11(context);
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  return maru_getMonitors_X11(context, out_count);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_X11(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_X11(monitor);
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_X11(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_X11(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  maru_resetMonitorMetrics_X11(monitor);
}

MARU_API MARU_Status maru_getX11ContextHandle(MARU_Context *context,
                                              MARU_X11ContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->display = (Display *)maru_getContextNativeHandle_X11(context);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11WindowHandle(MARU_Window *window,
                                             MARU_X11WindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  out_handle->display = (Display *)maru_getContextNativeHandle_X11(context);
  out_handle->window = (Window)(uintptr_t)maru_getWindowNativeHandle_X11(window);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandContextHandle(
    MARU_Context *context, MARU_WaylandContextHandle *out_handle) {
  (void)context;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandWindowHandle(
    MARU_Window *window, MARU_WaylandWindowHandle *out_handle) {
  (void)window;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  out_handle->surface = NULL;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getLinuxContextHandle(
    MARU_Context *context, MARU_LinuxContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_X11;
  return maru_getX11ContextHandle(context, &out_handle->x11);
}

MARU_API MARU_Status maru_getLinuxWindowHandle(
    MARU_Window *window, MARU_LinuxWindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_X11;
  return maru_getX11WindowHandle(window, &out_handle->x11);
}
#endif
