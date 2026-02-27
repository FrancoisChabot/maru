#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/details/contexts.h"
#include "maru/c/details/controllers.h"
#include <string.h>
#include <stdatomic.h>

#include <stdlib.h>

#include "maru/c/events.h"
#include "maru/c/instrumentation.h"
#include "maru/c/native/cocoa.h"
#include "maru/c/native/linux.h"
#include "maru/c/native/wayland.h"
#include "maru/c/native/win32.h"
#include "maru/c/native/x11.h"
#ifdef __linux__
#include "linux/linux_internal.h"
typedef struct MARU_Context_Linux {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;
} MARU_Context_Linux;
#endif
#include "core_event_queue.h"

#ifdef MARU_VALIDATE_API_CALLS
#include <pthread.h>
#include <stdio.h>

MARU_ThreadId _maru_getCurrentThreadId(void) {
  return (MARU_ThreadId)pthread_self();
}
#endif

#ifdef MARU_ENABLE_DIAGNOSTICS
#ifdef MARU_VALIDATE_API_CALLS
void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag,
                            const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {.diagnostic = diag,
                                .message = msg,
                                .context = (MARU_Context *)ctx,
                                .window = NULL};
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  }
}
#else
void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag,
                            const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {.diagnostic = diag,
                                .message = msg,
                                .context = (MARU_Context *)ctx,
                                .window = NULL};
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  }
}
#endif
#endif

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventId type,
                          MARU_Window *window, const MARU_Event *event) {
  const MARU_EventMask event_bit = MARU_EVENT_MASK(type);
  if (!ctx->pump_ctx) return;
  if ((ctx->event_mask & event_bit) == 0) return;
  if (window && (maru_getWindowEventMask(window) & event_bit) == 0) return;
  if (window &&
      (type == MARU_EVENT_DROP_ENTERED || type == MARU_EVENT_DROP_HOVERED ||
       type == MARU_EVENT_DROP_EXITED || type == MARU_EVENT_DROP_DROPPED)) {
    const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
    if (!win_base->attrs_effective.accept_drop) return;
  }

  ctx->pump_ctx->callback(type, window, event, ctx->pump_ctx->userdata);
}

void _maru_init_context_base(MARU_Context_Base *ctx_base) {
  ctx_base->pub.userdata = NULL;
  ctx_base->mouse_button_states = NULL;
  ctx_base->mouse_button_channels = NULL;
  ctx_base->pub.mouse_button_state = NULL;
  ctx_base->pub.mouse_button_channels = NULL;
  ctx_base->pub.mouse_button_count = 0;
  for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
    ctx_base->pub.mouse_default_button_channels[i] = -1;
  }

  uint32_t capacity = ctx_base->tuning.user_event_queue_size;
  if (capacity == 0) capacity = 256;
  if ((capacity & (capacity - 1u)) != 0u) {
    uint32_t rounded = 1u;
    while (rounded < capacity && rounded < 0x80000000u) {
      rounded <<= 1u;
    }
    capacity = rounded;
  }
  if (!_maru_event_queue_init(&ctx_base->queued_events, ctx_base, capacity)) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY,
                           "Failed to initialize internal event queue");
  }

  ctx_base->metrics.user_events = &ctx_base->user_event_metrics;
  ctx_base->pub.metrics = &ctx_base->metrics;
}

void _maru_drain_queued_events(MARU_Context_Base *ctx_base) {
  MARU_EventId type;
  MARU_Window *window;
  MARU_Event evt;

  while (_maru_event_queue_pop(&ctx_base->queued_events, &type, &window, &evt)) {
#ifdef __linux__
    if (type == (MARU_EventId)MARU_EVENT_INTERNAL_LINUX_DEVICE_ADD ||
        type == (MARU_EventId)MARU_EVENT_INTERNAL_LINUX_DEVICE_REMOVE) {
      if (ctx_base->pub.backend_type == MARU_BACKEND_WAYLAND || ctx_base->pub.backend_type == MARU_BACKEND_X11) {
        MARU_Context_Linux *ctx_linux = (MARU_Context_Linux *)ctx_base;
        _maru_linux_common_handle_internal_event(&ctx_linux->linux_common, (MARU_InternalEventId)type, window, &evt);
      }
      continue;
    }
#endif
    _maru_dispatch_event(ctx_base, type, window, &evt);
  }
  
  _maru_event_queue_update_metrics(&ctx_base->queued_events, &ctx_base->user_event_metrics);
}

void _maru_update_context_base(MARU_Context_Base *ctx_base, uint64_t field_mask,
                               const MARU_ContextAttributes *attributes) {
  ctx_base->attrs_dirty_mask |= field_mask;

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    ctx_base->attrs_requested.inhibit_idle = attributes->inhibit_idle;
    ctx_base->attrs_effective.inhibit_idle = attributes->inhibit_idle;
    ctx_base->inhibit_idle = ctx_base->attrs_effective.inhibit_idle;
  }

  if (field_mask & MARU_CONTEXT_ATTR_DIAGNOSTICS) {
    ctx_base->attrs_requested.diagnostic_cb = attributes->diagnostic_cb;
    ctx_base->attrs_requested.diagnostic_userdata = attributes->diagnostic_userdata;
    ctx_base->attrs_effective.diagnostic_cb = attributes->diagnostic_cb;
    ctx_base->attrs_effective.diagnostic_userdata = attributes->diagnostic_userdata;
    ctx_base->diagnostic_cb = ctx_base->attrs_effective.diagnostic_cb;
    ctx_base->diagnostic_userdata = ctx_base->attrs_effective.diagnostic_userdata;
  }

  if (field_mask & MARU_CONTEXT_ATTR_EVENT_MASK) {
    ctx_base->attrs_requested.event_mask = attributes->event_mask;
    ctx_base->attrs_effective.event_mask = attributes->event_mask;
    ctx_base->event_mask = ctx_base->attrs_effective.event_mask;
  }

  if (field_mask & MARU_CONTEXT_ATTR_IDLE_TIMEOUT) {
    ctx_base->attrs_requested.idle_timeout_ms = attributes->idle_timeout_ms;
    ctx_base->attrs_effective.idle_timeout_ms = attributes->idle_timeout_ms;
    ctx_base->tuning.idle_timeout_ms = ctx_base->attrs_effective.idle_timeout_ms;
  }

  ctx_base->attrs_dirty_mask &= ~field_mask;
}

void _maru_cleanup_context_base(MARU_Context_Base *ctx_base) {
  for (uint32_t i = 0; i < ctx_base->monitor_cache_count; i++) {
    MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)ctx_base->monitor_cache[i];
    mon_base->is_active = false;
    maru_releaseMonitor((MARU_Monitor *)mon_base);
  }
  if (ctx_base->monitor_cache) {
    maru_context_free(ctx_base, ctx_base->monitor_cache);
  }
  if (ctx_base->mouse_button_states) {
    maru_context_free(ctx_base, ctx_base->mouse_button_states);
    ctx_base->mouse_button_states = NULL;
  }
  if (ctx_base->mouse_button_channels) {
    maru_context_free(ctx_base, ctx_base->mouse_button_channels);
    ctx_base->mouse_button_channels = NULL;
  }
  _maru_event_queue_cleanup(&ctx_base->queued_events, ctx_base);
}

void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  win_base->ctx_prev = NULL;
  win_base->ctx_next = ctx_base->window_list_head;
  if (ctx_base->window_list_head) {
    ctx_base->window_list_head->ctx_prev = win_base;
  }
  ctx_base->window_list_head = win_base;
  ctx_base->window_count++;
}

void _maru_unregister_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->ctx_prev) {
    win_base->ctx_prev->ctx_next = win_base->ctx_next;
  } else if (ctx_base->window_list_head == win_base) {
    ctx_base->window_list_head = win_base->ctx_next;
  }
  if (win_base->ctx_next) {
    win_base->ctx_next->ctx_prev = win_base->ctx_prev;
  }

  win_base->ctx_prev = NULL;
  win_base->ctx_next = NULL;
  if (ctx_base->window_count > 0) {
    ctx_base->window_count--;
  }
}


void *_maru_default_alloc(size_t size, void *userdata) {
  (void)userdata;
  return malloc(size);
}

void *_maru_default_realloc(void *ptr, size_t new_size, void *userdata) {
  (void)userdata;
  return realloc(ptr, new_size);
}

void _maru_default_free(void *ptr, void *userdata) {
  (void)userdata;
  free(ptr);
}

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

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->pumpEvents(context, timeout_ms, callback, userdata);
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
#ifdef MARU_INDIRECT_BACKEND
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request_event->internal_handle;
  if (!handle || !handle->ctx_base || !handle->ctx_base->backend ||
      !handle->ctx_base->backend->provideData) {
    return MARU_FAILURE;
  }
  return handle->ctx_base->backend->provideData(request_event, data, size, flags);
#else
  (void)request_event;
  (void)data;
  (void)size;
  (void)flags;
  return MARU_FAILURE;
#endif
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

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                            MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  win_base->backend->getWindowGeometry(window, out_geometry);
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

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->getMonitors(context, out_count);
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


MARU_Status _maru_post_event_internal(MARU_Context_Base *ctx_base, MARU_EventId type,
                                      MARU_Window *window, const MARU_Event *evt) {
  if (_maru_event_queue_push(&ctx_base->queued_events, type, window, *evt)) {
    return maru_wakeContext((MARU_Context *)ctx_base);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_postEvent(MARU_Context *context, MARU_EventId type,
                                      MARU_Window *window, MARU_UserDefinedEvent user_evt) {
  MARU_API_VALIDATE(postEvent, context, type, window, user_evt);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  MARU_Event evt;
  evt.user = user_evt;

  return _maru_post_event_internal(ctx_base, type, window, &evt);
}


MARU_API MARU_Status maru_resetContextMetrics(MARU_Context *context) {
  MARU_API_VALIDATE(resetContextMetrics, context);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  memset(&ctx_base->user_event_metrics, 0, sizeof(MARU_UserEventMetrics));
  ctx_base->metrics.cursor_create_count_total = 0;
  ctx_base->metrics.cursor_destroy_count_total = 0;
  ctx_base->metrics.cursor_create_count_system = 0;
  ctx_base->metrics.cursor_create_count_custom = 0;
  ctx_base->metrics.cursor_alive_peak = ctx_base->metrics.cursor_alive_current;
  ctx_base->metrics.pump_call_count_total = 0;
  ctx_base->metrics.pump_duration_avg_ns = 0;
  ctx_base->metrics.pump_duration_peak_ns = 0;
  ctx_base->metrics.memory_allocated_peak = ctx_base->metrics.memory_allocated_current;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_resetWindowMetrics(MARU_Window *window) {
  MARU_API_VALIDATE(resetWindowMetrics, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  memset(&win_base->metrics, 0, sizeof(MARU_WindowMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Version maru_getVersion(void) {
  return (MARU_Version){.major = MARU_VERSION_MAJOR,
                        .minor = MARU_VERSION_MINOR,
                        .patch = MARU_VERSION_PATCH};
}
