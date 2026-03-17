#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

#include <stdlib.h>

#include "maru/maru.h"
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

void *_maru_default_alloc(size_t size, void *userdata) {
  (void)userdata;
  return malloc(size);
}

void *_maru_default_realloc(void *ptr, size_t size, void *userdata) {
  (void)userdata;
  return realloc(ptr, size);
}

void _maru_default_free(void *ptr, void *userdata) {
  (void)userdata;
  free(ptr);
}

MARU_Status _maru_post_event_internal(MARU_Context_Base *ctx_base, MARU_EventId type,
                               MARU_Window *window, const MARU_Event *evt) {
  if (_maru_event_queue_push(&ctx_base->internal_events, type, window, *evt)) {
    return MARU_SUCCESS;
  }
  return MARU_FAILURE;
}

#define MARU_INTERNAL_EVENT_QUEUE_CAPACITY 256u

#ifdef MARU_ENABLE_DIAGNOSTICS
void _maru_reportDiagnosticOn(const MARU_Context *ctx,
                              MARU_DiagnosticSubjectKind subject_kind,
                              MARU_DiagnosticSubject subject,
                              MARU_Diagnostic diag, const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {
        .diagnostic = diag,
        .message = msg,
        .context = ctx,
        .subject_kind = subject_kind,
        .subject = subject,
    };
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  }
}

void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag,
                            const char *msg) {
  _maru_reportDiagnosticOn(
      ctx, MARU_DIAGNOSTIC_SUBJECT_NONE,
      (MARU_DiagnosticSubject){.window = NULL}, diag, msg);
}
#endif

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventId type,
                          MARU_Window *window, const MARU_Event *event) {
  if (!ctx->pump_ctx) {
    if (type == MARU_EVENT_DATA_REQUESTED && ctx->urgent_data_requested_callback) {
        ctx->urgent_data_requested_callback(type, window, event, ctx->urgent_data_requested_userdata);
    }
    return;
  }
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
  ctx_base->window_list_head = NULL;
  ctx_base->window_count = 0;
  ctx_base->next_window_id = 1;
  ctx_base->monitor_cache = NULL;
  ctx_base->monitor_cache_count = 0;
  ctx_base->monitor_cache_capacity = 0;
  ctx_base->pub.mouse_button_state = NULL;
  ctx_base->pub.mouse_button_channels = NULL;
  ctx_base->pub.mouse_button_count = 0;

  memset(ctx_base->keyboard_state, 0, sizeof(ctx_base->keyboard_state));
  ctx_base->pub.keyboard_state = ctx_base->keyboard_state;
  ctx_base->pub.keyboard_key_count = MARU_KEY_COUNT;

#ifdef MARU_VALIDATE_API_CALLS
  ctx_base->creator_thread = _maru_getCurrentThreadId();
#endif

  memset(&ctx_base->internal_events, 0, sizeof(ctx_base->internal_events));
  memset(&ctx_base->user_events, 0, sizeof(ctx_base->user_events));

  if (!_maru_event_queue_init(&ctx_base->internal_events, ctx_base,
                              MARU_INTERNAL_EVENT_QUEUE_CAPACITY)) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY,
                           "Failed to initialize internal event queue");
  }

  if (!_maru_event_queue_init(&ctx_base->user_events, ctx_base,
                              MARU_INTERNAL_EVENT_QUEUE_CAPACITY)) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY,
                           "Failed to initialize user event queue");
  }
}

void _maru_monitor_set_name(MARU_Monitor_Base *mon_base, const char *name) {
  if (mon_base->name_storage) {
    maru_context_free(mon_base->ctx_base, mon_base->name_storage);
    mon_base->name_storage = NULL;
    mon_base->pub.name = NULL;
  }
  if (name) {
    size_t len = strlen(name);
    mon_base->name_storage = (char *)maru_context_alloc(mon_base->ctx_base, len + 1);
    if (mon_base->name_storage) {
      memcpy(mon_base->name_storage, name, len + 1);
      mon_base->pub.name = mon_base->name_storage;
    }
  }
}

void _maru_drain_queued_events(MARU_Context_Base *ctx_base) {
  for (MARU_Window_Base *it = ctx_base->window_list_head; it; it = it->ctx_next) {
    if (it->pending_ready_event) {
      it->pending_ready_event = false;
      it->pub.flags |= MARU_WINDOW_STATE_READY;
      MARU_Event ready_evt = {0};
      _maru_dispatch_event(ctx_base, MARU_EVENT_WINDOW_READY, (MARU_Window *)it,
                           &ready_evt);
    }
  }

  MARU_EventQueue *qi = &ctx_base->internal_events;
  MARU_EventQueue *qu = &ctx_base->user_events;
  MARU_EventId type;
  MARU_Window *window;
  MARU_Event evt;

  while (_maru_event_queue_pop(qi, &type, &window, &evt)) {
    _maru_dispatch_event(ctx_base, type, window, &evt);
  }
  while (_maru_event_queue_pop(qu, &type, &window, &evt)) {
    _maru_dispatch_event(ctx_base, type, NULL, &evt);
  }
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

void _maru_update_window_base(MARU_Window_Base *win_base, uint64_t field_mask,
                              const MARU_WindowAttributes *attributes) {
  MARU_Context_Base *ctx_base = win_base->ctx_base;
  win_base->attrs_dirty_mask |= field_mask;

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    maru_context_free(ctx_base, win_base->title_storage);
    win_base->title_storage = NULL;
    win_base->attrs_requested.title = NULL;
    win_base->attrs_effective.title = NULL;
    win_base->pub.title = NULL;

    if (attributes->title) {
      size_t len = strlen(attributes->title);
      win_base->title_storage = (char *)maru_context_alloc(ctx_base, len + 1);
      if (win_base->title_storage) {
        memcpy(win_base->title_storage, attributes->title, len + 1);
        win_base->attrs_requested.title = win_base->title_storage;
        win_base->attrs_effective.title = win_base->title_storage;
        win_base->pub.title = win_base->title_storage;
      }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT) {
    maru_context_free(ctx_base, win_base->surrounding_text_storage);
    win_base->surrounding_text_storage = NULL;
    win_base->attrs_requested.surrounding_text = NULL;
    win_base->attrs_effective.surrounding_text = NULL;

    if (attributes->surrounding_text) {
      size_t len = strlen(attributes->surrounding_text);
      win_base->surrounding_text_storage =
          (char *)maru_context_alloc(ctx_base, len + 1);
      if (win_base->surrounding_text_storage) {
        memcpy(win_base->surrounding_text_storage, attributes->surrounding_text,
               len + 1);
        win_base->attrs_requested.surrounding_text =
            win_base->surrounding_text_storage;
        win_base->attrs_effective.surrounding_text =
            win_base->surrounding_text_storage;
      }
    }
  }

  // Update simple fields that don't need special storage management
  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    win_base->attrs_requested.visible = attributes->visible;
    win_base->attrs_effective.visible = attributes->visible;
  }
  if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
    win_base->attrs_requested.presentation_state = attributes->presentation_state;
    win_base->attrs_effective.presentation_state = attributes->presentation_state;
  }
  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
    win_base->attrs_requested.resizable = attributes->resizable;
    win_base->attrs_effective.resizable = attributes->resizable;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_POSITION) {
    win_base->attrs_requested.dip_position = attributes->dip_position;
    win_base->attrs_effective.dip_position = attributes->dip_position;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_SIZE) {
    win_base->attrs_requested.dip_size = attributes->dip_size;
    win_base->attrs_effective.dip_size = attributes->dip_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_MIN_SIZE) {
    win_base->attrs_requested.dip_min_size = attributes->dip_min_size;
    win_base->attrs_effective.dip_min_size = attributes->dip_min_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_MAX_SIZE) {
    win_base->attrs_requested.dip_max_size = attributes->dip_max_size;
    win_base->attrs_effective.dip_max_size = attributes->dip_max_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
    win_base->attrs_requested.aspect_ratio = attributes->aspect_ratio;
    win_base->attrs_effective.aspect_ratio = attributes->aspect_ratio;
  }
  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    win_base->attrs_requested.cursor_mode = attributes->cursor_mode;
    win_base->attrs_effective.cursor_mode = attributes->cursor_mode;
  }
  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    win_base->attrs_requested.cursor = attributes->cursor;
    win_base->attrs_effective.cursor = attributes->cursor;
  }
  if (field_mask & MARU_WINDOW_ATTR_ICON) {
    win_base->attrs_requested.icon = attributes->icon;
    win_base->attrs_effective.icon = attributes->icon;
  }
  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
    win_base->attrs_requested.monitor = attributes->monitor;
    win_base->attrs_effective.monitor = attributes->monitor;
  }
  if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
    win_base->attrs_requested.accept_drop = attributes->accept_drop;
    win_base->attrs_effective.accept_drop = attributes->accept_drop;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
    win_base->attrs_requested.dip_viewport_size = attributes->dip_viewport_size;
    win_base->attrs_effective.dip_viewport_size = attributes->dip_viewport_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
    win_base->attrs_requested.text_input_type = attributes->text_input_type;
    win_base->attrs_effective.text_input_type = attributes->text_input_type;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT) {
    win_base->attrs_requested.dip_text_input_rect = attributes->dip_text_input_rect;
    win_base->attrs_effective.dip_text_input_rect = attributes->dip_text_input_rect;
  }
  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE) {
    win_base->attrs_requested.surrounding_cursor_byte =
        attributes->surrounding_cursor_byte;
    win_base->attrs_effective.surrounding_cursor_byte =
        attributes->surrounding_cursor_byte;
  }
  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE) {
    win_base->attrs_requested.surrounding_anchor_byte =
        attributes->surrounding_anchor_byte;
    win_base->attrs_effective.surrounding_anchor_byte =
        attributes->surrounding_anchor_byte;
  }
}

void _maru_cleanup_context_base(MARU_Context_Base *ctx_base) {
  for (uint32_t i = 0; i < ctx_base->monitor_cache_count; i++) {
    MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)ctx_base->monitor_cache[i];
    mon_base->is_active = false;
    maru_releaseMonitor((MARU_Monitor *)mon_base);
  }
  maru_context_free(ctx_base, ctx_base->monitor_cache);
  ctx_base->monitor_cache = NULL;
  ctx_base->monitor_cache_count = 0;
  ctx_base->monitor_cache_capacity = 0;

  maru_context_free(ctx_base, ctx_base->mouse_button_states);
  maru_context_free(ctx_base, ctx_base->mouse_button_channels);

  _maru_event_queue_cleanup(&ctx_base->internal_events, ctx_base);
  _maru_event_queue_cleanup(&ctx_base->user_events, ctx_base);
}

void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window) {
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  
  if (ctx_base->next_window_id == MARU_WINDOW_ID_NONE) {
    ctx_base->next_window_id = 1;
  }
  win_base->pub.window_id = ctx_base->next_window_id++;

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
  } else {
    ctx_base->window_list_head = win_base->ctx_next;
  }
  if (win_base->ctx_next) {
    win_base->ctx_next->ctx_prev = win_base->ctx_prev;
  }
  ctx_base->window_count--;
}

uint32_t _maru_cursor_frame_delay_ms(uint32_t delay_ms) {
  return (delay_ms == 0) ? 1u : delay_ms;
}

bool _maru_register_animated_cursor(MARU_Cursor_Base *cursor, uint32_t frame_count,
                                    const uint32_t *frame_delays_ms,
                                    const MARU_CursorAnimationCallbacks *callbacks,
                                    uint64_t now_ms) {
  if (cursor->anim_enabled) return true;

  cursor->anim_frame_count = frame_count;
  cursor->anim_frame_delays_ms = frame_delays_ms;
  cursor->anim_callbacks = callbacks;
  cursor->anim_current_frame = 0;
  cursor->anim_next_frame_deadline_ms = now_ms + _maru_cursor_frame_delay_ms(frame_delays_ms[0]);
  cursor->anim_enabled = true;

  cursor->anim_next = cursor->ctx_base->animated_cursor_head;
  if (cursor->ctx_base->animated_cursor_head) {
    cursor->ctx_base->animated_cursor_head->anim_prev = cursor;
  }
  cursor->ctx_base->animated_cursor_head = cursor;
  cursor->anim_prev = NULL;

  return true;
}

void _maru_unregister_animated_cursor(MARU_Cursor_Base *cursor) {
  if (!cursor->anim_enabled) return;

  if (cursor->anim_prev) {
    cursor->anim_prev->anim_next = cursor->anim_next;
  } else {
    cursor->ctx_base->animated_cursor_head = cursor->anim_next;
  }
  if (cursor->anim_next) {
    cursor->anim_next->anim_prev = cursor->anim_prev;
  }

  cursor->anim_enabled = false;
  cursor->anim_prev = NULL;
  cursor->anim_next = NULL;
}

bool _maru_advance_animated_cursors(MARU_Context_Base *ctx_base, uint64_t now_ms) {
  bool any_advanced = false;
  MARU_Cursor_Base *it = ctx_base->animated_cursor_head;
  while (it) {
    MARU_Cursor_Base *next = it->anim_next;
    if (now_ms >= it->anim_next_frame_deadline_ms) {
      uint32_t advanced_frames = 0;
      while (now_ms >= it->anim_next_frame_deadline_ms && advanced_frames < 16) {
        it->anim_current_frame = (it->anim_current_frame + 1) % it->anim_frame_count;
        it->anim_next_frame_deadline_ms +=
            _maru_cursor_frame_delay_ms(it->anim_frame_delays_ms[it->anim_current_frame]);
        advanced_frames++;
      }
      if (it->anim_callbacks && it->anim_callbacks->select_frame) {
        it->anim_callbacks->select_frame(it, it->anim_current_frame);
      }
      if (it->anim_callbacks && it->anim_callbacks->reapply) {
        if (it->anim_callbacks->reapply(it)) {
          any_advanced = true;
        }
      }
    }
    it = next;
  }
  return any_advanced;
}

uint32_t _maru_adjust_timeout_for_cursor_animation(const MARU_Context_Base *ctx_base,
                                                   uint32_t timeout_ms,
                                                   uint64_t now_ms) {
  uint32_t res = timeout_ms;
  MARU_Cursor_Base *it = ctx_base->animated_cursor_head;
  while (it) {
    if (it->anim_next_frame_deadline_ms <= now_ms) {
      return 0;
    }
    uint64_t diff = it->anim_next_frame_deadline_ms - now_ms;
    if (diff < (uint64_t)res) {
      res = (uint32_t)diff;
    }
    it = it->anim_next;
  }
  return res;
}

MARU_API MARU_Status maru_postEvent(MARU_Context *context, MARU_EventId type,
                                    MARU_UserDefinedEvent evt) {
  MARU_API_VALIDATE(postEvent, context, type, evt);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));

  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  MARU_Event internal_evt;
  memset(&internal_evt, 0, sizeof(internal_evt));
  internal_evt.user = evt;

  if (_maru_event_queue_push(&ctx_base->user_events, type, NULL, internal_evt)) {
    return maru_wakeContext(context);
  }

  return MARU_FAILURE;
}

#ifdef MARU_INDIRECT_BACKEND
MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  return ctx_base->backend->wakeContext(context);
}
#endif

MARU_API MARU_Status maru_getWindow(MARU_Context *context,
                                    MARU_WindowId window_id,
                                    MARU_Window **out_window) {
  MARU_API_VALIDATE(getWindow, context, window_id, out_window);

  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));

  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  for (MARU_Window_Base *it = ctx_base->window_list_head; it; it = it->ctx_next) {
    if (it->pub.window_id == window_id) {
      *out_window = (MARU_Window *)it;
      return MARU_SUCCESS;
    }
  }

  return MARU_FAILURE;
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
      if (current == 1u && !mon_base->is_active) {
        _maru_monitor_free(mon_base);
      }
      return;
    }
  }
}

MARU_API const char *maru_getDiagnosticString(MARU_Diagnostic diagnostic) {
#ifdef MARU_ENABLE_DIAGNOSTICS
  switch (diagnostic) {
    case MARU_DIAGNOSTIC_NONE:
      return "NONE";
    case MARU_DIAGNOSTIC_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE:
      return "RESOURCE_UNAVAILABLE";
    case MARU_DIAGNOSTIC_BACKEND_FAILURE:
      return "BACKEND_FAILURE";
    case MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED:
      return "FEATURE_UNSUPPORTED";
    default:
      return "UNKNOWN";
  }
#else
  (void)diagnostic;
  return "";
#endif
}


MARU_API MARU_Version maru_getVersion(void) {
  return (MARU_Version){.major = MARU_VERSION_MAJOR,
                        .minor = MARU_VERSION_MINOR,
                        .patch = MARU_VERSION_PATCH};
}
