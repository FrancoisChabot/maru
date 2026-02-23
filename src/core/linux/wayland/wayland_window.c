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
  MARU_Window_WL *window = (MARU_Window_WL *)data;

  if (width > 0 && height > 0) {
    const MARU_Scalar new_width = (MARU_Scalar)width;
    const MARU_Scalar new_height = (MARU_Scalar)height;
    if (window->size.x != new_width || window->size.y != new_height) {
      window->size.x = new_width;
      window->size.y = new_height;
      _maru_wayland_dispatch_window_resized(window);
    }
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
  window->base.pub.context = context;
  window->base.pub.event_mask = create_info->attributes.event_mask;
  window->size = create_info->attributes.logical_size;
  window->scale = (MARU_Scalar)1.0;
  window->viewport_size = create_info->attributes.viewport_size;
  window->decor_mode = ctx->decor_mode;
  
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
