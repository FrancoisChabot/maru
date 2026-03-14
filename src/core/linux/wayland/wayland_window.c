#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input-event-codes.h>

void _maru_wayland_update_text_input(MARU_Window_WL *window);

static MARU_Monitor_WL *_maru_wayland_find_monitor_by_output(MARU_Context_WL *ctx,
                                                             struct wl_output *output) {
  MARU_ASSUME(ctx != NULL);
  MARU_ASSUME(output != NULL);
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
    if (monitor && monitor->output == output) {
      return monitor;
    }
  }
  return NULL;
}

static bool _maru_wayland_window_add_monitor(MARU_Window_WL *window, MARU_Monitor_WL *monitor) {
  MARU_ASSUME(window != NULL);
  MARU_ASSUME(monitor != NULL);

  for (uint32_t i = 0; i < window->monitor_count; ++i) {
    if (window->monitors[i] == (MARU_Monitor *)monitor) {
      return true;
    }
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->monitor_count >= window->monitor_capacity) {
    const uint32_t old_capacity = window->monitor_capacity;
    const uint32_t new_capacity = old_capacity ? old_capacity * 2u : 4u;
    MARU_Monitor **new_list = (MARU_Monitor **)maru_context_realloc(
        &ctx->base, window->monitors, old_capacity * sizeof(MARU_Monitor *),
        new_capacity * sizeof(MARU_Monitor *));
    if (!new_list) {
      return false;
    }
    window->monitors = new_list;
    window->monitor_capacity = new_capacity;
  }

  maru_retainMonitor((MARU_Monitor *)monitor);
  window->monitors[window->monitor_count++] = (MARU_Monitor *)monitor;
  return true;
}

void _maru_wayland_window_drop_monitor(MARU_Window_WL *window, MARU_Monitor *monitor) {
  if (!window || !monitor) {
    return;
  }

  for (uint32_t i = 0; i < window->monitor_count; ++i) {
    if (window->monitors[i] != monitor) {
      continue;
    }

    for (uint32_t j = i; j + 1 < window->monitor_count; ++j) {
      window->monitors[j] = window->monitors[j + 1];
    }
    window->monitor_count--;
    maru_releaseMonitor(monitor);
    break;
  }

  if (window->base.attrs_requested.monitor == monitor) {
    window->base.attrs_requested.monitor = NULL;
  }
  if (window->base.attrs_effective.monitor == monitor) {
    window->base.attrs_effective.monitor = NULL;
  }
}

void _maru_wayland_dispatch_window_resized(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (!maru_isWindowReady((MARU_Window *)window)) {
    window->pending_resized_event = true;
    return;
  }

  window->pending_resized_event = false;
  MARU_Event evt = {0};
  maru_getWindowGeometry_WL((MARU_Window *)window, &evt.window_resized.geometry);
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)window, &evt);
}

static void _maru_wayland_apply_viewport_size(MARU_Window_WL *window) {
  if (!window) {
    return;
  }

  const MARU_Vec2Dip dip_viewport_size = window->base.attrs_effective.dip_viewport_size;
  const bool disabled = (dip_viewport_size.x <= (MARU_Scalar)0.0 ||
                         dip_viewport_size.y <= (MARU_Scalar)0.0);

  if (!window->ext.viewport) {
    if (!disabled && !window->missing_viewporter_reported) {
      MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "wp_viewporter unavailable; viewport size requests are ignored");
      window->missing_viewporter_reported = true;
    }
    return;
  }

  // Capability is present; clear one-shot warning latch.
  window->missing_viewporter_reported = false;

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (disabled) {
    maru_wp_viewport_set_destination(ctx, window->ext.viewport, -1, -1);
    return;
  }

  int32_t dst_w = (int32_t)dip_viewport_size.x;
  int32_t dst_h = (int32_t)dip_viewport_size.y;
  if (dst_w <= 0 || dst_h <= 0) {
    return;
  }
  maru_wp_viewport_set_destination(ctx, window->ext.viewport, dst_w, dst_h);
}

static uint32_t _maru_wayland_clamp_size(uint32_t value, uint32_t min_value, uint32_t max_value) {
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

void _maru_wayland_enforce_aspect_ratio(uint32_t *width, uint32_t *height,
                                        const MARU_Window_WL *window) {
  if (!width || !height || !window) return;
  const MARU_WindowAttributes *attrs = &window->base.attrs_effective;
  if (*width == 0 || *height == 0) return;
  if (attrs->aspect_ratio.num == 0 || attrs->aspect_ratio.denom == 0) return;

  uint32_t min_width = attrs->dip_min_size.x > 0 ? (uint32_t)attrs->dip_min_size.x : 0u;
  uint32_t min_height = attrs->dip_min_size.y > 0 ? (uint32_t)attrs->dip_min_size.y : 0u;
  uint32_t max_width = attrs->dip_max_size.x > 0 ? (uint32_t)attrs->dip_max_size.x : UINT32_MAX;
  uint32_t max_height = attrs->dip_max_size.y > 0 ? (uint32_t)attrs->dip_max_size.y : UINT32_MAX;

  if (max_width < min_width) max_width = min_width;
  if (max_height < min_height) max_height = min_height;

  const uint64_t ratio_num = (uint32_t)attrs->aspect_ratio.num;
  const uint64_t ratio_den = (uint32_t)attrs->aspect_ratio.denom;

  for (int pass = 0; pass < 2; ++pass) {
    uint64_t lhs = (uint64_t)(*width) * ratio_den;
    uint64_t rhs = (uint64_t)(*height) * ratio_num;

    if (lhs > rhs) {
      uint64_t adjusted = ((uint64_t)(*height) * ratio_num + (ratio_den / 2u)) / ratio_den;
      *width = (uint32_t)(adjusted > 0 ? adjusted : 1u);
    } else if (lhs < rhs) {
      uint64_t adjusted = ((uint64_t)(*width) * ratio_den + (ratio_num / 2u)) / ratio_num;
      *height = (uint32_t)(adjusted > 0 ? adjusted : 1u);
    }

    *width = _maru_wayland_clamp_size(*width, min_width, max_width);
    *height = _maru_wayland_clamp_size(*height, min_height, max_height);
  }
}

static void _maru_wayland_apply_size_constraints(MARU_Window_WL *window) {
  if (!window) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_WindowAttributes *attrs = &window->base.attrs_effective;
  const MARU_Scalar old_width = attrs->dip_size.x;
  const MARU_Scalar old_height = attrs->dip_size.y;
  bool size_changed = false;
  int32_t min_w = (int32_t)attrs->dip_min_size.x;
  int32_t min_h = (int32_t)attrs->dip_min_size.y;
  int32_t max_w = (int32_t)attrs->dip_max_size.x;
  int32_t max_h = (int32_t)attrs->dip_max_size.y;

  if ((window->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) == 0) {
    int32_t fixed_w = (int32_t)attrs->dip_size.x;
    int32_t fixed_h = (int32_t)attrs->dip_size.y;
    if (fixed_w > 0 && fixed_h > 0) {
      min_w = fixed_w;
      min_h = fixed_h;
      max_w = fixed_w;
      max_h = fixed_h;
    }
  }

  if (((window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) == 0) &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) == 0)) {
    uint32_t constrained_w = (uint32_t)((attrs->dip_size.x > 0) ? attrs->dip_size.x : 1);
    uint32_t constrained_h = (uint32_t)((attrs->dip_size.y > 0) ? attrs->dip_size.y : 1);
    _maru_wayland_enforce_aspect_ratio(&constrained_w, &constrained_h, window);
    attrs->dip_size.x = (MARU_Scalar)constrained_w;
    attrs->dip_size.y = (MARU_Scalar)constrained_h;
    size_changed = (attrs->dip_size.x != old_width || attrs->dip_size.y != old_height);
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD && window->libdecor.frame) {
    maru_libdecor_frame_set_min_content_size(ctx, window->libdecor.frame, min_w, min_h);
    maru_libdecor_frame_set_max_content_size(ctx, window->libdecor.frame, max_w, max_h);

    struct xdg_toplevel *xdg_toplevel =
        maru_libdecor_frame_get_xdg_toplevel(ctx, window->libdecor.frame);
    if (xdg_toplevel) {
      maru_xdg_toplevel_set_min_size(ctx, xdg_toplevel, min_w, min_h);
      maru_xdg_toplevel_set_max_size(ctx, xdg_toplevel, max_w, max_h);
    }
  } else if (window->xdg.toplevel) {
    maru_xdg_toplevel_set_min_size(ctx, window->xdg.toplevel, min_w, min_h);
    maru_xdg_toplevel_set_max_size(ctx, window->xdg.toplevel, max_w, max_h);
  }

  if (window->xdg.surface) {
    maru_xdg_surface_set_window_geometry(ctx, window->xdg.surface, 0, 0,
                                         (int32_t)attrs->dip_size.x, (int32_t)attrs->dip_size.y);
  }

  _maru_wayland_update_opaque_region(window);

  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD && window->libdecor.frame &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) == 0) &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) == 0)) {
    struct libdecor_state *state = maru_libdecor_state_new(
        ctx, (int)attrs->dip_size.x, (int)attrs->dip_size.y);
    if (state) {
      maru_libdecor_frame_commit(ctx, window->libdecor.frame, state, NULL);
      maru_libdecor_state_free(ctx, state);
    }
  }

  if (size_changed) {
    window->pending_resized_event = true;
  }

  maru_wl_surface_commit(ctx, window->wl.surface);
}

static uint32_t _maru_wayland_map_content_type(MARU_ContentType type) {
  switch (type) {
    case MARU_CONTENT_TYPE_PHOTO:
      return WP_CONTENT_TYPE_V1_TYPE_PHOTO;
    case MARU_CONTENT_TYPE_VIDEO:
      return WP_CONTENT_TYPE_V1_TYPE_VIDEO;
    case MARU_CONTENT_TYPE_GAME:
      return WP_CONTENT_TYPE_V1_TYPE_GAME;
    case MARU_CONTENT_TYPE_NONE:
    default:
      return WP_CONTENT_TYPE_V1_TYPE_NONE;
  }
}

static void _fractional_scale_handle_preferred_scale(
    void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale) {
  (void)fractional_scale;
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (!window || scale == 0) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_Scalar new_scale = (MARU_Scalar)scale / (MARU_Scalar)120.0;
  if (window->scale == new_scale) {
    return;
  }

  window->scale = new_scale;
  maru_wl_surface_set_buffer_scale(ctx, window->wl.surface, 1);
  _maru_wayland_dispatch_window_resized(window);
}

const struct wp_fractional_scale_v1_listener _maru_wayland_fractional_scale_listener = {
    .preferred_scale = _fractional_scale_handle_preferred_scale,
};

static void _wl_surface_handle_enter(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (!window || !output) {
    return;
  }
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_Monitor_WL *monitor = _maru_wayland_find_monitor_by_output(ctx, output);
  if (!monitor) {
    return;
  }
  (void)_maru_wayland_window_add_monitor(window, monitor);
  (void)surface;
}

static void _wl_surface_handle_leave(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (!window || !output) {
    return;
  }
  for (uint32_t i = 0; i < window->monitor_count; ++i) {
    MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)window->monitors[i];
    if (monitor && monitor->output == output) {
      _maru_wayland_window_drop_monitor(window, (MARU_Monitor *)monitor);
      break;
    }
  }
  (void)surface;
}

static void _wl_surface_handle_preferred_buffer_scale(void *data,
                                                      struct wl_surface *surface,
                                                      int32_t factor) {
  (void)surface;
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (!window || factor <= 0) {
    return;
  }

  if (window->ext.fractional_scale) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_Scalar new_scale = (MARU_Scalar)factor;
  if (window->scale == new_scale) {
    return;
  }

  window->scale = new_scale;
  maru_wl_surface_set_buffer_scale(ctx, window->wl.surface, factor);
  _maru_wayland_dispatch_window_resized(window);
}

static void _wl_surface_handle_preferred_buffer_transform(
    void *data, struct wl_surface *surface, uint32_t transform) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  (void)surface;
  if (!window) {
    return;
  }

  MARU_BufferTransform new_transform = MARU_BUFFER_TRANSFORM_NORMAL;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_90:
      new_transform = MARU_BUFFER_TRANSFORM_90;
      break;
    case WL_OUTPUT_TRANSFORM_180:
      new_transform = MARU_BUFFER_TRANSFORM_180;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      new_transform = MARU_BUFFER_TRANSFORM_270;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      new_transform = MARU_BUFFER_TRANSFORM_FLIPPED;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      new_transform = MARU_BUFFER_TRANSFORM_FLIPPED_90;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      new_transform = MARU_BUFFER_TRANSFORM_FLIPPED_180;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      new_transform = MARU_BUFFER_TRANSFORM_FLIPPED_270;
      break;
    case WL_OUTPUT_TRANSFORM_NORMAL:
    default:
      new_transform = MARU_BUFFER_TRANSFORM_NORMAL;
      break;
  }

  if (window->preferred_buffer_transform == new_transform) {
    return;
  }

  window->preferred_buffer_transform = new_transform;
  _maru_wayland_dispatch_window_resized(window);
}

const struct wl_surface_listener _maru_wayland_surface_listener = {
    .enter = _wl_surface_handle_enter,
    .leave = _wl_surface_handle_leave,
    .preferred_buffer_scale = _wl_surface_handle_preferred_buffer_scale,
    .preferred_buffer_transform = _wl_surface_handle_preferred_buffer_transform,
};

static void _xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                           int32_t width, int32_t height,
                                           struct wl_array *states) {
  (void)xdg_toplevel;
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_WindowAttributes *effective = &window->base.attrs_effective;

  bool is_maximized = false;
  bool is_fullscreen = false;
  bool is_activated = false;
  const uint32_t *state = NULL;
  wl_array_for_each(state, states) {
    if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) {
      is_maximized = true;
    } else if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
      is_fullscreen = true;
    } else if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
      is_activated = true;
    }
  }

  if (width > 0 && height > 0) {
    uint32_t pending_width = (uint32_t)width;
    uint32_t pending_height = (uint32_t)height;
    if (!is_maximized && !is_fullscreen) {
      _maru_wayland_enforce_aspect_ratio(&pending_width, &pending_height, window);
    }
    const MARU_Scalar new_width = (MARU_Scalar)pending_width;
    const MARU_Scalar new_height = (MARU_Scalar)pending_height;
    if (effective->dip_size.x != new_width || effective->dip_size.y != new_height) {
      effective->dip_size.x = new_width;
      effective->dip_size.y = new_height;
      _maru_wayland_dispatch_window_resized(window);
    }
  }

  _maru_wayland_update_opaque_region(window);

  // xdg-shell has no explicit minimized state; many compositors signal it via
  // an inactive 0x0 configure.
  const bool inferred_minimized =
      (width == 0 && height == 0 && !is_activated && !is_maximized &&
       !is_fullscreen);

  if (effective->presentation_state !=
      (inferred_minimized ? MARU_WINDOW_PRESENTATION_MINIMIZED
                          : (is_fullscreen ? MARU_WINDOW_PRESENTATION_FULLSCREEN
                                           : (is_maximized
                                                  ? MARU_WINDOW_PRESENTATION_MAXIMIZED
                                                  : MARU_WINDOW_PRESENTATION_NORMAL)))) {
    uint32_t changed = 0;
    const bool was_visible =
        (window->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;

    if (inferred_minimized) {
      effective->presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;
      window->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      effective->visible = false;
      if (was_visible) {
        changed |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
      }
      changed |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
    } else {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
      if (is_fullscreen) {
        effective->presentation_state = MARU_WINDOW_PRESENTATION_FULLSCREEN;
        window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
        changed |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
      } else if (is_maximized) {
        effective->presentation_state = MARU_WINDOW_PRESENTATION_MAXIMIZED;
        window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
        changed |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
      } else {
        effective->presentation_state = MARU_WINDOW_PRESENTATION_NORMAL;
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
        changed |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
      }

      window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      effective->visible = true;
      if (!was_visible) {
        changed |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
      }
    }
    _maru_wayland_dispatch_state_changed(window, changed);
  }
}

static void _xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Event evt = {0};
  _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)window, &evt);
}

static const struct xdg_toplevel_listener _xdg_toplevel_listener = {
    .configure = _xdg_toplevel_handle_configure,
    .close = _xdg_toplevel_handle_close,
};

static void _xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface,
                                          uint32_t serial) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  maru_xdg_surface_ack_configure(ctx, xdg_surface, serial);

  if (!maru_isWindowReady((MARU_Window *)window)) {
    window->base.pub.flags |= MARU_WINDOW_STATE_READY;
    MARU_Event evt = {0};
    maru_getWindowGeometry_WL((MARU_Window *)window, &evt.window_ready.geometry);
    _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_READY, (MARU_Window *)window, &evt);
  }

  // Maru does not stage/own window content buffers; applications/renderers do.
  // This commit acknowledges configure and applies pending surface state only.
  maru_wl_surface_commit(ctx, window->wl.surface);
}

static const struct xdg_surface_listener _xdg_surface_listener = {
    .configure = _xdg_surface_handle_configure,
};

static void _wl_frame_callback_done(void *data, struct wl_callback *callback,
                                    uint32_t callback_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_ASSUME(window != NULL);

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  window->wl.frame_callback = NULL;
  maru_wl_callback_destroy(ctx, callback);

  MARU_Event evt = {0};
  evt.window_frame.timestamp_ms = callback_data;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME, (MARU_Window *)window, &evt);
}

static const struct wl_callback_listener _maru_wayland_frame_listener = {
    .done = _wl_frame_callback_done,
};

void _maru_wayland_request_frame(MARU_Window_WL *window) {
  if (!window || !window->wl.surface || window->wl.frame_callback) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  window->wl.frame_callback = maru_wl_surface_frame(ctx, window->wl.surface);
  if (!window->wl.frame_callback) {
    return;
  }

  maru_wl_callback_add_listener(ctx, window->wl.frame_callback,
                                &_maru_wayland_frame_listener, window);
  maru_wl_surface_commit(ctx, window->wl.surface);
}

bool _maru_wayland_create_xdg_shell_objects(MARU_Window_WL *window,
                                             const MARU_WindowCreateInfo *create_info) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const MARU_WindowAttributes *attrs = &window->base.attrs_effective;

  window->xdg.surface = maru_xdg_wm_base_get_xdg_surface(ctx, ctx->protocols.xdg_wm_base, window->wl.surface);
  if (!window->xdg.surface) {
    return false;
  }

  maru_xdg_surface_add_listener(ctx, window->xdg.surface, &_xdg_surface_listener, window);

  window->xdg.toplevel = maru_xdg_surface_get_toplevel(ctx, window->xdg.surface);
  if (!window->xdg.toplevel) {
    return false;
  }

  maru_xdg_toplevel_add_listener(ctx, window->xdg.toplevel, &_xdg_toplevel_listener, window);

  if (create_info->attributes.title) {
    maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel, create_info->attributes.title);
  }

  if (create_info->app_id) {
    maru_xdg_toplevel_set_app_id(ctx, window->xdg.toplevel, create_info->app_id);
  }

  if (attrs->presentation_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
    struct wl_output *output = NULL;
    if (attrs->monitor &&
        maru_getMonitorContext(attrs->monitor) == window->base.pub.context) {
      MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)attrs->monitor;
      output = monitor->output;
    }
    maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, output);
  } else if (attrs->presentation_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
    maru_xdg_toplevel_set_maximized(ctx, window->xdg.toplevel);
  } else if (attrs->presentation_state == MARU_WINDOW_PRESENTATION_MINIMIZED ||
             !attrs->visible) {
    maru_xdg_toplevel_set_minimized(ctx, window->xdg.toplevel);
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_SSD) {
    _maru_wayland_create_ssd_decoration(window);
  }

  maru_wl_surface_commit(ctx, window->wl.surface);

  return true;
}

MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_Window_WL *window = (MARU_Window_WL *)maru_context_zalloc(&ctx->base, sizeof(MARU_Window_WL));
  if (!window) {
    return MARU_FAILURE;
  }

  window->base.ctx_base = &ctx->base;
  window->base.pub.userdata = create_info->userdata;
  window->base.pub.context = context;
  window->base.attrs_requested = create_info->attributes;
  window->base.attrs_effective = create_info->attributes;
  window->base.attrs_dirty_mask = MARU_WINDOW_ATTR_ALL;
  window->base.pub.cursor_mode = window->base.attrs_effective.cursor_mode;
  window->base.pub.current_cursor = window->base.attrs_effective.cursor;
  window->scale = (MARU_Scalar)1.0;

  window->decor_mode = ctx->decor_mode;
  if (!create_info->has_decorations &&
      window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD) {
    window->decor_mode = MARU_WAYLAND_DECORATION_STRATEGY_NONE;
  }
  window->pending_resized_event = true;
  window->preferred_buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
  maru_getWindowGeometry_WL((MARU_Window *)window, NULL);

  switch (window->base.attrs_effective.presentation_state) {
  case MARU_WINDOW_PRESENTATION_FULLSCREEN:
    window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
    window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    break;
  case MARU_WINDOW_PRESENTATION_MAXIMIZED:
    window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    break;
  case MARU_WINDOW_PRESENTATION_MINIMIZED:
    window->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
    window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
    break;
  case MARU_WINDOW_PRESENTATION_NORMAL:
    if (window->base.attrs_effective.visible) {
      window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    }
    break;
  }
  if (!window->base.attrs_effective.visible) {
    window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
    if (window->base.attrs_effective.presentation_state ==
        MARU_WINDOW_PRESENTATION_NORMAL) {
      window->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
    }
  }
  if (window->base.attrs_effective.resizable) {
    window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }
  if (create_info->has_decorations) {
    window->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  
  #ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  window->base.backend = &maru_backend_WL;
  #endif

  _maru_update_window_base(&window->base, MARU_WINDOW_ATTR_TITLE | MARU_WINDOW_ATTR_SURROUNDING_TEXT, &create_info->attributes);

  window->wl.surface = maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);
  if (!window->wl.surface) {
    goto cleanup_window;
  }
  maru_wl_surface_set_buffer_scale(ctx, window->wl.surface, 1);
  maru_wl_surface_set_user_data(ctx, window->wl.surface, window);
  maru_wl_surface_add_listener(ctx, window->wl.surface, &_maru_wayland_surface_listener, window);
  if (ctx->protocols.opt.wp_content_type_manager_v1) {
    window->ext.content_type = maru_wp_content_type_manager_v1_get_surface_content_type(
        ctx, ctx->protocols.opt.wp_content_type_manager_v1, window->wl.surface);
    if (window->ext.content_type) {
      maru_wp_content_type_v1_set_content_type(
          ctx, window->ext.content_type, _maru_wayland_map_content_type(create_info->content_type));
    }
  }
  if (ctx->protocols.opt.wp_viewporter) {
    window->ext.viewport = maru_wp_viewporter_get_viewport(ctx, ctx->protocols.opt.wp_viewporter, window->wl.surface);
    _maru_wayland_apply_viewport_size(window);
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD) {
    if (!_maru_wayland_create_libdecor_frame(window, create_info)) {
        goto cleanup_surface;
    }
  } else {
    if (!_maru_wayland_create_xdg_shell_objects(window, create_info)) {
      goto cleanup_surface;
    }
  }

  if (ctx->protocols.opt.wp_fractional_scale_manager_v1) {
    window->ext.fractional_scale = maru_wp_fractional_scale_manager_v1_get_fractional_scale(
        ctx, ctx->protocols.opt.wp_fractional_scale_manager_v1, window->wl.surface);
    if (window->ext.fractional_scale) {
      maru_wp_fractional_scale_v1_add_listener(
          ctx, window->ext.fractional_scale, &_maru_wayland_fractional_scale_listener, window);
    }
  }

  _maru_wayland_apply_size_constraints(window);
  _maru_wayland_update_idle_inhibitor(window);
  _maru_wayland_update_text_input(window);
  window->base.attrs_dirty_mask = 0;

  _maru_register_window(&ctx->base, (MARU_Window *)window);

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;

cleanup_surface:
  if (window->ext.fractional_scale) {
    maru_wp_fractional_scale_v1_destroy(ctx, window->ext.fractional_scale);
    window->ext.fractional_scale = NULL;
  }
  if (window->ext.content_type) {
    maru_wp_content_type_v1_destroy(ctx, window->ext.content_type);
    window->ext.content_type = NULL;
  }
  if (window->wl.frame_callback) {
    maru_wl_callback_destroy(ctx, window->wl.frame_callback);
    window->wl.frame_callback = NULL;
  }
  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD) {
    _maru_wayland_destroy_libdecor_frame(window);
  } else {
    _maru_wayland_destroy_ssd_decoration(window);
  }
  if (window->xdg.toplevel) {
    maru_xdg_toplevel_destroy(ctx, window->xdg.toplevel);
    window->xdg.toplevel = NULL;
  }
  if (window->xdg.surface) {
    maru_xdg_surface_destroy(ctx, window->xdg.surface);
    window->xdg.surface = NULL;
  }
  if (window->ext.viewport) {
    maru_wp_viewport_destroy(ctx, window->ext.viewport);
    window->ext.viewport = NULL;
  }
  if (window->wl.surface) {
    maru_wl_surface_destroy(ctx, window->wl.surface);
    window->wl.surface = NULL;
  }
cleanup_window:
  maru_context_free(&ctx->base, window->base.title_storage);
  maru_context_free(&ctx->base, window->base.surrounding_text_storage);
  maru_context_free(&ctx->base, window);
  return MARU_FAILURE;
}

void _maru_wayland_update_text_input(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  struct wl_seat *seat = ctx->wl.seat ? ctx->wl.seat : ctx->protocols.opt.wl_seat;

  if (!ctx->protocols.opt.zwp_text_input_manager_v3) {
    const MARU_WindowAttributes *attrs = &window->base.attrs_effective;
    const bool ime_requested =
        (attrs->text_input_type != MARU_TEXT_INPUT_TYPE_NONE) ||
        (attrs->surrounding_text != NULL) ||
        (attrs->dip_text_input_rect.size.x > (MARU_Scalar)0.0) ||
        (attrs->dip_text_input_rect.size.y > (MARU_Scalar)0.0);
    if (ime_requested && !window->missing_text_input_v3_reported) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "zwp_text_input_manager_v3 unavailable; IME features running in fallback mode");
      window->missing_text_input_v3_reported = true;
    }
    return;
  }

  // Protocol is present; clear one-shot warning latch.
  window->missing_text_input_v3_reported = false;

  if (!seat) {
    return;
  }

  if (!window->ext.text_input) {
    window->ext.text_input = maru_zwp_text_input_manager_v3_get_text_input(
        ctx, ctx->protocols.opt.zwp_text_input_manager_v3, seat);
    if (!window->ext.text_input) {
      return;
    }
    maru_zwp_text_input_v3_add_listener(ctx, window->ext.text_input,
                                        &_maru_wayland_text_input_listener, window);
  }

  if (window->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE) {
    uint32_t hint = ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE;
    uint32_t purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;

    switch (window->base.attrs_effective.text_input_type) {
      case MARU_TEXT_INPUT_TYPE_PASSWORD:
        hint = ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT |
               ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
        purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
        break;
      case MARU_TEXT_INPUT_TYPE_EMAIL:
        purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
        break;
      case MARU_TEXT_INPUT_TYPE_NUMERIC:
        purpose = ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
        break;
      default:
        break;
    }

    maru_zwp_text_input_v3_enable(ctx, window->ext.text_input);
    maru_zwp_text_input_v3_set_content_type(ctx, window->ext.text_input, hint, purpose);
    maru_zwp_text_input_v3_set_cursor_rectangle(
        ctx, window->ext.text_input, (int32_t)window->base.attrs_effective.dip_text_input_rect.position.x,
        (int32_t)window->base.attrs_effective.dip_text_input_rect.position.y,
        (int32_t)window->base.attrs_effective.dip_text_input_rect.size.x,
        (int32_t)window->base.attrs_effective.dip_text_input_rect.size.y);
    
    if (window->base.attrs_effective.surrounding_text) {
        maru_zwp_text_input_v3_set_surrounding_text(ctx, window->ext.text_input, 
            window->base.attrs_effective.surrounding_text,
            (int32_t)window->base.attrs_effective.surrounding_cursor_byte, 
            (int32_t)window->base.attrs_effective.surrounding_anchor_byte);
    }

    maru_zwp_text_input_v3_commit(ctx, window->ext.text_input);
  } else {
    maru_zwp_text_input_v3_disable(ctx, window->ext.text_input);
    maru_zwp_text_input_v3_commit(ctx, window->ext.text_input);
  }
}

MARU_Status maru_updateWindow_WL(MARU_Window *window_handle, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_WindowAttributes *requested = &window->base.attrs_requested;
  MARU_WindowAttributes *effective = &window->base.attrs_effective;
  MARU_Status status = MARU_SUCCESS;
  uint32_t state_changed_mask = 0u;

  _maru_update_window_base(&window->base, field_mask, attributes);

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
      if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD) {
          _maru_wayland_libdecor_set_title(window, effective->title ? effective->title : "");
      } else if (window->xdg.toplevel) {
          maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel,
                                      effective->title ? effective->title : "");
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
    const MARU_WindowPresentationState requested_state = attributes->presentation_state;
    const MARU_WindowPresentationState old_state = requested->presentation_state;

    // _maru_update_window_base already updated effective->presentation_state
    // but we need to apply it to Wayland objects.

    if (requested_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
      window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
      window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      effective->visible = true;

      struct wl_output *output = NULL;
      if (requested->monitor &&
          maru_getMonitorContext(requested->monitor) ==
              window->base.pub.context) {
        MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)requested->monitor;
        output = monitor->output;
      }

      if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD &&
          window->libdecor.frame) {
        maru_libdecor_frame_set_fullscreen(ctx, window->libdecor.frame, output);
      } else if (window->xdg.toplevel) {
        maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, output);
      }
    } else if (requested_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
      window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
      window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      effective->visible = true;

      if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD &&
          window->libdecor.frame) {
        if (old_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
          maru_libdecor_frame_unset_fullscreen(ctx, window->libdecor.frame);
        }
        maru_libdecor_frame_set_maximized(ctx, window->libdecor.frame);
      } else if (window->xdg.toplevel) {
        if (old_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
          maru_xdg_toplevel_unset_fullscreen(ctx, window->xdg.toplevel);
        }
        maru_xdg_toplevel_set_maximized(ctx, window->xdg.toplevel);
      }
    } else if (requested_state == MARU_WINDOW_PRESENTATION_MINIMIZED) {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
      window->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      effective->visible = false;

      struct xdg_toplevel *toplevel = window->xdg.toplevel;
      if (!toplevel &&
          window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD &&
          window->libdecor.frame) {
        toplevel =
            maru_libdecor_frame_get_xdg_toplevel(ctx, window->libdecor.frame);
      }
      if (toplevel) {
        maru_xdg_toplevel_set_minimized(ctx, toplevel);
      } else {
        MARU_REPORT_DIAGNOSTIC(
            (MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
            "Window minimization is unavailable on this compositor setup");
        status = MARU_FAILURE;
      }
    } else {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);

      if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD &&
          window->libdecor.frame) {
        if (old_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
          maru_libdecor_frame_unset_fullscreen(ctx, window->libdecor.frame);
        } else if (old_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
          maru_libdecor_frame_unset_maximized(ctx, window->libdecor.frame);
        }
      } else if (window->xdg.toplevel) {
        if (old_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
          maru_xdg_toplevel_unset_fullscreen(ctx, window->xdg.toplevel);
        } else if (old_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
          maru_xdg_toplevel_unset_maximized(ctx, window->xdg.toplevel);
        }
      }
    }

    if (old_state != requested_state) {
      state_changed_mask |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_SIZE) {
      if (window->xdg.surface) {
          maru_xdg_surface_set_window_geometry(ctx, window->xdg.surface, 0, 0,
                                               (int32_t)effective->dip_size.x, (int32_t)effective->dip_size.y);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
      const bool was_resizable = (window->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
      if (effective->resizable) {
          window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
      }
      if (was_resizable != effective->resizable) {
          state_changed_mask |= MARU_WINDOW_STATE_CHANGED_RESIZABLE;
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_POSITION) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Window positioning is compositor-controlled on Wayland");
      status = MARU_FAILURE;
  }

  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
      if (effective->presentation_state == MARU_WINDOW_PRESENTATION_FULLSCREEN || (window->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) != 0) {
          struct wl_output *output = NULL;
          if (effective->monitor && maru_getMonitorContext(effective->monitor) == window->base.pub.context) {
              MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)effective->monitor;
              output = monitor->output;
          }

          if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD && window->libdecor.frame) {
              maru_libdecor_frame_set_fullscreen(ctx, window->libdecor.frame, output);
          } else if (window->xdg.toplevel) {
              maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, output);
          }
      } else {
          MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                                 "Monitor targeting requires fullscreen on Wayland");
          status = MARU_FAILURE;
      }
  }

  if (field_mask & (MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT |
                    MARU_WINDOW_ATTR_SURROUNDING_TEXT | MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE)) {
      _maru_wayland_update_text_input(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
      window->base.pub.current_cursor = effective->cursor;
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
      if (window->base.pub.cursor_mode != effective->cursor_mode) {
          MARU_CursorMode previous_mode = window->base.pub.cursor_mode;
          window->base.pub.cursor_mode = effective->cursor_mode;
          if (!_maru_wayland_update_cursor_mode(window)) {
              window->base.pub.cursor_mode = previous_mode;
              effective->cursor_mode = previous_mode;
              (void)_maru_wayland_update_cursor_mode(window);
              status = MARU_FAILURE;
          }
      }
  }

  if ((field_mask & MARU_WINDOW_ATTR_CURSOR) || (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE)) {
      if (ctx->linux_common.pointer.focused_window == window_handle) {
          _maru_wayland_update_cursor(ctx, window, ctx->linux_common.pointer.enter_serial);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
      _maru_wayland_apply_viewport_size(window);
  }

  if (field_mask & (MARU_WINDOW_ATTR_DIP_SIZE | MARU_WINDOW_ATTR_DIP_MIN_SIZE |
                    MARU_WINDOW_ATTR_DIP_MAX_SIZE | MARU_WINDOW_ATTR_ASPECT_RATIO |
                    MARU_WINDOW_ATTR_RESIZABLE)) {
      _maru_wayland_apply_size_constraints(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
      const bool was_visible = (window->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
      const bool was_minimized = (window->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;

      if (effective->visible) {
          window->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
          effective->presentation_state = MARU_WINDOW_PRESENTATION_NORMAL;
          if (!was_visible) {
              state_changed_mask |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
          }
          if (was_minimized) {
              state_changed_mask |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
          }
          const MARU_Status focus_status = maru_requestWindowFocus_WL(window_handle);
          if (focus_status != MARU_SUCCESS) {
              status = focus_status;
          }
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
          window->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
          effective->presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;
          if (was_visible) {
              state_changed_mask |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
          }
          if (!was_minimized) {
              state_changed_mask |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
          }
          struct xdg_toplevel *toplevel = window->xdg.toplevel;
          if (!toplevel && window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD && window->libdecor.frame) {
              toplevel = maru_libdecor_frame_get_xdg_toplevel(ctx, window->libdecor.frame);
          }
          if (toplevel) {
              maru_xdg_toplevel_set_minimized(ctx, toplevel);
          } else {
              MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                                     "Window minimization is unavailable on this compositor setup");
              status = MARU_FAILURE;
          }
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_ICON) {
      state_changed_mask |= MARU_WINDOW_STATE_CHANGED_ICON;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Wayland taskbar/dock icon is compositor-shell managed; window icon request stored as intent only");
  }

  if (state_changed_mask != 0u) {
      _maru_wayland_dispatch_state_changed(window, state_changed_mask);
  }

  maru_getWindowGeometry_WL(window_handle, NULL);
  window->base.attrs_dirty_mask &= ~field_mask;

  if (!_maru_wayland_flush_or_mark_lost(ctx, "wl_display_flush() failed during window update")) {
    status = MARU_CONTEXT_LOST;
  }

  return status;
}

bool _maru_wayland_update_cursor_mode(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_CursorMode mode = window->base.pub.cursor_mode;
  bool success = true;

  // Cleanup old state
  if (window->ext.relative_pointer) {
    maru_zwp_relative_pointer_v1_destroy(ctx, window->ext.relative_pointer);
    window->ext.relative_pointer = NULL;
  }
  if (window->ext.locked_pointer) {
    maru_zwp_locked_pointer_v1_destroy(ctx, window->ext.locked_pointer);
    window->ext.locked_pointer = NULL;
  }

  if (mode == MARU_CURSOR_LOCKED) {
    if (ctx->protocols.opt.zwp_pointer_constraints_v1 && ctx->wl.pointer) {
      window->ext.locked_pointer = maru_zwp_pointer_constraints_v1_lock_pointer(
          ctx, ctx->protocols.opt.zwp_pointer_constraints_v1, window->wl.surface,
          ctx->wl.pointer, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

      if (window->ext.locked_pointer) {
        maru_zwp_locked_pointer_v1_add_listener(ctx, window->ext.locked_pointer,
                                                &_maru_wayland_locked_pointer_listener,
                                                window);
      } else {
        MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                               "Pointer lock request failed");
        success = false;
      }
    } else {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, 
                             "Pointer constraints protocol missing or no pointer available");
      success = false;
    }

    if (ctx->protocols.opt.zwp_relative_pointer_manager_v1 && ctx->wl.pointer) {
      window->ext.relative_pointer = maru_zwp_relative_pointer_manager_v1_get_relative_pointer(
          ctx, ctx->protocols.opt.zwp_relative_pointer_manager_v1, ctx->wl.pointer);

      if (window->ext.relative_pointer) {
        maru_zwp_relative_pointer_v1_add_listener(ctx, window->ext.relative_pointer,
                                                  &_maru_wayland_relative_pointer_listener,
                                                  window);
      } else {
        MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                               "Relative pointer request failed");
        success = false;
      }
    } else {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, 
                             "Relative pointer protocol missing or no pointer available");
      success = false;
    }
  }

  if (!success) {
    if (window->ext.relative_pointer) {
      maru_zwp_relative_pointer_v1_destroy(ctx, window->ext.relative_pointer);
      window->ext.relative_pointer = NULL;
    }
    if (window->ext.locked_pointer) {
      maru_zwp_locked_pointer_v1_destroy(ctx, window->ext.locked_pointer);
      window->ext.locked_pointer = NULL;
    }
  }

  return success;
}

MARU_Status maru_destroyWindow_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  _maru_wayland_cancel_activation_for_window(ctx, window);

  if (ctx->linux_common.pointer.focused_window == window_handle) {
    ctx->linux_common.pointer.focused_window = NULL;
  }
  if (ctx->linux_common.xkb.focused_window == window_handle) {
    ctx->linux_common.xkb.focused_window = NULL;
  }

  _maru_unregister_window(&ctx->base, window_handle);

  if (window->ext.locked_pointer) {
    maru_zwp_locked_pointer_v1_destroy(ctx, window->ext.locked_pointer);
  }
  if (window->ext.relative_pointer) {
    maru_zwp_relative_pointer_v1_destroy(ctx, window->ext.relative_pointer);
  }
  if (window->ext.viewport) {
    maru_wp_viewport_destroy(ctx, window->ext.viewport);
    window->ext.viewport = NULL;
  }
  if (window->ext.idle_inhibitor) {
    maru_zwp_idle_inhibitor_v1_destroy(ctx, window->ext.idle_inhibitor);
    window->ext.idle_inhibitor = NULL;
  }
  if (window->ext.text_input) {
    maru_zwp_text_input_v3_destroy(ctx, window->ext.text_input);
    window->ext.text_input = NULL;
  }
  _maru_wayland_clear_text_input_pending(window);
  if (window->ext.content_type) {
    maru_wp_content_type_v1_destroy(ctx, window->ext.content_type);
    window->ext.content_type = NULL;
  }
  if (window->ext.fractional_scale) {
    maru_wp_fractional_scale_v1_destroy(ctx, window->ext.fractional_scale);
    window->ext.fractional_scale = NULL;
  }
  if (window->wl.frame_callback) {
    maru_wl_callback_destroy(ctx, window->wl.frame_callback);
    window->wl.frame_callback = NULL;
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_STRATEGY_CSD) {
    _maru_wayland_destroy_libdecor_frame(window);
  } else {
    _maru_wayland_destroy_ssd_decoration(window);
  }

  if (window->xdg.toplevel) {
    maru_xdg_toplevel_destroy(ctx, window->xdg.toplevel);
  }
  if (window->xdg.surface) {
    maru_xdg_surface_destroy(ctx, window->xdg.surface);
  }
  if (window->wl.surface) {
    maru_wl_surface_destroy(ctx, window->wl.surface);
  }

  for (uint32_t i = 0; i < window->monitor_count; ++i) {
    maru_releaseMonitor(window->monitors[i]);
  }
  maru_context_free(&ctx->base, window->monitors);
  window->monitors = NULL;
  window->monitor_count = 0;
  window->monitor_capacity = 0;

  maru_context_free(&ctx->base, window->base.title_storage);
  maru_context_free(&ctx->base, window->base.surrounding_text_storage);
  maru_context_free(&ctx->base, window);

  return MARU_SUCCESS;
}

void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_WindowGeometry geometry = {0};
  geometry.dip_size = window->base.attrs_effective.dip_size;
  geometry.scale = (window->scale > (MARU_Scalar)0) ? window->scale : (MARU_Scalar)1.0;
  geometry.px_size.x = (int32_t)(geometry.dip_size.x * geometry.scale);
  geometry.px_size.y = (int32_t)(geometry.dip_size.y * geometry.scale);
  geometry.buffer_transform = window->preferred_buffer_transform;
  window->base.pub.geometry = geometry;
  if (out_geometry) {
    *out_geometry = geometry;
  }
}

#include <errno.h>

void _maru_wayland_cancel_activation(MARU_Context_WL *ctx) {
  MARU_ASSUME(ctx != NULL);

  if (ctx->activation.token_obj) {
    maru_xdg_activation_token_v1_destroy(ctx, ctx->activation.token_obj);
    ctx->activation.token_obj = NULL;
  }
  maru_context_free(&ctx->base, ctx->activation.token_copy);
  ctx->activation.token_copy = NULL;

  ctx->activation.target_window = NULL;
  ctx->activation.pending = false;
  ctx->activation.done = false;
  ctx->activation.failed = false;
}

void _maru_wayland_cancel_activation_for_window(MARU_Context_WL *ctx,
                                                MARU_Window_WL *window) {
  if (!ctx || !window) {
    return;
  }
  if (ctx->activation.target_window == window) {
    _maru_wayland_cancel_activation(ctx);
  }
}

static void _maru_wayland_activation_token_done(void *data,
                                                struct xdg_activation_token_v1 *token_obj,
                                                const char *token) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || token_obj != ctx->activation.token_obj) {
    return;
  }

  maru_context_free(&ctx->base, ctx->activation.token_copy);
  ctx->activation.token_copy = NULL;

  size_t len = token ? strlen(token) : 0;
  char *copy = (char *)maru_context_alloc(&ctx->base, len + 1);
  if (!copy) {
    ctx->activation.failed = true;
    ctx->activation.done = true;
    return;
  }

  if (len > 0) {
    memcpy(copy, token, len);
  }
  copy[len] = '\0';
  ctx->activation.token_copy = copy;
  ctx->activation.done = true;
}

static const struct xdg_activation_token_v1_listener _maru_wayland_activation_token_listener = {
    .done = _maru_wayland_activation_token_done,
};

void _maru_wayland_check_activation(MARU_Context_WL *ctx) {
  if (!ctx || !ctx->activation.pending || !ctx->activation.done) {
    return;
  }

  MARU_Window_WL *window = ctx->activation.target_window;
  const char *token = ctx->activation.token_copy;
  bool can_activate = !ctx->activation.failed && window && window->wl.surface &&
                      ctx->protocols.opt.xdg_activation_v1 && token && token[0] != '\0';

  if (!can_activate) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           "Asynchronous activation request failed");
    _maru_wayland_cancel_activation(ctx);
    return;
  }

  maru_xdg_activation_v1_activate(ctx, ctx->protocols.opt.xdg_activation_v1, token,
                                  window->wl.surface);
  maru_wl_surface_commit(ctx, window->wl.surface);
  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           "wl_display_flush() failure while activating focus request");
  }
  _maru_wayland_cancel_activation(ctx);
}

MARU_Status maru_requestWindowFocus_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_ASSUME(ctx != NULL);
  MARU_ASSUME(window->wl.surface != NULL);

  if (!ctx->protocols.opt.xdg_activation_v1) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                           "xdg_activation_v1 is unavailable; compositor focus request unsupported");
    return MARU_FAILURE;
  }

  struct xdg_activation_token_v1 *token_obj =
      maru_xdg_activation_v1_get_activation_token(ctx, ctx->protocols.opt.xdg_activation_v1);
  if (!token_obj) {
    return MARU_FAILURE;
  }

  // Cancel any previous in-flight request and replace it with the latest one.
  _maru_wayland_cancel_activation(ctx);
  ctx->activation.token_obj = token_obj;
  ctx->activation.target_window = window;
  ctx->activation.pending = true;
  ctx->activation.done = false;
  ctx->activation.failed = false;

  maru_xdg_activation_token_v1_add_listener(ctx, token_obj,
                                            &_maru_wayland_activation_token_listener, ctx);
  maru_xdg_activation_token_v1_set_surface(ctx, token_obj, window->wl.surface);
  if (ctx->protocols.opt.wl_seat && ctx->last_interaction_serial != 0) {
    maru_xdg_activation_token_v1_set_serial(
        ctx, token_obj, ctx->last_interaction_serial, ctx->protocols.opt.wl_seat);
  }
  maru_xdg_activation_token_v1_commit(ctx, token_obj);

  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    _maru_wayland_cancel_activation(ctx);
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_ASSUME(window->wl.surface != NULL);

  _maru_wayland_request_frame(window);
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_WL(MARU_Window *window_handle) {
  return maru_requestWindowFocus_WL(window_handle);
}
