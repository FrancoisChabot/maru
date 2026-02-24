#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void _maru_wayland_update_text_input(MARU_Window_WL *window);

void _maru_wayland_dispatch_window_resized(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (!maru_isWindowReady((MARU_Window *)window)) {
    window->pending_resized_event = true;
    return;
  }

  window->pending_resized_event = false;
  MARU_Event evt = {0};
  maru_getWindowGeometry_WL((MARU_Window *)window, &evt.resized.geometry);
  _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window *)window, &evt);
}

static void _maru_wayland_apply_viewport_size(MARU_Window_WL *window) {
  if (!window || !window->ext.viewport) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const MARU_Vec2Dip viewport_size = window->base.attrs_effective.viewport_size;
  const bool disabled = (viewport_size.x <= (MARU_Scalar)0.0 ||
                         viewport_size.y <= (MARU_Scalar)0.0);
  if (disabled) {
    maru_wp_viewport_set_destination(ctx, window->ext.viewport, -1, -1);
    return;
  }

  int32_t dst_w = (int32_t)viewport_size.x;
  int32_t dst_h = (int32_t)viewport_size.y;
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
  const MARU_WindowAttributes *attrs = &window->base.attrs_effective;
  if (!width || !height || !window) return;
  if (*width == 0 || *height == 0) return;
  if (attrs->aspect_ratio.num == 0 || attrs->aspect_ratio.denom == 0) return;

  uint32_t min_width = attrs->min_size.x > 0 ? (uint32_t)attrs->min_size.x : 0u;
  uint32_t min_height = attrs->min_size.y > 0 ? (uint32_t)attrs->min_size.y : 0u;
  uint32_t max_width = attrs->max_size.x > 0 ? (uint32_t)attrs->max_size.x : UINT32_MAX;
  uint32_t max_height = attrs->max_size.y > 0 ? (uint32_t)attrs->max_size.y : UINT32_MAX;

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
  const MARU_Scalar old_width = attrs->logical_size.x;
  const MARU_Scalar old_height = attrs->logical_size.y;
  bool size_changed = false;
  int32_t min_w = (int32_t)attrs->min_size.x;
  int32_t min_h = (int32_t)attrs->min_size.y;
  int32_t max_w = (int32_t)attrs->max_size.x;
  int32_t max_h = (int32_t)attrs->max_size.y;

  if ((window->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) == 0) {
    int32_t fixed_w = (int32_t)attrs->logical_size.x;
    int32_t fixed_h = (int32_t)attrs->logical_size.y;
    if (fixed_w > 0 && fixed_h > 0) {
      min_w = fixed_w;
      min_h = fixed_h;
      max_w = fixed_w;
      max_h = fixed_h;
    }
  }

  if (((window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) == 0) &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) == 0)) {
    uint32_t constrained_w = (uint32_t)((attrs->logical_size.x > 0) ? attrs->logical_size.x : 1);
    uint32_t constrained_h = (uint32_t)((attrs->logical_size.y > 0) ? attrs->logical_size.y : 1);
    _maru_wayland_enforce_aspect_ratio(&constrained_w, &constrained_h, window);
    attrs->logical_size.x = (MARU_Scalar)constrained_w;
    attrs->logical_size.y = (MARU_Scalar)constrained_h;
    size_changed = (attrs->logical_size.x != old_width || attrs->logical_size.y != old_height);
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
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
                                         (int32_t)attrs->logical_size.x, (int32_t)attrs->logical_size.y);
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) == 0) &&
      ((window->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) == 0)) {
    struct libdecor_state *state = maru_libdecor_state_new(
        ctx, (int)attrs->logical_size.x, (int)attrs->logical_size.y);
    if (state) {
      maru_libdecor_frame_commit(ctx, window->libdecor.frame, state, window->libdecor.last_configuration);
      maru_libdecor_state_free(ctx, state);
    }
  }

  if (size_changed) {
    window->pending_resized_event = true;
  }

  maru_wl_surface_commit(ctx, window->wl.surface);
}

static void _maru_wayland_apply_mouse_passthrough(MARU_Window_WL *window) {
  if (!window || !window->wl.surface) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if ((window->base.pub.flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH) != 0) {
    struct wl_region *empty_region = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
    if (empty_region) {
      maru_wl_surface_set_input_region(ctx, window->wl.surface, empty_region);
      maru_wl_region_destroy(ctx, empty_region);
    }
  } else {
    maru_wl_surface_set_input_region(ctx, window->wl.surface, NULL);
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
  (void)data;
  (void)surface;
  (void)output;
}

static void _wl_surface_handle_leave(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  (void)data;
  (void)surface;
  (void)output;
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
  (void)data;
  (void)surface;
  (void)transform;
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
  const uint32_t *state = NULL;
  wl_array_for_each(state, states) {
    if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) {
      is_maximized = true;
    } else if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
      is_fullscreen = true;
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
    if (effective->logical_size.x != new_width || effective->logical_size.y != new_height) {
      effective->logical_size.x = new_width;
      effective->logical_size.y = new_height;
      _maru_wayland_dispatch_window_resized(window);
    }
  }

  if (effective->maximized != is_maximized) {
    effective->maximized = is_maximized;
    if (is_maximized) {
      window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    } else {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
    }

    MARU_Event evt = {0};
    evt.maximized.maximized = is_maximized;
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_MAXIMIZED, (MARU_Window *)window, &evt);
  }

  effective->fullscreen = is_fullscreen;
  if (is_fullscreen) {
    window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  } else {
    window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
  }
}

static void _xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Event evt = {0};
  _maru_dispatch_event(&ctx->base, MARU_CLOSE_REQUESTED, (MARU_Window *)window, &evt);
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
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_READY, (MARU_Window *)window, &evt);
  }

  // TODO: If window has a pending buffer, commit it here.
  // For now, we just commit to acknowledge the configuration.
  maru_wl_surface_commit(ctx, window->wl.surface);
}

static const struct xdg_surface_listener _xdg_surface_listener = {
    .configure = _xdg_surface_handle_configure,
};

static void _wl_frame_callback_done(void *data, struct wl_callback *callback,
                                    uint32_t callback_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (!window) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  window->wl.frame_callback = NULL;
  maru_wl_callback_destroy(ctx, callback);

  MARU_Event evt = {0};
  evt.frame.timestamp_ms = callback_data;
  _maru_dispatch_event(&ctx->base, MARU_WINDOW_FRAME, (MARU_Window *)window, &evt);
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

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_SSD) {
    _maru_wayland_create_ssd_decoration(window);
  }

  maru_wl_surface_commit(ctx, window->wl.surface);

  return true;
}

MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_Window_WL *window = (MARU_Window_WL *)maru_context_alloc(&ctx->base, sizeof(MARU_Window_WL));
  if (!window) {
    return MARU_FAILURE;
  }

  memset(window, 0, sizeof(MARU_Window_WL));
  window->base.ctx_base = &ctx->base;
  window->base.pub.userdata = create_info->userdata;
  window->base.pub.context = context;
  window->base.pub.metrics = &window->base.metrics;
  window->base.pub.keyboard_state = window->base.keyboard_state;
  window->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  window->base.attrs_requested = create_info->attributes;
  window->base.attrs_effective = create_info->attributes;
  window->base.attrs_dirty_mask = MARU_WINDOW_ATTR_ALL;
  window->base.pub.event_mask = window->base.attrs_effective.event_mask;
  window->base.pub.cursor_mode = window->base.attrs_effective.cursor_mode;
  window->base.pub.current_cursor = window->base.attrs_effective.cursor;
  window->scale = (MARU_Scalar)1.0;

  window->decor_mode = ctx->decor_mode;
  window->pending_resized_event = true;

  if (window->base.attrs_effective.maximized) {
    window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
  }
  if (window->base.attrs_effective.fullscreen) {
    window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  }
  if (window->base.attrs_effective.resizable) {
    window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }
  if (create_info->decorated) {
    window->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  if (window->base.attrs_effective.mouse_passthrough) {
    window->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
  }
  
  #ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  window->base.backend = &maru_backend_WL;
  #endif

  if (create_info->attributes.title) {
    size_t len = strlen(create_info->attributes.title);
    window->base.title_storage = maru_context_alloc(&ctx->base, len + 1);
    if (window->base.title_storage) {
      memcpy(window->base.title_storage, create_info->attributes.title, len + 1);
      window->base.attrs_requested.title = window->base.title_storage;
      window->base.attrs_effective.title = window->base.title_storage;
      window->base.pub.title = window->base.title_storage;
    } else {
      window->base.attrs_requested.title = NULL;
      window->base.attrs_effective.title = NULL;
      window->base.pub.title = NULL;
    }
  }

  if (create_info->attributes.surrounding_text) {
    size_t len = strlen(create_info->attributes.surrounding_text);
    window->base.surrounding_text_storage = maru_context_alloc(&ctx->base, len + 1);
    if (window->base.surrounding_text_storage) {
      memcpy(window->base.surrounding_text_storage, create_info->attributes.surrounding_text, len + 1);
      window->base.attrs_requested.surrounding_text = window->base.surrounding_text_storage;
      window->base.attrs_effective.surrounding_text = window->base.surrounding_text_storage;
    } else {
      window->base.attrs_requested.surrounding_text = NULL;
      window->base.attrs_effective.surrounding_text = NULL;
    }
  }

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

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
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
  _maru_wayland_apply_mouse_passthrough(window);
  _maru_wayland_update_text_input(window);
  window->base.attrs_dirty_mask = 0;

  _maru_register_window(&ctx->base, (MARU_Window *)window);

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;

cleanup_surface:
  if (window->ext.viewport) {
    maru_wp_viewport_destroy(ctx, window->ext.viewport);
    window->ext.viewport = NULL;
  }
  maru_wl_surface_destroy(ctx, window->wl.surface);
cleanup_window:
  if (window->base.title_storage) maru_context_free(&ctx->base, window->base.title_storage);
  if (window->base.surrounding_text_storage) maru_context_free(&ctx->base, window->base.surrounding_text_storage);
  maru_context_free(&ctx->base, window);
  return MARU_FAILURE;
}

void _maru_wayland_update_text_input(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  struct wl_seat *seat = ctx->wl.seat ? ctx->wl.seat : ctx->protocols.opt.wl_seat;

  if (!ctx->protocols.opt.zwp_text_input_manager_v3 || !seat) {
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
        ctx, window->ext.text_input, (int32_t)window->base.attrs_effective.text_input_rect.origin.x,
        (int32_t)window->base.attrs_effective.text_input_rect.origin.y,
        (int32_t)window->base.attrs_effective.text_input_rect.size.x,
        (int32_t)window->base.attrs_effective.text_input_rect.size.y);
    
    if (window->base.attrs_effective.surrounding_text) {
        maru_zwp_text_input_v3_set_surrounding_text(ctx, window->ext.text_input, 
            window->base.attrs_effective.surrounding_text,
            (int32_t)window->base.attrs_effective.surrounding_cursor_offset, 
            (int32_t)window->base.attrs_effective.surrounding_anchor_offset);
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

  window->base.attrs_dirty_mask |= field_mask;

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
      if (window->base.title_storage) {
          maru_context_free(&ctx->base, window->base.title_storage);
          window->base.title_storage = NULL;
      }
      requested->title = NULL;
      effective->title = NULL;
      window->base.pub.title = NULL;

      if (attributes->title) {
          size_t len = strlen(attributes->title);
          window->base.title_storage = maru_context_alloc(&ctx->base, len + 1);
          if (window->base.title_storage) {
              memcpy(window->base.title_storage, attributes->title, len + 1);
              requested->title = window->base.title_storage;
              effective->title = window->base.title_storage;
              window->base.pub.title = window->base.title_storage;
          }
      }

      if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
          _maru_wayland_libdecor_set_title(window, requested->title ? requested->title : "");
      } else if (window->xdg.toplevel) {
          maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel,
                                      requested->title ? requested->title : "");
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
      requested->fullscreen = attributes->fullscreen;
      effective->fullscreen = attributes->fullscreen;
      if (requested->fullscreen) {
          window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
          if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
              maru_libdecor_frame_set_fullscreen(ctx, window->libdecor.frame, NULL);
          } else if (window->xdg.toplevel) {
              maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, NULL);
          }
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
          if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
              maru_libdecor_frame_unset_fullscreen(ctx, window->libdecor.frame);
          } else if (window->xdg.toplevel) {
              maru_xdg_toplevel_unset_fullscreen(ctx, window->xdg.toplevel);
          }
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
      requested->logical_size = attributes->logical_size;
      effective->logical_size = attributes->logical_size;
      if (window->xdg.surface) {
          maru_xdg_surface_set_window_geometry(ctx, window->xdg.surface, 0, 0,
                                               (int32_t)effective->logical_size.x, (int32_t)effective->logical_size.y);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_MIN_SIZE) {
      requested->min_size = attributes->min_size;
      effective->min_size = attributes->min_size;
  }

  if (field_mask & MARU_WINDOW_ATTR_MAX_SIZE) {
      requested->max_size = attributes->max_size;
      effective->max_size = attributes->max_size;
  }

  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
      requested->aspect_ratio = attributes->aspect_ratio;
      effective->aspect_ratio = attributes->aspect_ratio;
  }

  if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
      requested->maximized = attributes->maximized;
      effective->maximized = attributes->maximized;
      if (requested->maximized) {
          window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
          if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
              maru_libdecor_frame_set_maximized(ctx, window->libdecor.frame);
          } else if (window->xdg.toplevel) {
              maru_xdg_toplevel_set_maximized(ctx, window->xdg.toplevel);
          }
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
          if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
              maru_libdecor_frame_unset_maximized(ctx, window->libdecor.frame);
          } else if (window->xdg.toplevel) {
              maru_xdg_toplevel_unset_maximized(ctx, window->xdg.toplevel);
          }
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
      requested->resizable = attributes->resizable;
      effective->resizable = attributes->resizable;
      if (requested->resizable) {
          window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
      requested->mouse_passthrough = attributes->mouse_passthrough;
      effective->mouse_passthrough = attributes->mouse_passthrough;
      if (requested->mouse_passthrough) {
          window->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MOUSE_PASSTHROUGH);
      }
      _maru_wayland_apply_mouse_passthrough(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_POSITION) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Window positioning is compositor-controlled on Wayland");
  }

  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
      requested->monitor = attributes->monitor;
      effective->monitor = attributes->monitor;
      if ((field_mask & MARU_WINDOW_ATTR_FULLSCREEN) && requested->fullscreen && window->xdg.toplevel) {
          struct wl_output *output = NULL;
          if (requested->monitor && maru_getMonitorContext(requested->monitor) == window->base.pub.context) {
              MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)requested->monitor;
              output = monitor->output;
          }
          maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, output);
      } else {
          MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                                 "Monitor targeting is only honored for fullscreen on Wayland");
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
      requested->text_input_type = attributes->text_input_type;
      effective->text_input_type = attributes->text_input_type;
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_RECT) {
      requested->text_input_rect = attributes->text_input_rect;
      effective->text_input_rect = attributes->text_input_rect;
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT) {
      if (window->base.surrounding_text_storage) {
          maru_context_free(&ctx->base, window->base.surrounding_text_storage);
          window->base.surrounding_text_storage = NULL;
      }
      requested->surrounding_text = NULL;
      effective->surrounding_text = NULL;
      if (attributes->surrounding_text) {
          size_t len = strlen(attributes->surrounding_text);
          window->base.surrounding_text_storage = maru_context_alloc(&ctx->base, len + 1);
          if (window->base.surrounding_text_storage) {
              memcpy(window->base.surrounding_text_storage, attributes->surrounding_text, len + 1);
              requested->surrounding_text = window->base.surrounding_text_storage;
              effective->surrounding_text = window->base.surrounding_text_storage;
          }
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET) {
      requested->surrounding_cursor_offset = attributes->surrounding_cursor_offset;
      requested->surrounding_anchor_offset = attributes->surrounding_anchor_offset;
      effective->surrounding_cursor_offset = attributes->surrounding_cursor_offset;
      effective->surrounding_anchor_offset = attributes->surrounding_anchor_offset;
  }

  if (field_mask & (MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_TEXT_INPUT_RECT |
                    MARU_WINDOW_ATTR_SURROUNDING_TEXT | MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET)) {
      _maru_wayland_update_text_input(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
      requested->cursor = attributes->cursor;
      effective->cursor = attributes->cursor;
      window->base.pub.current_cursor = attributes->cursor;
  }
  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
      requested->cursor_mode = attributes->cursor_mode;
      effective->cursor_mode = attributes->cursor_mode;
      if (window->base.pub.cursor_mode != attributes->cursor_mode) {
          window->base.pub.cursor_mode = attributes->cursor_mode;
          _maru_wayland_update_cursor_mode(window);
      }
  }

  if ((field_mask & MARU_WINDOW_ATTR_CURSOR) || (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE)) {
      if (ctx->linux_common.pointer.focused_window == window_handle) {
          _maru_wayland_update_cursor(ctx, window, ctx->linux_common.pointer.enter_serial);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_VIEWPORT_SIZE) {
      requested->viewport_size = attributes->viewport_size;
      effective->viewport_size = attributes->viewport_size;
      _maru_wayland_apply_viewport_size(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
      requested->event_mask = attributes->event_mask;
      effective->event_mask = attributes->event_mask;
      window->base.pub.event_mask = attributes->event_mask;
  }

  if (field_mask & (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE |
                    MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE)) {
      _maru_wayland_apply_size_constraints(window);
  }

  window->base.attrs_dirty_mask &= ~field_mask;

  return MARU_SUCCESS;
}

void _maru_wayland_update_cursor_mode(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  MARU_CursorMode mode = window->base.pub.cursor_mode;

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
      }
    } else {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, 
                             "Pointer constraints protocol missing or no pointer available");
    }

    if (ctx->protocols.opt.zwp_relative_pointer_manager_v1 && ctx->wl.pointer) {
      window->ext.relative_pointer = maru_zwp_relative_pointer_manager_v1_get_relative_pointer(
          ctx, ctx->protocols.opt.zwp_relative_pointer_manager_v1, ctx->wl.pointer);

      if (window->ext.relative_pointer) {
        maru_zwp_relative_pointer_v1_add_listener(ctx, window->ext.relative_pointer,
                                                  &_maru_wayland_relative_pointer_listener,
                                                  window);
      }
    } else {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, 
                             "Relative pointer protocol missing or no pointer available");
    }
  }
}

MARU_Status maru_destroyWindow_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

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

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
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

  if (window->base.title_storage) maru_context_free(&ctx->base, window->base.title_storage);
  if (window->base.surrounding_text_storage) maru_context_free(&ctx->base, window->base.surrounding_text_storage);
  maru_context_free(&ctx->base, window);

  return MARU_SUCCESS;
}

void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_WL *window = (const MARU_Window_WL *)window_handle;
  memset(out_geometry, 0, sizeof(MARU_WindowGeometry));
  out_geometry->logical_size = window->base.attrs_effective.logical_size;
  out_geometry->scale = (window->scale > (MARU_Scalar)0) ? window->scale : (MARU_Scalar)1.0;
  out_geometry->pixel_size.x = (int32_t)(out_geometry->logical_size.x * out_geometry->scale);
  out_geometry->pixel_size.y = (int32_t)(out_geometry->logical_size.y * out_geometry->scale);
}

void *maru_getWindowNativeHandle_WL(MARU_Window *window) {
  MARU_Window_WL *win = (MARU_Window_WL *)window;
  return win->wl.surface;
}
