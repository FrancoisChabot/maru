#include "maru_internal.h"
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

MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_X11));
  if (!ctx) {
    return MARU_FAILURE;
  }

  ctx->base.pub.backend_type = MARU_BACKEND_X11;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }
  ctx->base.tuning = create_info->tuning;
  _maru_init_context_base(&ctx->base);
#ifdef MARU_GATHER_METRICS
  _maru_update_mem_metrics_alloc(&ctx->base, sizeof(MARU_Context_X11));
#endif

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }
  
  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_mask = create_info->attributes.event_mask;
  ctx->base.inhibit_idle = create_info->attributes.inhibit_idle;

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_X11;
  ctx->base.backend = &maru_backend_X11;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_linux_common_cleanup(&ctx->linux_common);
  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  (void)callback;
  (void)userdata;

  // 1. Drain hotplug events from worker thread
  _maru_linux_common_drain_internal_events(&ctx->linux_common);

  // 2. Prepare poll set
  // In a real X11 implementation, we would get the connection FD here.
  // For now, we only have controllers and the wake_fd.
  struct pollfd pfds[32];
  uint32_t nfds = 0;

  // Slot 0: Wake FD
  pfds[nfds].fd = ctx->linux_common.worker.event_fd;
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;
  nfds++;

  // Controller FDs
  uint32_t ctrl_count = _maru_linux_common_fill_pollfds(&ctx->linux_common, &pfds[nfds], 31);
  uint32_t ctrl_start_idx = nfds;
  nfds += ctrl_count;

  int poll_timeout = (timeout_ms == MARU_NEVER) ? -1 : (int)timeout_ms;
  int ret = poll(pfds, nfds, poll_timeout);

  if (ret > 0) {
    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      read(ctx->linux_common.worker.event_fd, &val, sizeof(val));
    }

    if (ctrl_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common, &pfds[ctrl_start_idx], ctrl_count);
    }
  }

  // Final drain in case poll returned due to worker activity
  _maru_linux_common_drain_internal_events(&ctx->linux_common);

  return MARU_SUCCESS;
}

MARU_Status maru_createWindow_X11(MARU_Context *context,
                                 const MARU_WindowCreateInfo *create_info,
                                 MARU_Window **out_window) {
  (void)context;
  (void)create_info;
  (void)out_window;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_X11(MARU_Window *window) {
  (void)window;
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_X11(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

static void *maru_getContextNativeHandle_X11(MARU_Context *context) {
  (void)context;
  return NULL;
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
  for (MARU_LinuxController *it = common->controllers; it; it = it->next) {
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

static void *maru_getWindowNativeHandle_X11(MARU_Window *window) {
  (void)window;
  return NULL;
}

#ifdef MARU_INDIRECT_BACKEND
static MARU_Status maru_createImage_X11(MARU_Context *context,
                                        const MARU_ImageCreateInfo *create_info,
                                        MARU_Image **out_image) {
  (void)context;
  (void)create_info;
  (void)out_image;
  return MARU_FAILURE;
}

static MARU_Status maru_destroyImage_X11(MARU_Image *image) {
  (void)image;
  return MARU_FAILURE;
}

const MARU_Backend maru_backend_X11 = {
  .destroyContext = maru_destroyContext_X11,
  .pumpEvents = maru_pumpEvents_X11,
  .createWindow = maru_createWindow_X11,
  .destroyWindow = maru_destroyWindow_X11,
  .getWindowGeometry = NULL,
  .updateWindow = NULL,
  .requestWindowFocus = NULL,
  .requestWindowAttention = maru_requestWindowAttention_X11,
  .createCursor = NULL,
  .destroyCursor = NULL,
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
  .wakeContext = NULL,
  .getMonitorModes = NULL,
  .setMonitorMode = NULL,
  .resetMonitorMetrics = NULL,
  .getContextNativeHandle = maru_getContextNativeHandle_X11,
  .getWindowNativeHandle = maru_getWindowNativeHandle_X11,
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
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;

  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  (void)context;
  (void)create_info;
  (void)out_image;
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  (void)image;
  return MARU_FAILURE; // Not implemented in X11 yet
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
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_X11(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return MARU_FAILURE;
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  *out_count = 0;
  return NULL;
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
      return;
    }
  }
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  *out_count = 0;
  return NULL;
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return MARU_FAILURE;
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
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
