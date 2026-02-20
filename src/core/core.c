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
  if (ctx_base->window_cache) {
    maru_context_free(ctx_base, ctx_base->window_cache);
  }
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
  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    if (win_base->title) {
      maru_context_free(win_base->ctx_base, win_base->title);
    }
    size_t len = strlen(attributes->title);
    win_base->title = (char *)maru_context_alloc(win_base->ctx_base, len + 1);
    memcpy(win_base->title, attributes->title, len + 1);
    win_base->pub.title = win_base->title;
  }

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
    win_base->pub.event_mask = attributes->event_mask;
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
    if (attributes->mouse_passthrough) {
      win_base->pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
    } else {
      win_base->pub.flags &= ~MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
    }
  }
}

void _maru_init_window_base(MARU_Window_Base *win_base, MARU_Context_Base *ctx_base, const MARU_WindowCreateInfo *create_info) {
  memset(win_base, 0, sizeof(MARU_Window_Base));
  win_base->ctx_base = ctx_base;
  win_base->pub.context = (MARU_Context *)ctx_base;
  win_base->pub.userdata = create_info->userdata;
  win_base->pub.metrics = &win_base->metrics;
  win_base->pub.keyboard_state = win_base->keyboard_state;
  win_base->pub.keyboard_key_count = MARU_KEY_COUNT;
  win_base->pub.event_mask = create_info->attributes.event_mask;
  win_base->pub.cursor_mode = create_info->attributes.cursor_mode;
  win_base->pub.current_cursor = create_info->attributes.cursor;

  if (create_info->attributes.title) {
    size_t len = strlen(create_info->attributes.title);
    win_base->title = (char *)maru_context_alloc(ctx_base, len + 1);
    memcpy(win_base->title, create_info->attributes.title, len + 1);
    win_base->pub.title = win_base->title;
  }
}

void _maru_cleanup_window_base(MARU_Window_Base *win_base) {
  if (win_base->title) {
    maru_context_free(win_base->ctx_base, win_base->title);
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

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (ctx_base->backend->createCursor) {
    return ctx_base->backend->createCursor(context, create_info, out_cursor);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  MARU_Context_Base *ctx_base = cur_base->ctx_base;

  // Reset any window using this cursor
  for (uint32_t i = 0; i < ctx_base->window_cache_count; i++) {
    MARU_Window *window = ctx_base->window_cache[i];
    if (((const MARU_WindowExposed *)window)->current_cursor == cursor) {
      maru_setWindowCursor(window, NULL);
    }
  }

  if (cur_base->backend->destroyCursor) {
    return cur_base->backend->destroyCursor(cursor);
  }
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
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
