// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "dlib/loader.h"
#include "linux_internal.h"
#include "maru_api_constraints.h"
#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <limits.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void _maru_x11_apply_idle_inhibit(MARU_Context_X11 *ctx) {
  if (!ctx->display || !ctx->xss_lib.base.available ||
      !ctx->xss_lib.XScreenSaverSuspend ||
      !ctx->xss_lib.XScreenSaverQueryExtension) {
    return;
  }
  if (ctx->xss_idle_inhibit_active == ctx->base.inhibit_idle) {
    return;
  }

  int event_base = 0;
  int error_base = 0;
  if (!ctx->xss_lib.XScreenSaverQueryExtension(ctx->display, &event_base,
                                               &error_base)) {
    return;
  }

  ctx->xss_lib.XScreenSaverSuspend(ctx->display,
                                   ctx->base.inhibit_idle ? True : False);
  ctx->x11_lib.XFlush(ctx->display);
  ctx->xss_idle_inhibit_active = ctx->base.inhibit_idle;
}

static void _maru_x11_detect_pointer_barrier_support(MARU_Context_X11 *ctx) {
  ctx->xfixes_pointer_barriers_available = false;
  if (!ctx->display || !ctx->xfixes_lib.base.available ||
      !ctx->xfixes_lib.XFixesQueryExtension ||
      !ctx->xfixes_lib.XFixesQueryVersion ||
      !ctx->xfixes_lib.XFixesCreatePointerBarrier ||
      !ctx->xfixes_lib.XFixesDestroyPointerBarrier) {
    return;
  }

  int event_base = 0;
  int error_base = 0;
  if (!ctx->xfixes_lib.XFixesQueryExtension(ctx->display, &event_base,
                                            &error_base)) {
    return;
  }

  int major = 5;
  int minor = 0;
  if (!ctx->xfixes_lib.XFixesQueryVersion(ctx->display, &major, &minor)) {
    return;
  }

  ctx->xfixes_pointer_barriers_available =
      (major > 5) || (major == 5 && minor >= 0);
}

MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                   const MARU_ContextAttributes *attributes) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBIT_IDLE) {
    _maru_x11_apply_idle_inhibit(ctx);
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
  ctx->base.pub.userdata = create_info->userdata;
  ctx->linux_common.controller_snapshot_dirty = true;

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_X11;
  ctx->base.backend = &maru_backend_X11;
#endif

  if (!_maru_x11_init_context_mouse_channels(ctx)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  if (!maru_load_x11_symbols(&ctx->base, &ctx->x11_lib)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }
  (void)maru_load_xcursor_symbols(&ctx->base, &ctx->xcursor_lib);
  (void)maru_load_xi2_symbols(&ctx->base, &ctx->xi2_lib);
  (void)maru_load_xshape_symbols(&ctx->base, &ctx->xshape_lib);
  (void)maru_load_xrandr_symbols(&ctx->base, &ctx->xrandr_lib);
  (void)maru_load_xfixes_symbols(&ctx->base, &ctx->xfixes_lib);
  (void)maru_load_xss_symbols(&ctx->base, &ctx->xss_lib);

  ctx->display = ctx->x11_lib.XOpenDisplay(NULL);
  if (!ctx->display) {
    maru_unload_xfixes_symbols(&ctx->xfixes_lib);
    maru_unload_xrandr_symbols(&ctx->xrandr_lib);
    maru_unload_xshape_symbols(&ctx->xshape_lib);
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
    maru_unload_xss_symbols(&ctx->xss_lib);
    maru_unload_x11_symbols(&ctx->x11_lib);
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  (void)ctx->x11_lib.XSetLocaleModifiers("");
  ctx->xim = ctx->x11_lib.XOpenIM(ctx->display, NULL, NULL, NULL);
  if (!ctx->xim) {
    MARU_REPORT_DIAGNOSTIC(
        (MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
        "X11 IME support unavailable. Configure the process locale before "
        "creating the context to enable XIM.");
  }

  ctx->screen = DefaultScreen(ctx->display);
  ctx->root = RootWindow(ctx->display, ctx->screen);

  ctx->wm_protocols =
      ctx->x11_lib.XInternAtom(ctx->display, "WM_PROTOCOLS", False);
  ctx->wm_delete_window =
      ctx->x11_lib.XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
  ctx->net_wm_name =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_NAME", False);
  ctx->net_wm_icon_name =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_ICON_NAME", False);
  ctx->net_wm_icon =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_ICON", False);
  ctx->net_wm_state =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE", False);
  ctx->wm_state = ctx->x11_lib.XInternAtom(ctx->display, "WM_STATE", False);
  ctx->net_wm_state_fullscreen =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_FULLSCREEN", False);
  ctx->net_wm_state_maximized_vert = ctx->x11_lib.XInternAtom(
      ctx->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  ctx->net_wm_state_maximized_horz = ctx->x11_lib.XInternAtom(
      ctx->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  ctx->net_wm_state_demands_attention = ctx->x11_lib.XInternAtom(
      ctx->display, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
  ctx->net_active_window =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_ACTIVE_WINDOW", False);
  ctx->net_wm_frame_drawn =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_FRAME_DRAWN", False);
  ctx->net_wm_frame_timings =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_FRAME_TIMINGS", False);
  ctx->net_wm_sync_request =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_SYNC_REQUEST", False);
  ctx->net_wm_sync_request_counter =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_SYNC_REQUEST_COUNTER", False);
  ctx->net_supported =
      ctx->x11_lib.XInternAtom(ctx->display, "_NET_SUPPORTED", False);
  ctx->motif_wm_hints =
      ctx->x11_lib.XInternAtom(ctx->display, "_MOTIF_WM_HINTS", False);
  ctx->selection_clipboard =
      ctx->x11_lib.XInternAtom(ctx->display, "CLIPBOARD", False);
  ctx->selection_primary = XA_PRIMARY;
  ctx->selection_targets =
      ctx->x11_lib.XInternAtom(ctx->display, "TARGETS", False);
  ctx->selection_incr = ctx->x11_lib.XInternAtom(ctx->display, "INCR", False);
  ctx->selection_timestamp =
      ctx->x11_lib.XInternAtom(ctx->display, "TIMESTAMP", False);
  ctx->selection_multiple =
      ctx->x11_lib.XInternAtom(ctx->display, "MULTIPLE", False);
  ctx->selection_save_targets =
      ctx->x11_lib.XInternAtom(ctx->display, "SAVE_TARGETS", False);
  ctx->utf8_string =
      ctx->x11_lib.XInternAtom(ctx->display, "UTF8_STRING", False);
  ctx->text_atom = ctx->x11_lib.XInternAtom(ctx->display, "TEXT", False);
  ctx->compound_text =
      ctx->x11_lib.XInternAtom(ctx->display, "COMPOUND_TEXT", False);
  ctx->maru_selection_property =
      ctx->x11_lib.XInternAtom(ctx->display, "MARU_SELECTION", False);
  ctx->maru_selection_targets_property =
      ctx->x11_lib.XInternAtom(ctx->display, "MARU_SELECTION_TARGETS", False);
  ctx->xdnd_aware = ctx->x11_lib.XInternAtom(ctx->display, "XdndAware", False);
  ctx->xdnd_enter = ctx->x11_lib.XInternAtom(ctx->display, "XdndEnter", False);
  ctx->xdnd_position =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndPosition", False);
  ctx->xdnd_status =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndStatus", False);
  ctx->xdnd_leave = ctx->x11_lib.XInternAtom(ctx->display, "XdndLeave", False);
  ctx->xdnd_drop = ctx->x11_lib.XInternAtom(ctx->display, "XdndDrop", False);
  ctx->xdnd_finished =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndFinished", False);
  ctx->xdnd_selection =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndSelection", False);
  ctx->xdnd_type_list =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndTypeList", False);
  ctx->xdnd_action_list =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndActionList", False);
  ctx->xdnd_action_copy =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndActionCopy", False);
  ctx->xdnd_action_move =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndActionMove", False);
  ctx->xdnd_action_link =
      ctx->x11_lib.XInternAtom(ctx->display, "XdndActionLink", False);
  ctx->compositor_supports_extended_frame_sync = false;
  if (ctx->net_supported != None && ctx->net_wm_frame_drawn != None &&
      ctx->net_wm_frame_timings != None) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char *prop = NULL;
    if (ctx->x11_lib.XGetWindowProperty(
            ctx->display, ctx->root, ctx->net_supported, 0, 4096, False,
            XA_ATOM, &actual_type, &actual_format, &item_count, &bytes_after,
            &prop) == Success) {
      if (prop && actual_type == XA_ATOM && actual_format == 32) {
        const Atom *atoms = (const Atom *)prop;
        bool has_drawn = false;
        bool has_timings = false;
        for (unsigned long i = 0; i < item_count; ++i) {
          if (atoms[i] == ctx->net_wm_frame_drawn) {
            has_drawn = true;
          } else if (atoms[i] == ctx->net_wm_frame_timings) {
            has_timings = true;
          }
        }
        ctx->compositor_supports_extended_frame_sync = has_drawn && has_timings;
      }
      ctx->x11_lib.XFree(prop);
    }
  }
  (void)_maru_x11_enable_xi2_raw_motion(ctx);
  _maru_x11_detect_pointer_barrier_support(ctx);

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    if (ctx->xim) {
      ctx->x11_lib.XCloseIM(ctx->xim);
      ctx->xim = NULL;
    }
    ctx->x11_lib.XCloseDisplay(ctx->display);
    maru_unload_xfixes_symbols(&ctx->xfixes_lib);
    maru_unload_xrandr_symbols(&ctx->xrandr_lib);
    maru_unload_xshape_symbols(&ctx->xshape_lib);
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
    maru_unload_xss_symbols(&ctx->xss_lib);
    maru_unload_x11_symbols(&ctx->x11_lib);
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->base.pub.flags = 0;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.inhibit_idle = create_info->attributes.inhibit_idle;
  ctx->xss_idle_inhibit_active = false;
  _maru_x11_apply_idle_inhibit(ctx);

  if (!_maru_linux_common_run(&ctx->linux_common)) {
    maru_destroyContext_X11((MARU_Context *)ctx);
    return MARU_FAILURE;
  }

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

void maru_destroyContext_X11(MARU_Context *context) {
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

  MARU_X11DataOffer *clipboard_offer =
      _maru_x11_get_offer(ctx, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD);
  MARU_X11DataOffer *primary_offer =
      _maru_x11_get_offer(ctx, MARU_LINUX_PRIVATE_TARGET_PRIMARY);
  MARU_X11DataOffer *dnd_offer =
      _maru_x11_get_offer(ctx, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);

  _maru_x11_clear_offer(ctx, clipboard_offer);
  _maru_x11_clear_offer(ctx, primary_offer);
  _maru_x11_clear_offer(ctx, dnd_offer);

  _maru_x11_clear_dnd_session(ctx);
  _maru_x11_clear_dnd_source_session(ctx, false, MARU_DROP_ACTION_NONE);

  if (ctx->display) {
    if (ctx->xss_idle_inhibit_active && ctx->xss_lib.base.available &&
        ctx->xss_lib.XScreenSaverSuspend) {
      ctx->xss_lib.XScreenSaverSuspend(ctx->display, False);
      ctx->xss_idle_inhibit_active = false;
      ctx->x11_lib.XFlush(ctx->display);
    }
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
        (MARU_Monitor_Base *)
            ctx->base.monitor_cache[ctx->base.monitor_cache_count - 1u];
    monitor->is_active = false;
    monitor->pub.flags |= MARU_MONITOR_STATE_LOST;
    ctx->base.monitor_cache_count--;
    maru_releaseMonitor_X11((MARU_Monitor *)monitor);
  }

  maru_unload_xfixes_symbols(&ctx->xfixes_lib);
  maru_unload_xrandr_symbols(&ctx->xrandr_lib);
  maru_unload_xshape_symbols(&ctx->xshape_lib);
  maru_unload_xcursor_symbols(&ctx->xcursor_lib);
  maru_unload_xi2_symbols(&ctx->xi2_lib);
  maru_unload_xss_symbols(&ctx->xss_lib);
  maru_unload_x11_symbols(&ctx->x11_lib);

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
}

MARU_Status maru_wakeContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  if (maru_isContextLost(context)) {
    return MARU_CONTEXT_LOST;
  }
  uint64_t val = 1;
  if (write(ctx->linux_common.worker.event_fd, &val, sizeof(val)) >= 0) {
    return MARU_SUCCESS;
  }
  return MARU_FAILURE;
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

uint64_t _maru_x11_get_monotonic_time_ms(void) {
  const uint64_t ns = _maru_linux_get_monotonic_time_ns();
  if (ns == 0) {
    return 0;
  }
  return ns / 1000000ull;
}

MARU_Scalar _maru_x11_scale_from_metrics(MARU_Scalar pixel_width,
                                         MARU_Scalar pixel_height,
                                         MARU_Scalar mm_width,
                                         MARU_Scalar mm_height) {
  if (pixel_width <= (MARU_Scalar)0.0 || pixel_height <= (MARU_Scalar)0.0) {
    return (MARU_Scalar)0.0;
  }
  if (mm_width <= (MARU_Scalar)0.0 && mm_height <= (MARU_Scalar)0.0) {
    return (MARU_Scalar)0.0;
  }

  MARU_Scalar dpi_x = (MARU_Scalar)0.0;
  MARU_Scalar dpi_y = (MARU_Scalar)0.0;
  if (mm_width > (MARU_Scalar)0.0) {
    dpi_x = pixel_width * (MARU_Scalar)25.4 / mm_width;
  }
  if (mm_height > (MARU_Scalar)0.0) {
    dpi_y = pixel_height * (MARU_Scalar)25.4 / mm_height;
  }

  MARU_Scalar dpi = dpi_x;
  if (dpi <= (MARU_Scalar)0.0 || (dpi_y > (MARU_Scalar)0.0 && dpi_y > dpi)) {
    dpi = dpi_y;
  }
  if (dpi <= (MARU_Scalar)0.0) {
    return (MARU_Scalar)0.0;
  }

  const MARU_Scalar scale = dpi / (MARU_Scalar)96.0;
  if (scale <= (MARU_Scalar)0.0) {
    return (MARU_Scalar)0.0;
  }
  return scale;
}

static const char *_maru_x11_find_xft_dpi_value(const char *resource_text) {
  if (!resource_text) {
    return NULL;
  }

  const char *line = resource_text;
  while (*line != '\0') {
    while (*line == '\n' || *line == '\r') {
      ++line;
    }
    if (*line == '\0') {
      break;
    }
    const char *line_end = line;
    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
      ++line_end;
    }

    const char *cursor = line;
    while (cursor < line_end &&
           (*cursor == ' ' || *cursor == '\t' || *cursor == '!')) {
      ++cursor;
    }
    if ((line_end - cursor) >= 8 && strncmp(cursor, "Xft.dpi:", 8) == 0) {
      cursor += 8;
      while (cursor < line_end && isspace((unsigned char)*cursor)) {
        ++cursor;
      }
      if (cursor < line_end) {
        return cursor;
      }
    }
    line = line_end;
  }
  return NULL;
}

MARU_Scalar _maru_x11_get_global_scale(MARU_Context_X11 *ctx) {
  MARU_ASSUME(ctx != NULL);
  MARU_ASSUME(ctx->display != NULL);

  const Atom resource_manager =
      ctx->x11_lib.XInternAtom(ctx->display, "RESOURCE_MANAGER", False);
  if (resource_manager != None) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char *prop = NULL;
    if (ctx->x11_lib.XGetWindowProperty(
            ctx->display, ctx->root, resource_manager, 0, 1L << 20, False,
            AnyPropertyType, &actual_type, &actual_format, &item_count,
            &bytes_after, &prop) == Success) {
      if (prop && actual_format == 8 && item_count > 0) {
        char *resource_text =
            (char *)maru_context_alloc(&ctx->base, (size_t)item_count + 1u);
        if (resource_text) {
          memcpy(resource_text, prop, item_count);
          resource_text[item_count] = '\0';

          const char *dpi_text = _maru_x11_find_xft_dpi_value(resource_text);
          if (dpi_text) {
            char *end = NULL;
            const double dpi = strtod(dpi_text, &end);
            if (end != dpi_text && dpi > 0.0) {
              maru_context_free(&ctx->base, resource_text);
              ctx->x11_lib.XFree(prop);
              return (MARU_Scalar)(dpi / 96.0);
            }
          }
          maru_context_free(&ctx->base, resource_text);
        }
      }
      if (prop) {
        ctx->x11_lib.XFree(prop);
      }
    }
  }

  const MARU_Scalar fallback = _maru_x11_scale_from_metrics(
      (MARU_Scalar)DisplayWidth(ctx->display, ctx->screen),
      (MARU_Scalar)DisplayHeight(ctx->display, ctx->screen),
      (MARU_Scalar)DisplayWidthMM(ctx->display, ctx->screen),
      (MARU_Scalar)DisplayHeightMM(ctx->display, ctx->screen));
  return (fallback > (MARU_Scalar)0.0) ? fallback : (MARU_Scalar)1.0;
}

static int _maru_x11_compute_poll_timeout_ms(MARU_Context_X11 *ctx,
                                             uint32_t timeout_ms) {
  const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
  uint32_t effective_timeout = timeout_ms;
  if (now_ms != 0u) {
    effective_timeout =
        _maru_adjust_timeout_for_cursor_animation(&ctx->base, timeout_ms, now_ms);
  }

  if (effective_timeout == MARU_NEVER) {
    return -1;
  }
  if (effective_timeout > (uint32_t)INT_MAX) {
    return INT_MAX;
  }
  return (int)effective_timeout;
}

void _maru_x11_process_event(MARU_Context_X11 *ctx, XEvent *ev) {
  if (ev->type == GenericEvent && ctx->xi2_raw_motion_enabled &&
      ev->xcookie.extension == ctx->xi2_opcode &&
      ctx->x11_lib.XGetEventData(ctx->display, &ev->xcookie)) {
    if (ev->xcookie.evtype == XI_RawMotion && ctx->locked_window &&
        (ctx->locked_window->base.pub.cursor_mode == MARU_CURSOR_LOCKED)) {
      MARU_Window_X11 *locked_window = ctx->locked_window;
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
          if (locked_window->lock_pointer_barriers_active) {
            unsigned int state = 0u;
            Window root_ret = 0;
            Window child_ret = 0;
            int root_x = 0;
            int root_y = 0;
            int win_x = 0;
            int win_y = 0;
            (void)ctx->x11_lib.XQueryPointer(
                ctx->display, locked_window->handle, &root_ret, &child_ret,
                &root_x, &root_y, &win_x, &win_y, &state);

            MARU_Event mevt = {0};
            mevt.mouse_moved.dip_position.x =
                (MARU_Scalar)ctx->linux_common.pointer.x;
            mevt.mouse_moved.dip_position.y =
                (MARU_Scalar)ctx->linux_common.pointer.y;
            mevt.mouse_moved.dip_delta.x = raw_dx;
            mevt.mouse_moved.dip_delta.y = raw_dy;
            mevt.mouse_moved.raw_dip_delta.x = raw_dx;
            mevt.mouse_moved.raw_dip_delta.y = raw_dy;
            mevt.mouse_moved.modifiers = _maru_x11_get_modifiers(state);
            ctx->linux_common.pointer.focused_window =
                (MARU_Window *)locked_window;
            _maru_x11_clear_locked_raw_accum(ctx);
            _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED,
                                 (MARU_Window *)locked_window, &mevt);
          } else {
            ctx->locked_raw_dx_accum += raw_dx;
            ctx->locked_raw_dy_accum += raw_dy;
            ctx->locked_raw_pending = true;
          }
        }
      }
    }
    ctx->x11_lib.XFreeEventData(ctx->display, &ev->xcookie);
  }

  if (_maru_x11_process_window_event(ctx, ev))
    return;
  if (_maru_x11_process_input_event(ctx, ev))
    return;
  if (_maru_x11_process_dataexchange_event(ctx, ev))
    return;
}

static bool _maru_x11_pop_next_event(MARU_Context_X11 *ctx, XEvent *out_ev) {
  if (ctx->has_buffered_event) {
    *out_ev = ctx->buffered_event;
    ctx->has_buffered_event = false;
    memset(&ctx->buffered_event, 0, sizeof(ctx->buffered_event));
    return true;
  }
  if (!ctx->x11_lib.XPending(ctx->display)) {
    return false;
  }
  ctx->x11_lib.XNextEvent(ctx->display, out_ev);
  return true;
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms,
                                MARU_EventMask mask,
                                MARU_EventCallback callback, void *userdata) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  const uint64_t pump_start_ns = _maru_linux_get_monotonic_time_ns();
  MARU_PumpContext pump_ctx = {.mask = mask, .callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;
  ctx->linux_common.controller_snapshot_dirty = true;
  _maru_x11_clear_mime_query_cache(ctx);

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_drain_queued_events(&ctx->base);
  _maru_x11_dispatch_pending_frames(ctx);

  {
    XEvent ev;
    while (_maru_x11_pop_next_event(ctx, &ev)) {
      if (ctx->x11_lib.XFilterEvent(&ev, None))
        continue;
      _maru_x11_process_event(ctx, &ev);
    }
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0u) {
      _maru_advance_animated_cursors(&ctx->base, now_ms);
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

  uint32_t ctrl_count = _maru_linux_common_fill_pollfds(&ctx->linux_common,
                                                        &pfds[nfds], 32 - nfds);
  uint32_t ctrl_start_idx = nfds;
  nfds += ctrl_count;

  int poll_timeout = _maru_x11_compute_poll_timeout_ms(ctx, timeout_ms);
  int ret = poll(pfds, nfds, poll_timeout);

  if (ret > 0) {
    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      if (read(ctx->linux_common.worker.event_fd, &val, sizeof(val)) < 0) {
      }
    }

    if (pfds[1].revents & POLLIN) {
    }

    if (ctrl_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common,
                                         &pfds[ctrl_start_idx], ctrl_count);
    }
  }

  {
    XEvent ev;
    while (_maru_x11_pop_next_event(ctx, &ev)) {
      if (ctx->x11_lib.XFilterEvent(&ev, None))
        continue;
      _maru_x11_process_event(ctx, &ev);
    }
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0u) {
      _maru_advance_animated_cursors(&ctx->base, now_ms);
    }
  }

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_x11_dispatch_pending_frames(ctx);
  ctx->base.pump_ctx = NULL;

  return MARU_SUCCESS;
}

void *maru_getContextNativeHandle_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  return ctx->display;
}
