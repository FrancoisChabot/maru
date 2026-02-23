#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/details/contexts.h"
#include <string.h>

#include <stdlib.h>

#include "maru/c/events.h"
#include "maru/c/instrumentation.h"
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

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventType type,
                          MARU_Window *window, const MARU_Event *event) {
  if (ctx->pump_ctx && (ctx->event_mask & type)) {
    ctx->pump_ctx->callback(type, window, event, ctx->pump_ctx->userdata);
  }
}

void _maru_init_context_base(MARU_Context_Base *ctx_base) {
  memset(ctx_base->pub.extensions, 0, sizeof(ctx_base->pub.extensions));

  uint32_t capacity = ctx_base->tuning.user_event_queue_size;
  if (capacity == 0) capacity = 256;
  _maru_event_queue_init(&ctx_base->user_event_queue, ctx_base, capacity);

  ctx_base->metrics.user_events = &ctx_base->user_event_metrics;
  ctx_base->pub.metrics = &ctx_base->metrics;
}

void _maru_drain_user_events(MARU_Context_Base *ctx_base) {
  MARU_EventType type;
  MARU_Window *window;
  MARU_UserDefinedEvent user_evt;
  MARU_Event evt;

  while (_maru_event_queue_pop(&ctx_base->user_event_queue, &type, &window, &user_evt)) {
    evt.user = user_evt;
    _maru_dispatch_event(ctx_base, type, window, &evt);
  }
  
  _maru_event_queue_update_metrics(&ctx_base->user_event_queue, &ctx_base->user_event_metrics);
}

void _maru_update_context_base(MARU_Context_Base *ctx_base, uint64_t field_mask,
                               const MARU_ContextAttributes *attributes) {
  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    ctx_base->inhibit_idle = attributes->inhibit_idle;
  }

  if (field_mask & MARU_CONTEXT_ATTR_DIAGNOSTICS) {
    ctx_base->diagnostic_cb = attributes->diagnostic_cb;
    ctx_base->diagnostic_userdata = attributes->diagnostic_userdata;
  }

  if (field_mask & MARU_CONTEXT_ATTR_EVENT_MASK) {
    ctx_base->event_mask = attributes->event_mask;
  }
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
  if (ctx_base->window_cache) {
    maru_context_free(ctx_base, ctx_base->window_cache);
  }

  _maru_event_queue_cleanup(&ctx_base->user_event_queue, ctx_base);
}

void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  if (ctx_base->window_cache_count >= ctx_base->window_cache_capacity) {
    uint32_t old_cap = ctx_base->window_cache_capacity;
    uint32_t new_cap = old_cap ? old_cap * 2 : 4;
    ctx_base->window_cache = (MARU_Window **)maru_context_realloc(ctx_base, ctx_base->window_cache, old_cap * sizeof(MARU_Window *), new_cap * sizeof(MARU_Window *));
    ctx_base->window_cache_capacity = new_cap;
  }
  ctx_base->window_cache[ctx_base->window_cache_count++] = window;
}

void _maru_unregister_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  for (uint32_t i = 0; i < ctx_base->window_cache_count; i++) {
    if (ctx_base->window_cache[i] == window) {
      for (uint32_t j = i; j < ctx_base->window_cache_count - 1; j++) {
        ctx_base->window_cache[j] = ctx_base->window_cache[j + 1];
      }
      ctx_base->window_cache_count--;
      return;
    }
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

MARU_API MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                            MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(getStandardCursor, context, shape, out_cursor);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->getStandardCursor(context, shape, out_cursor);
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

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  mon_base->ref_count++;
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  mon_base->ref_count--;
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

MARU_API void *maru_getContextNativeHandle(MARU_Context *context) {
  MARU_API_VALIDATE(getContextNativeHandle, context);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->getContextNativeHandle(context);
}

MARU_API void *maru_getWindowNativeHandle(MARU_Window *window) {
  MARU_API_VALIDATE(getWindowNativeHandle, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  return win_base->backend->getWindowNativeHandle(window);
}

#endif


MARU_API MARU_Status maru_postEvent(MARU_Context *context, MARU_EventType type,
                                      MARU_Window *window, MARU_UserDefinedEvent evt) {
  MARU_API_VALIDATE(postEvent, context, type, window, evt);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

  if (_maru_event_queue_push(&ctx_base->user_event_queue, type, window, evt)) {
    return maru_wakeContext(context);
  }
  return MARU_FAILURE;
}


MARU_API MARU_Status maru_resetContextMetrics(MARU_Context *context) {
  MARU_API_VALIDATE(resetContextMetrics, context);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  memset(&ctx_base->user_event_metrics, 0, sizeof(MARU_UserEventMetrics));
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


MARU_API void *maru_contextAlloc(MARU_Context *context, size_t size) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return maru_context_alloc(ctx_base, size);
}

MARU_API void maru_contextFree(MARU_Context *context, void *ptr) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  maru_context_free(ctx_base, ptr);
}

MARU_API MARU_Status maru_registerExtension(MARU_Context *context, MARU_ExtensionID id, void *state, MARU_ExtensionCleanupCallback cleanup) {
  if (!context || id >= MARU_EXT_COUNT || !state || !cleanup) {
    return MARU_FAILURE;
  }
  MARU_ContextExposed *ctx_exp = (MARU_ContextExposed *)context;
  if (ctx_exp->extensions[id] != NULL) {
    return MARU_FAILURE;
  }
  ctx_exp->extensions[id] = state;
  (void)cleanup;
  return MARU_SUCCESS;
}

MARU_API void *maru_getExtension(const MARU_Context *context, MARU_ExtensionID id) {
  if (!context || id >= MARU_EXT_COUNT) {
    return NULL;
  }
  const MARU_ContextExposed *ctx_exp = (const MARU_ContextExposed *)context;
  return ctx_exp->extensions[id];
}
