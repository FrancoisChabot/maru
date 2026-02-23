#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

void _maru_wayland_dispatch_window_resized(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (!maru_isWindowReady((MARU_Window *)window)) {
    return;
  }

  MARU_Event evt = {0};
  maru_getWindowGeometry_WL((MARU_Window *)window, &evt.resized.geometry);
  _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window *)window, &evt);
}

static void _maru_wayland_apply_viewport_size(MARU_Window_WL *window) {
  if (!window || !window->ext.viewport) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const bool disabled = (window->viewport_size.x <= (MARU_Scalar)0.0 ||
                         window->viewport_size.y <= (MARU_Scalar)0.0);
  if (disabled) {
    maru_wp_viewport_set_destination(ctx, window->ext.viewport, -1, -1);
    return;
  }

  int32_t dst_w = (int32_t)window->viewport_size.x;
  int32_t dst_h = (int32_t)window->viewport_size.y;
  if (dst_w <= 0 || dst_h <= 0) {
    return;
  }
  maru_wp_viewport_set_destination(ctx, window->ext.viewport, dst_w, dst_h);
}

static void _maru_wayland_apply_size_constraints(MARU_Window_WL *window) {
  if (!window) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  int32_t min_w = (int32_t)window->min_size.x;
  int32_t min_h = (int32_t)window->min_size.y;
  int32_t max_w = (int32_t)window->max_size.x;
  int32_t max_h = (int32_t)window->max_size.y;

  if (!window->is_resizable) {
    int32_t fixed_w = (int32_t)window->size.x;
    int32_t fixed_h = (int32_t)window->size.y;
    if (fixed_w > 0 && fixed_h > 0) {
      min_w = fixed_w;
      min_h = fixed_h;
      max_w = fixed_w;
      max_h = fixed_h;
    }
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
    maru_libdecor_frame_set_min_content_size(ctx, window->libdecor.frame, min_w, min_h);
    maru_libdecor_frame_set_max_content_size(ctx, window->libdecor.frame, max_w, max_h);
  } else if (window->xdg.toplevel) {
    maru_xdg_toplevel_set_min_size(ctx, window->xdg.toplevel, min_w, min_h);
    maru_xdg_toplevel_set_max_size(ctx, window->xdg.toplevel, max_w, max_h);
  }
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

  if (width > 0 && height > 0) {
    const MARU_Scalar new_width = (MARU_Scalar)width;
    const MARU_Scalar new_height = (MARU_Scalar)height;
    if (window->size.x != new_width || window->size.y != new_height) {
      window->size.x = new_width;
      window->size.y = new_height;
      _maru_wayland_dispatch_window_resized(window);
    }
  }

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

  if (window->is_maximized != is_maximized) {
    window->is_maximized = is_maximized;
    if (is_maximized) {
      window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    } else {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
    }

    MARU_Event evt = {0};
    evt.maximized.maximized = is_maximized;
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_MAXIMIZED, (MARU_Window *)window, &evt);
  }

  window->is_fullscreen = is_fullscreen;
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
  window->base.pub.event_mask = create_info->attributes.event_mask;
  window->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  window->base.pub.current_cursor = create_info->attributes.cursor;
  window->size = create_info->attributes.logical_size;
  window->min_size = create_info->attributes.min_size;
  window->max_size = create_info->attributes.max_size;
  window->scale = (MARU_Scalar)1.0;
  window->viewport_size = create_info->attributes.viewport_size;
  window->text_input_rect = create_info->attributes.text_input_rect;
  window->aspect_ratio = create_info->attributes.aspect_ratio;
  window->text_input_type = create_info->attributes.text_input_type;
  window->decor_mode = ctx->decor_mode;
  window->is_maximized = create_info->attributes.maximized;
  window->is_fullscreen = create_info->attributes.fullscreen;
  window->is_resizable = create_info->attributes.resizable;
  window->is_decorated = create_info->decorated;
  window->accepts_drop = create_info->attributes.accept_drop;
  window->primary_selection = create_info->attributes.primary_selection;

  if (window->is_maximized) {
    window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
  }
  if (window->is_fullscreen) {
    window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  }
  if (window->is_resizable) {
    window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }
  if (window->is_decorated) {
    window->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  if (create_info->attributes.mouse_passthrough) {
    window->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
  }
  
  #ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  window->base.backend = &maru_backend_WL;
  #endif

  if (create_info->attributes.title) {
    size_t len = strlen(create_info->attributes.title);
    window->base.title = maru_context_alloc(&ctx->base, len + 1);
    if (window->base.title) {
      memcpy(window->base.title, create_info->attributes.title, len + 1);
      window->base.pub.title = window->base.title;
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
  if (window->base.title) maru_context_free(&ctx->base, window->base.title);
  maru_context_free(&ctx->base, window);
  return MARU_FAILURE;
}

MARU_Status maru_updateWindow_WL(MARU_Window *window_handle, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
      if (window->base.title) {
          maru_context_free(&ctx->base, window->base.title);
          window->base.title = NULL;
      }
      window->base.pub.title = NULL;

      if (attributes->title) {
          size_t len = strlen(attributes->title);
          window->base.title = maru_context_alloc(&ctx->base, len + 1);
          if (window->base.title) {
              memcpy(window->base.title, attributes->title, len + 1);
              window->base.pub.title = window->base.title;
          }
      }

      if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
          _maru_wayland_libdecor_set_title(window, attributes->title ? attributes->title : "");
      } else if (window->xdg.toplevel) {
          maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel,
                                      attributes->title ? attributes->title : "");
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
      window->is_fullscreen = attributes->fullscreen;
      if (attributes->fullscreen) {
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
      window->size = attributes->logical_size;
      if (window->xdg.surface) {
          maru_xdg_surface_set_window_geometry(ctx, window->xdg.surface, 0, 0,
                                               (int32_t)window->size.x, (int32_t)window->size.y);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_MIN_SIZE) {
      window->min_size = attributes->min_size;
  }

  if (field_mask & MARU_WINDOW_ATTR_MAX_SIZE) {
      window->max_size = attributes->max_size;
  }

  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
      window->aspect_ratio = attributes->aspect_ratio;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Wayland backend does not enforce aspect ratio constraints yet");
  }

  if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
      window->is_maximized = attributes->maximized;
      if (attributes->maximized) {
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
      window->is_resizable = attributes->resizable;
      if (attributes->resizable) {
          window->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
      } else {
          window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
      if (attributes->mouse_passthrough) {
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
      if ((field_mask & MARU_WINDOW_ATTR_FULLSCREEN) && attributes->fullscreen && window->xdg.toplevel) {
          struct wl_output *output = NULL;
          if (attributes->monitor && maru_getMonitorContext(attributes->monitor) == window->base.pub.context) {
              MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)attributes->monitor;
              output = monitor->output;
          }
          maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, output);
      } else {
          MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                                 "Monitor targeting is only honored for fullscreen on Wayland");
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
      window->accepts_drop = attributes->accept_drop;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Drop acceptance filtering is not implemented on Wayland");
  }

  if (field_mask & MARU_WINDOW_ATTR_PRIMARY_SELECTION) {
      window->primary_selection = attributes->primary_selection;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Primary selection toggling is not implemented on Wayland");
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
      window->text_input_type = attributes->text_input_type;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Text input type hints are not implemented on Wayland");
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_RECT) {
      window->text_input_rect = attributes->text_input_rect;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Text input rectangle hints are not implemented on Wayland");
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
      window->base.pub.current_cursor = attributes->cursor;
  }
  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
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
      window->viewport_size = attributes->viewport_size;
      _maru_wayland_apply_viewport_size(window);
  }

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
      window->base.pub.event_mask = attributes->event_mask;
  }

  if (field_mask & (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE | MARU_WINDOW_ATTR_RESIZABLE)) {
      _maru_wayland_apply_size_constraints(window);
  }

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

  if (window->base.title) maru_context_free(&ctx->base, window->base.title);
  maru_context_free(&ctx->base, window);

  return MARU_SUCCESS;
}

void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_WL *window = (const MARU_Window_WL *)window_handle;
  memset(out_geometry, 0, sizeof(MARU_WindowGeometry));
  out_geometry->logical_size = window->size;
  out_geometry->scale = (window->scale > (MARU_Scalar)0) ? window->scale : (MARU_Scalar)1.0;
  out_geometry->pixel_size.x = (int32_t)(window->size.x * out_geometry->scale);
  out_geometry->pixel_size.y = (int32_t)(window->size.y * out_geometry->scale);
}

void *maru_getWindowNativeHandle_WL(MARU_Window *window) {
  MARU_Window_WL *win = (MARU_Window_WL *)window;
  return win->wl.surface;
}
