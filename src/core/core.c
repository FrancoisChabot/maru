#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

#include <stdlib.h>

#include "maru/maru.h"
#include "maru/native/cocoa.h"
#include "maru/native/linux.h"
#include "maru/native/wayland.h"
#include "maru/native/win32.h"
#include "maru/native/x11.h"
#ifdef __linux__
#include "linux/linux_internal.h"
typedef struct MARU_Context_Linux {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;
} MARU_Context_Linux;
#endif
#include "core_event_queue.h"

#ifdef MARU_VALIDATE_API_CALLS
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <stdio.h>

MARU_ThreadId _maru_getCurrentThreadId(void) {
#ifdef _WIN32
  return (MARU_ThreadId)GetCurrentThreadId();
#else
  return (MARU_ThreadId)pthread_self();
#endif
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
                                .context = ctx,
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
                                .context = ctx,
                                .window = NULL};
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  }
}
#endif
#endif

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventId type,
                          MARU_Window *window, const MARU_Event *event) {
  if (!ctx->pump_ctx) return;
  MARU_ASSUME(ctx->pump_ctx != NULL);

  const MARU_EventMask event_bit = MARU_EVENT_MASK(type);
  if ((ctx->pump_ctx->mask & event_bit) == 0) return;
  if (window &&
      (type == MARU_EVENT_DROP_ENTERED || type == MARU_EVENT_DROP_HOVERED ||
       type == MARU_EVENT_DROP_EXITED || type == MARU_EVENT_DROP_DROPPED)) {
    const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
    if (!win_base->attrs_effective.accept_drop) return;
  }

  ctx->pump_ctx->callback(type, window, event, ctx->pump_ctx->userdata);
}

void _maru_init_context_base(MARU_Context_Base *ctx_base) {
  if (ctx_base->allocator.alloc_cb == NULL) {
    ctx_base->allocator.alloc_cb = _maru_default_alloc;
    ctx_base->allocator.realloc_cb = _maru_default_realloc;
    ctx_base->allocator.free_cb = _maru_default_free;
    ctx_base->allocator.userdata = NULL;
  }
  ctx_base->pub.userdata = NULL;
  ctx_base->mouse_button_states = NULL;
  ctx_base->mouse_button_channels = NULL;
  ctx_base->animated_cursor_head = NULL;
  ctx_base->pub.mouse_button_state = NULL;
  ctx_base->pub.mouse_button_channels = NULL;
  ctx_base->pub.mouse_button_count = 0;
  for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
    ctx_base->pub.mouse_default_button_channels[i] = -1;
  }

  memset(ctx_base->keyboard_state, 0, sizeof(ctx_base->keyboard_state));
  ctx_base->pub.keyboard_state = ctx_base->keyboard_state;
  ctx_base->pub.keyboard_key_count = MARU_KEY_COUNT;

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

  ctx_base->monitor_cache = NULL;
  ctx_base->monitor_cache_count = 0;
  ctx_base->monitor_cache_capacity = 0;
  ctx_base->next_window_id = 1;

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx_base->creator_thread = _maru_getCurrentThreadId();
#endif
}

void _maru_drain_queued_events(MARU_Context_Base *ctx_base) {
  // First, check for windows that need a READY event
  for (MARU_Window_Base *it = ctx_base->window_list_head; it; it = it->ctx_next) {
    if (it->pending_ready_event) {
      it->pending_ready_event = false;
      it->pub.flags |= MARU_WINDOW_STATE_READY;
      MARU_Event evt = {0};
      evt.window_ready.geometry = maru_getWindowGeometry((MARU_Window *)it);
      _maru_dispatch_event(ctx_base, MARU_EVENT_WINDOW_READY, (MARU_Window *)it, &evt);
    }
  }

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

  if (field_mask & MARU_CONTEXT_ATTR_INHIBIT_IDLE) {
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

  if (field_mask & MARU_CONTEXT_ATTR_IDLE_TIMEOUT) {
    ctx_base->attrs_requested.idle_timeout_ms = attributes->idle_timeout_ms;
    ctx_base->attrs_effective.idle_timeout_ms = attributes->idle_timeout_ms;
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
  ctx_base->animated_cursor_head = NULL;
  _maru_event_queue_cleanup(&ctx_base->queued_events, ctx_base);
}

void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  win_base->pub.window_id = ctx_base->next_window_id++;
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

uint32_t _maru_cursor_frame_delay_ms(uint32_t delay_ms) {
  return (delay_ms == 0u) ? 1u : delay_ms;
}

static void _maru_link_animated_cursor(MARU_Context_Base *ctx_base,
                                       MARU_Cursor_Base *cursor) {
  cursor->anim_prev = NULL;
  cursor->anim_next = ctx_base->animated_cursor_head;
  if (ctx_base->animated_cursor_head) {
    ctx_base->animated_cursor_head->anim_prev = cursor;
  }
  ctx_base->animated_cursor_head = cursor;
}

bool _maru_register_animated_cursor(MARU_Cursor_Base *cursor, uint32_t frame_count,
                                    const uint32_t *frame_delays_ms,
                                    const MARU_CursorAnimationCallbacks *callbacks,
                                    uint64_t now_ms) {
  if (!cursor || !cursor->ctx_base || !frame_delays_ms || !callbacks ||
      !callbacks->select_frame || !callbacks->reapply || frame_count <= 1u) {
    return false;
  }

  cursor->anim_frame_count = frame_count;
  cursor->anim_frame_delays_ms = frame_delays_ms;
  cursor->anim_callbacks = callbacks;
  cursor->anim_current_frame = 0u;
  cursor->anim_next_frame_deadline_ms =
      (now_ms == 0u) ? 0u : now_ms + (uint64_t)frame_delays_ms[0];
  cursor->anim_enabled = true;
  _maru_link_animated_cursor(cursor->ctx_base, cursor);
  return true;
}

void _maru_unregister_animated_cursor(MARU_Cursor_Base *cursor) {
  if (!cursor || !cursor->anim_enabled || !cursor->ctx_base) {
    return;
  }
  MARU_Context_Base *ctx_base = cursor->ctx_base;
  if (cursor->anim_prev) {
    cursor->anim_prev->anim_next = cursor->anim_next;
  } else if (ctx_base->animated_cursor_head == cursor) {
    ctx_base->animated_cursor_head = cursor->anim_next;
  }
  if (cursor->anim_next) {
    cursor->anim_next->anim_prev = cursor->anim_prev;
  }
  cursor->anim_prev = NULL;
  cursor->anim_next = NULL;
  cursor->anim_enabled = false;
}

bool _maru_advance_animated_cursors(MARU_Context_Base *ctx_base, uint64_t now_ms) {
  if (!ctx_base || now_ms == 0u) {
    return false;
  }

  bool any_reapplied = false;
  const uint32_t max_advance_per_pump = 16u;
  for (MARU_Cursor_Base *cursor = ctx_base->animated_cursor_head; cursor;
       cursor = cursor->anim_next) {
    if (!cursor->anim_enabled || !cursor->anim_callbacks ||
        !cursor->anim_callbacks->select_frame || !cursor->anim_callbacks->reapply ||
        !cursor->anim_frame_delays_ms || cursor->anim_frame_count <= 1u) {
      continue;
    }

    if (cursor->anim_next_frame_deadline_ms == 0u) {
      cursor->anim_next_frame_deadline_ms =
          now_ms + (uint64_t)cursor->anim_frame_delays_ms[cursor->anim_current_frame];
    }
    if (now_ms < cursor->anim_next_frame_deadline_ms) {
      continue;
    }

    uint32_t advanced = 0u;
    while (now_ms >= cursor->anim_next_frame_deadline_ms &&
           advanced < max_advance_per_pump) {
      cursor->anim_current_frame =
          (cursor->anim_current_frame + 1u) % cursor->anim_frame_count;
      cursor->anim_callbacks->select_frame(cursor, cursor->anim_current_frame);
      cursor->anim_next_frame_deadline_ms +=
          (uint64_t)cursor->anim_frame_delays_ms[cursor->anim_current_frame];
      advanced++;
    }
    if (advanced == max_advance_per_pump &&
        now_ms >= cursor->anim_next_frame_deadline_ms) {
      cursor->anim_next_frame_deadline_ms = now_ms + 1u;
    }
    if (advanced > 0u && cursor->anim_callbacks->reapply(cursor)) {
      if (cursor->anim_callbacks->on_reapplied) {
        cursor->anim_callbacks->on_reapplied(cursor);
      }
      any_reapplied = true;
    }
  }

  return any_reapplied;
}

uint32_t _maru_adjust_timeout_for_cursor_animation(const MARU_Context_Base *ctx_base,
                                                   uint32_t timeout_ms,
                                                   uint64_t now_ms) {
  if (!ctx_base || now_ms == 0u) {
    return timeout_ms;
  }

  uint64_t next_deadline = 0u;
  for (const MARU_Cursor_Base *cursor = ctx_base->animated_cursor_head; cursor;
       cursor = cursor->anim_next) {
    if (!cursor->anim_enabled || cursor->anim_frame_count <= 1u ||
        cursor->anim_next_frame_deadline_ms == 0u) {
      continue;
    }
    if (next_deadline == 0u || cursor->anim_next_frame_deadline_ms < next_deadline) {
      next_deadline = cursor->anim_next_frame_deadline_ms;
    }
  }

  if (next_deadline == 0u) {
    return timeout_ms;
  }
  if (next_deadline <= now_ms) {
    return 0u;
  }

  uint64_t wait_ms_u64 = next_deadline - now_ms;
  if (wait_ms_u64 > (uint64_t)UINT32_MAX) {
    wait_ms_u64 = (uint64_t)UINT32_MAX;
  }
  const uint32_t cursor_timeout_ms = (uint32_t)wait_ms_u64;
  if (timeout_ms == MARU_NEVER || cursor_timeout_ms < timeout_ms) {
    return cursor_timeout_ms;
  }
  return timeout_ms;
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

bool _maru_post_event_internal(MARU_Context_Base *ctx_base, MARU_EventId type,
                                 MARU_Window *window, const MARU_Event *evt) {
  if (_maru_event_queue_push(&ctx_base->queued_events, type, window, *evt)) {
    return maru_wakeContext((MARU_Context *)ctx_base);
  }
  return false;
}

MARU_API bool maru_postEvent(MARU_Context *context, MARU_EventId type,
                             MARU_Window *window, MARU_UserDefinedEvent user_evt) {
  MARU_API_VALIDATE(postEvent, context, type, window, user_evt);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  if (maru_isContextLost(context)) {
    return false;
  }
  MARU_Event evt;
  evt.user = user_evt;

  return _maru_post_event_internal(ctx_base, type, window, &evt);
}


MARU_API MARU_Status maru_resetContextMetrics(MARU_Context *context) {
  MARU_API_VALIDATE(resetContextMetrics, context);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
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

MARU_API MARU_Version maru_getVersion(void) {
  return (MARU_Version){.major = MARU_VERSION_MAJOR,
                        .minor = MARU_VERSION_MINOR,
                        .patch = MARU_VERSION_PATCH};
}
