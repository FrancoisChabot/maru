// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "linux_internal.h"
#include "x11_internal.h"
#include "dlib/loader.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <time.h>

MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                    const MARU_ContextAttributes *attributes) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    // X11 idle inhibition plumbing is not implemented yet.
  }

  return MARU_SUCCESS;
}

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

  if (!_maru_x11_init_context_mouse_channels(ctx)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  if (!maru_load_x11_symbols(&ctx->base, &ctx->x11_lib)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }
  (void)maru_load_xcursor_symbols(&ctx->base, &ctx->xcursor_lib);
  (void)maru_load_xi2_symbols(&ctx->base, &ctx->xi2_lib);
  (void)maru_load_xshape_symbols(&ctx->base, &ctx->xshape_lib);
  (void)maru_load_xrandr_symbols(&ctx->base, &ctx->xrandr_lib);

  ctx->display = ctx->x11_lib.XOpenDisplay(NULL);
  if (!ctx->display) {
    maru_unload_xrandr_symbols(&ctx->xrandr_lib);
    maru_unload_xshape_symbols(&ctx->xshape_lib);
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
    maru_unload_x11_symbols(&ctx->x11_lib);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  (void)setlocale(LC_CTYPE, "");
  (void)ctx->x11_lib.XSetLocaleModifiers("");
  ctx->xim = ctx->x11_lib.XOpenIM(ctx->display, NULL, NULL, NULL);

  ctx->screen = DefaultScreen(ctx->display);
  ctx->root = RootWindow(ctx->display, ctx->screen);

  ctx->wm_protocols = ctx->x11_lib.XInternAtom(ctx->display, "WM_PROTOCOLS", False);
  ctx->wm_delete_window = ctx->x11_lib.XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
  ctx->net_wm_name = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_NAME", False);
  ctx->net_wm_icon_name = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_ICON_NAME", False);
  ctx->net_wm_icon = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_ICON", False);
  ctx->net_wm_state = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE", False);
  ctx->net_wm_state_fullscreen = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_FULLSCREEN", False);
  ctx->net_wm_state_maximized_vert = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  ctx->net_wm_state_maximized_horz = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  ctx->net_wm_state_demands_attention = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
  ctx->net_active_window = ctx->x11_lib.XInternAtom(ctx->display, "_NET_ACTIVE_WINDOW", False);
  ctx->selection_clipboard = ctx->x11_lib.XInternAtom(ctx->display, "CLIPBOARD", False);
  ctx->selection_primary = XA_PRIMARY;
  ctx->selection_targets = ctx->x11_lib.XInternAtom(ctx->display, "TARGETS", False);
  ctx->selection_incr = ctx->x11_lib.XInternAtom(ctx->display, "INCR", False);
  ctx->utf8_string = ctx->x11_lib.XInternAtom(ctx->display, "UTF8_STRING", False);
  ctx->text_atom = ctx->x11_lib.XInternAtom(ctx->display, "TEXT", False);
  ctx->compound_text = ctx->x11_lib.XInternAtom(ctx->display, "COMPOUND_TEXT", False);
  ctx->maru_selection_property =
      ctx->x11_lib.XInternAtom(ctx->display, "MARU_SELECTION", False);
  ctx->xdnd_aware = ctx->x11_lib.XInternAtom(ctx->display, "XdndAware", False);
  ctx->xdnd_enter = ctx->x11_lib.XInternAtom(ctx->display, "XdndEnter", False);
  ctx->xdnd_position = ctx->x11_lib.XInternAtom(ctx->display, "XdndPosition", False);
  ctx->xdnd_status = ctx->x11_lib.XInternAtom(ctx->display, "XdndStatus", False);
  ctx->xdnd_leave = ctx->x11_lib.XInternAtom(ctx->display, "XdndLeave", False);
  ctx->xdnd_drop = ctx->x11_lib.XInternAtom(ctx->display, "XdndDrop", False);
  ctx->xdnd_finished = ctx->x11_lib.XInternAtom(ctx->display, "XdndFinished", False);
  ctx->xdnd_selection = ctx->x11_lib.XInternAtom(ctx->display, "XdndSelection", False);
  ctx->xdnd_type_list = ctx->x11_lib.XInternAtom(ctx->display, "XdndTypeList", False);
  ctx->xdnd_action_copy = ctx->x11_lib.XInternAtom(ctx->display, "XdndActionCopy", False);
  ctx->xdnd_action_move = ctx->x11_lib.XInternAtom(ctx->display, "XdndActionMove", False);
  ctx->xdnd_action_link = ctx->x11_lib.XInternAtom(ctx->display, "XdndActionLink", False);
  (void)_maru_x11_enable_xi2_raw_motion(ctx);

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    ctx->x11_lib.XCloseDisplay(ctx->display);
    maru_unload_xrandr_symbols(&ctx->xrandr_lib);
    maru_unload_xshape_symbols(&ctx->xshape_lib);
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
    maru_unload_x11_symbols(&ctx->x11_lib);
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

  if (!_maru_linux_common_run(&ctx->linux_common)) {
    maru_destroyContext_X11((MARU_Context *)ctx);
    return MARU_FAILURE;
  }

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_x11_release_pointer_lock(ctx, NULL);
  
  while (ctx->base.window_list_head) {
    maru_destroyWindow_X11((MARU_Window *)ctx->base.window_list_head);
  }

  _maru_linux_common_cleanup(&ctx->linux_common);
  _maru_x11_clear_mime_query_cache(ctx);
  _maru_x11_clear_pending_request(ctx, &ctx->clipboard_request);
  _maru_x11_clear_pending_request(ctx, &ctx->primary_request);
  _maru_x11_clear_pending_request(ctx, &ctx->dnd_request);
  
  MARU_X11DataOffer *clipboard_offer = _maru_x11_get_offer(ctx, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD);
  MARU_X11DataOffer *primary_offer = _maru_x11_get_offer(ctx, MARU_DATA_EXCHANGE_TARGET_PRIMARY);
  MARU_X11DataOffer *dnd_offer = _maru_x11_get_offer(ctx, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  
  _maru_x11_clear_offer(ctx, clipboard_offer);
  _maru_x11_clear_offer(ctx, primary_offer);
  _maru_x11_clear_offer(ctx, dnd_offer);

  _maru_x11_clear_dnd_session(ctx);
  _maru_x11_clear_dnd_source_session(ctx, false, MARU_DROP_ACTION_NONE);
  
  if (ctx->display) {
    if (ctx->xim) {
      ctx->x11_lib.XCloseIM(ctx->xim);
      ctx->xim = NULL;
    }
    if (ctx->hidden_cursor) {
      ctx->x11_lib.XFreeCursor(ctx->display, ctx->hidden_cursor);
      ctx->hidden_cursor = (Cursor)0;
    }
    ctx->x11_lib.XCloseDisplay(ctx->display);
  }

  while (ctx->base.monitor_cache_count > 0) {
    MARU_Monitor_Base *monitor =
        (MARU_Monitor_Base *)ctx->base.monitor_cache[ctx->base.monitor_cache_count - 1u];
    monitor->is_active = false;
    monitor->pub.flags |= MARU_MONITOR_STATE_LOST;
    ctx->base.monitor_cache_count--;
    maru_releaseMonitor_X11((MARU_Monitor *)monitor);
  }

  maru_unload_xrandr_symbols(&ctx->xrandr_lib);
  maru_unload_xshape_symbols(&ctx->xshape_lib);
  maru_unload_xcursor_symbols(&ctx->xcursor_lib);
  maru_unload_xi2_symbols(&ctx->xi2_lib);
  maru_unload_x11_symbols(&ctx->x11_lib);

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  uint64_t val = 1;
  if (write(ctx->linux_common.worker.event_fd, &val, sizeof(val)) < 0) {
      return MARU_FAILURE;
  }
  return MARU_SUCCESS;
}

bool _maru_x11_copy_string(MARU_Context_X11 *ctx, const char *src,
                                  char **out_str) {
  const size_t len = strlen(src);
  char *copy = (char *)maru_context_alloc(&ctx->base, len + 1u);
  if (!copy) {
    *out_str = NULL;
    return false;
  }
  memcpy(copy, src, len + 1u);
  *out_str = copy;
  return true;
}

uint64_t _maru_x11_get_monotonic_time_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

uint64_t _maru_x11_get_monotonic_time_ms(void) {
  const uint64_t ns = _maru_x11_get_monotonic_time_ns();
  if (ns == 0) {
    return 0;
  }
  return ns / 1000000ull;
}

void _maru_x11_record_pump_duration_ns(MARU_Context_X11 *ctx,
                                              uint64_t duration_ns) {
  MARU_ContextMetrics *metrics = &ctx->base.metrics;
  metrics->pump_call_count_total++;
  if (metrics->pump_call_count_total == 1) {
    metrics->pump_duration_avg_ns = duration_ns;
    metrics->pump_duration_peak_ns = duration_ns;
    return;
  }

  if (duration_ns > metrics->pump_duration_peak_ns) {
    metrics->pump_duration_peak_ns = duration_ns;
  }

  if (duration_ns >= metrics->pump_duration_avg_ns) {
    metrics->pump_duration_avg_ns +=
        (duration_ns - metrics->pump_duration_avg_ns) /
        metrics->pump_call_count_total;
  } else {
    metrics->pump_duration_avg_ns -=
        (metrics->pump_duration_avg_ns - duration_ns) /
        metrics->pump_call_count_total;
  }
}

static uint64_t _maru_x11_next_cursor_deadline_ms(const MARU_Context_X11 *ctx) {
  uint64_t next_deadline = 0;
  for (const MARU_Cursor_X11 *cur = ctx->animated_cursors_head; cur;
       cur = cur->anim_next) {
    if (!cur->is_animated || cur->frame_count <= 1 || cur->next_frame_deadline_ms == 0) {
      continue;
    }
    if (next_deadline == 0 || cur->next_frame_deadline_ms < next_deadline) {
      next_deadline = cur->next_frame_deadline_ms;
    }
  }
  return next_deadline;
}

static int _maru_x11_compute_poll_timeout_ms(MARU_Context_X11 *ctx,
                                             uint32_t timeout_ms) {
  int timeout = -1;
  if (timeout_ms != MARU_NEVER) {
    timeout = (timeout_ms > (uint32_t)INT_MAX) ? INT_MAX : (int)timeout_ms;
  }
  const uint64_t cursor_deadline_ms = _maru_x11_next_cursor_deadline_ms(ctx);
  if (cursor_deadline_ms == 0) {
    return timeout;
  }

  const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
  if (now_ms == 0) {
    return timeout;
  }
  if (cursor_deadline_ms <= now_ms) {
    return 0;
  }

  uint64_t wait_ms_u64 = cursor_deadline_ms - now_ms;
  if (wait_ms_u64 > (uint64_t)INT_MAX) {
    wait_ms_u64 = (uint64_t)INT_MAX;
  }
  const int cursor_timeout_ms = (int)wait_ms_u64;
  if (timeout < 0 || cursor_timeout_ms < timeout) {
    timeout = cursor_timeout_ms;
  }
  return timeout;
}

void _maru_x11_process_event(MARU_Context_X11 *ctx, XEvent *ev) {
  if (ev->type == GenericEvent && ctx->xi2_raw_motion_enabled &&
      ev->xcookie.extension == ctx->xi2_opcode &&
      ctx->x11_lib.XGetEventData(ctx->display, &ev->xcookie)) {
    if (ev->xcookie.evtype == XI_RawMotion && ctx->locked_window &&
        (ctx->locked_window->base.pub.cursor_mode == MARU_CURSOR_LOCKED)) {
      const XIRawEvent *raw = (const XIRawEvent *)ev->xcookie.data;
      if (raw && raw->raw_values && raw->valuators.mask &&
          raw->valuators.mask_len > 0) {
        const double *values = raw->raw_values;
        const int bit_count = raw->valuators.mask_len * 8;
        MARU_Scalar raw_dx = (MARU_Scalar)0.0;
        MARU_Scalar raw_dy = (MARU_Scalar)0.0;
        for (int bit = 0; bit < bit_count; ++bit) {
          if (!XIMaskIsSet(raw->valuators.mask, bit)) {
            continue;
          }
          const MARU_Scalar value = (MARU_Scalar)(*values++);
          if (bit == 0) {
            raw_dx += value;
          } else if (bit == 1) {
            raw_dy += value;
          }
        }
        if (raw_dx != (MARU_Scalar)0.0 || raw_dy != (MARU_Scalar)0.0) {
          ctx->locked_raw_dx_accum += raw_dx;
          ctx->locked_raw_dy_accum += raw_dy;
          ctx->locked_raw_pending = true;
        }
      }
    }
    ctx->x11_lib.XFreeEventData(ctx->display, &ev->xcookie);
  }

  if (_maru_x11_process_window_event(ctx, ev)) return;
  if (_maru_x11_process_input_event(ctx, ev)) return;
  if (_maru_x11_process_dataexchange_event(ctx, ev)) return;
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  const uint64_t pump_start_ns = _maru_x11_get_monotonic_time_ns();
  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;
  _maru_x11_clear_mime_query_cache(ctx);

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_drain_queued_events(&ctx->base);

  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      if (ctx->x11_lib.XFilterEvent(&ev, None)) continue;
      _maru_x11_process_event(ctx, &ev);
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0 && _maru_x11_advance_animated_cursors(ctx, now_ms)) {
      ctx->x11_lib.XFlush(ctx->display);
    }
  }

  struct pollfd pfds[32];
  uint32_t nfds = 0;

  pfds[nfds].fd = ctx->linux_common.worker.event_fd;
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;
  nfds++;

  pfds[nfds].fd = ctx->x11_lib.XConnectionNumber(ctx->display);
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;
  nfds++;

  uint32_t ctrl_count = _maru_linux_common_fill_pollfds(&ctx->linux_common, &pfds[nfds], 32 - nfds);
  uint32_t ctrl_start_idx = nfds;
  nfds += ctrl_count;

  int poll_timeout = _maru_x11_compute_poll_timeout_ms(ctx, timeout_ms);
  int ret = poll(pfds, nfds, poll_timeout);

  if (ret > 0) {
    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      if (read(ctx->linux_common.worker.event_fd, &val, sizeof(val)) < 0) {}
    }

    if (pfds[1].revents & POLLIN) {
    }

    if (ctrl_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common, &pfds[ctrl_start_idx], ctrl_count);
    }
  }

  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      if (ctx->x11_lib.XFilterEvent(&ev, None)) continue;
      _maru_x11_process_event(ctx, &ev);
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0 && _maru_x11_advance_animated_cursors(ctx, now_ms)) {
      ctx->x11_lib.XFlush(ctx->display);
    }
  }

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  ctx->base.pump_ctx = NULL;

  const uint64_t pump_end_ns = _maru_x11_get_monotonic_time_ns();
  if (pump_start_ns != 0 && pump_end_ns != 0 && pump_end_ns >= pump_start_ns) {
    _maru_x11_record_pump_duration_ns(ctx, pump_end_ns - pump_start_ns);
  }

  return MARU_SUCCESS;
}

void *maru_getContextNativeHandle_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  return ctx->display;
}
