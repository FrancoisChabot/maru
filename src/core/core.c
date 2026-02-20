#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include <string.h>

#include <stdlib.h>

#include "maru/c/events.h"
#include "maru/c/instrumentation.h"

#ifdef MARU_VALIDATE_API_CALLS
#include <pthread.h>
#include <stdio.h>

MARU_ThreadId _maru_getCurrentThreadId(void) {
  return (MARU_ThreadId)pthread_self();
}

void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag,
                            const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {.diagnostic = diag,
                                .message = msg,
                                .context = (MARU_Context *)ctx,
                                .window = NULL};
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  } else {
    fprintf(stderr, "[MARU DIAGNOSTIC %d] %s\n", (int)diag, msg);
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

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventType type,
                          MARU_Window *window, const MARU_Event *event) {
  if (ctx->event_cb && (ctx->event_mask & type)) {
    ctx->event_cb(type, window, event);
  }
}

void _maru_monitor_free(MARU_Monitor_Base *monitor) {
#ifdef MARU_INDIRECT_BACKEND
  if (monitor->backend->destroyMonitor) {
    monitor->backend->destroyMonitor((MARU_Monitor *)monitor);
  }
#endif
  maru_context_free(monitor->ctx_base, monitor);
}

void _maru_update_context_base(MARU_Context_Base *ctx_base, uint64_t field_mask,
                               const MARU_ContextAttributes *attributes) {
  if (field_mask & MARU_CONTEXT_ATTR_DIAGNOSTICS) {
    ctx_base->diagnostic_cb = attributes->diagnostic_cb;
    ctx_base->diagnostic_userdata = attributes->diagnostic_userdata;
  }

  if (field_mask & MARU_CONTEXT_ATTR_EVENT_CALLBACK) {
    ctx_base->event_cb = attributes->event_callback;
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
}

#ifdef MARU_INDIRECT_BACKEND
MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

  _maru_update_context_base(ctx_base, field_mask, attributes);

  if (ctx_base->backend->updateContext) {
    return ctx_base->backend->updateContext(context, field_mask, attributes);
  }

  return MARU_SUCCESS;
}
#endif

void _maru_update_window_base(MARU_Window_Base *win_base, uint64_t field_mask,
                              const MARU_WindowAttributes *attributes) {
  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
    win_base->pub.event_mask = attributes->event_mask;
  }
}

#ifdef MARU_INDIRECT_BACKEND
MARU_API MARU_Status
maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                   const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;

  _maru_update_window_base(win_base, field_mask, attributes);

  if (win_base->backend->updateWindow) {
    return win_base->backend->updateWindow(window, field_mask, attributes);
  }

  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->requestWindowFocus) {
    return win_base->backend->requestWindowFocus(window);
  }
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getWindowBackendHandle(MARU_Window *window,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
  MARU_API_VALIDATE(getWindowBackendHandle, window, out_type, out_handle);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  if (win_base->backend->getWindowBackendHandle) {
    return win_base->backend->getWindowBackendHandle(window, out_type, out_handle);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                              MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(getStandardCursor, context, shape, out_cursor);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->backend->getStandardCursor) {
    return ctx_base->backend->getStandardCursor(context, shape, out_cursor);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->backend->wakeContext) {
    return ctx_base->backend->wakeContext(context);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

  if (ctx_base->backend->updateMonitors) {
    MARU_Status res = ctx_base->backend->updateMonitors(context);
    if (res != MARU_SUCCESS) return res;
  }

  out_list->monitors = ctx_base->monitor_cache;
  out_list->count = ctx_base->monitor_cache_count;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  mon_base->ref_count++;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  if (mon_base->ref_count > 0) {
    mon_base->ref_count--;
    if (mon_base->ref_count == 0 && !mon_base->is_active) {
      _maru_monitor_free(mon_base);
    }
  }
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  if (mon_base->backend->getMonitorModes) {
    return mon_base->backend->getMonitorModes(monitor, out_list);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  const MARU_Monitor_Base *mon_base = (const MARU_Monitor_Base *)monitor;
  if (mon_base->backend->setMonitorMode) {
    return mon_base->backend->setMonitorMode(monitor, mode);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
  return MARU_SUCCESS;
}
#endif

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

MARU_API MARU_Version maru_getVersion(void) {
  return (MARU_Version){.major = MARU_VERSION_MAJOR,
                        .minor = MARU_VERSION_MINOR,
                        .patch = MARU_VERSION_PATCH};
}
