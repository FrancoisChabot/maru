#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static void _xdg_decoration_handle_configure(void *data,
                                             struct zxdg_toplevel_decoration_v1 *decoration,
                                             uint32_t mode) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  (void)decoration;
  (void)mode;
}

const struct zxdg_toplevel_decoration_v1_listener _maru_wayland_xdg_decoration_listener = {
    .configure = _xdg_decoration_handle_configure,
};

MARU_WaylandDecorationMode _maru_wayland_get_decoration_mode(const MARU_ContextCreateInfo *create_info) {
  if (create_info->tuning) {
    return create_info->tuning->wayland.decoration_mode;
  }
  return MARU_WAYLAND_DECORATION_MODE_AUTO;
}

static void _fractional_scale_handle_preferred_scale(
    void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  window->scale = scale / (MARU_Scalar)120.0;

  // When using fractional scaling, buffer scale must be 1.
  maru_wl_surface_set_buffer_scale(ctx, window->wl.surface, 1);

  if (window->base.pub.flags & MARU_WINDOW_STATE_READY) {
    MARU_Event evt = {0};
    evt.resized.geometry.logical_size = window->size;
    evt.resized.geometry.pixel_size.x = (int32_t)(window->size.x * window->scale);
    evt.resized.geometry.pixel_size.y = (int32_t)(window->size.y * window->scale);
    
    _maru_wayland_update_opaque_region(window);
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
  }
}

const struct wp_fractional_scale_v1_listener _maru_wayland_fractional_scale_listener = {
    .preferred_scale = _fractional_scale_handle_preferred_scale,
};

static uint32_t _maru_wayland_map_content_type(MARU_ContentType type) {
  switch (type) {
    case MARU_CONTENT_TYPE_NONE: return WP_CONTENT_TYPE_V1_TYPE_NONE;
    case MARU_CONTENT_TYPE_PHOTO: return WP_CONTENT_TYPE_V1_TYPE_PHOTO;
    case MARU_CONTENT_TYPE_VIDEO: return WP_CONTENT_TYPE_V1_TYPE_VIDEO;
    case MARU_CONTENT_TYPE_GAME: return WP_CONTENT_TYPE_V1_TYPE_GAME;
    default: return WP_CONTENT_TYPE_V1_TYPE_NONE;
  }
}

static void _wl_surface_handle_enter(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Monitor_WL *monitor = NULL;
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; i++) {
    if (((MARU_Monitor_WL *)ctx->base.monitor_cache[i])->output == output) {
      monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
      break;
    }
  }

  if (!monitor) return;

  if (window->monitor_count >= window->monitor_capacity) {
    uint32_t old_cap = window->monitor_capacity;
    uint32_t new_cap = old_cap ? old_cap * 2 : 4;
    window->monitors = maru_context_realloc(&ctx->base, window->monitors, old_cap * sizeof(MARU_Monitor *), new_cap * sizeof(MARU_Monitor *));
    window->monitor_capacity = new_cap;
  }
  window->monitors[window->monitor_count++] = (MARU_Monitor *)monitor;
  
  (void)surface;
}

static void _wl_surface_handle_leave(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  for (uint32_t i = 0; i < window->monitor_count; i++) {
    if (((MARU_Monitor_WL *)window->monitors[i])->output == output) {
      for (uint32_t j = i; j < window->monitor_count - 1; j++) {
        window->monitors[j] = window->monitors[j + 1];
      }
      window->monitor_count--;
      break;
    }
  }
  (void)surface;
}

static void _wl_handle_frame_done(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener _wl_frame_listener = {
    .done = _wl_handle_frame_done,
};

static void _wl_handle_frame_done(void *data, struct wl_callback *callback, uint32_t time) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (callback == window->wl.frame_callback) {
    maru_wl_callback_destroy(ctx, callback);
    window->wl.frame_callback = NULL;

    MARU_Event evt = {0};
    evt.frame.timestamp_ms = time;
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_FRAME, (MARU_Window *)window, &evt);
  }
}

void _maru_wayland_request_frame(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->wl.frame_callback) return;

  window->wl.frame_callback = maru_wl_surface_frame(ctx, window->wl.surface);
  maru_wl_callback_add_listener(ctx, window->wl.frame_callback, &_wl_frame_listener, window);
}

static void _wl_surface_handle_preferred_buffer_scale(void *data, struct wl_surface *surface,
                                                      int32_t factor) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  if (window->ext.fractional_scale) return;

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->scale != (MARU_Scalar)factor) {
    window->scale = (MARU_Scalar)factor;
    maru_wl_surface_set_buffer_scale(ctx, window->wl.surface, factor);

    if (window->base.pub.flags & MARU_WINDOW_STATE_READY) {
      MARU_Event evt = {0};
      evt.resized.geometry.logical_size = window->size;
      evt.resized.geometry.pixel_size.x = (int32_t)(window->size.x * window->scale);
      evt.resized.geometry.pixel_size.y = (int32_t)(window->size.y * window->scale);
      
      _maru_wayland_update_opaque_region(window);
      _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
    }
  }
  (void)surface;
}

const struct wl_surface_listener _maru_wayland_surface_listener = {
    .enter = _wl_surface_handle_enter,
    .leave = _wl_surface_handle_leave,
    .preferred_buffer_scale = _wl_surface_handle_preferred_buffer_scale,
};

static void _xdg_surface_handle_configure(void *data, struct xdg_surface *surface,
                                          uint32_t serial) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  maru_xdg_surface_ack_configure(ctx, surface, serial);

  if (!(window->base.pub.flags & MARU_WINDOW_STATE_READY)) {
    window->base.pub.flags |= MARU_WINDOW_STATE_READY;
    
    MARU_Event evt = {0};
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_READY, (MARU_Window*)window, &evt);
  }
}

const struct xdg_surface_listener _maru_wayland_xdg_surface_listener = {
    .configure = _xdg_surface_handle_configure};

static void _xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *toplevel,
                                           int32_t width, int32_t height, struct wl_array *states) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  
  bool maximized = false;
  bool fullscreen = false;
  
  uint32_t *state;
  wl_array_for_each(state, states) {
    switch (*state) {
      case XDG_TOPLEVEL_STATE_MAXIMIZED: maximized = true; break;
      case XDG_TOPLEVEL_STATE_FULLSCREEN: fullscreen = true; break;
    }
  }

  // Basic resize handling
  if (width > 0 && height > 0) {
    if (window->size.x != (MARU_Scalar)width || window->size.y != (MARU_Scalar)height) {
      window->size.x = (MARU_Scalar)width;
      window->size.y = (MARU_Scalar)height;
      
      MARU_Event evt = {0};
      evt.resized.geometry.logical_size = window->size;
      evt.resized.geometry.pixel_size.x = (int32_t)window->size.x;
      evt.resized.geometry.pixel_size.y = (int32_t)window->size.y;
      
      _maru_wayland_update_opaque_region(window);
      _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
    }
  }

  if (window->is_maximized != maximized) {
    window->is_maximized = maximized;
    if (maximized)
      window->base.pub.flags |= (uint64_t)MARU_WINDOW_STATE_MAXIMIZED;
    else
      window->base.pub.flags &= ~(uint64_t)MARU_WINDOW_STATE_MAXIMIZED;

    MARU_Event evt = {0};
    evt.maximized.maximized = maximized;
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_MAXIMIZED, (MARU_Window*)window, &evt);
  }

  if (window->is_fullscreen != fullscreen) {
    window->is_fullscreen = fullscreen;
    if (fullscreen)
      window->base.pub.flags |= (uint64_t)MARU_WINDOW_STATE_FULLSCREEN;
    else
      window->base.pub.flags &= ~(uint64_t)MARU_WINDOW_STATE_FULLSCREEN;
  }
  
  (void)toplevel;
}

static void _xdg_toplevel_handle_close(void *data, struct xdg_toplevel *toplevel) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  
  MARU_Event evt = {0};
  
  _maru_dispatch_event(&ctx->base, MARU_CLOSE_REQUESTED, (MARU_Window*)window, &evt);
  (void)toplevel;
}

const struct xdg_toplevel_listener _maru_wayland_xdg_toplevel_listener = {
    .configure = _xdg_toplevel_handle_configure,
    .close = _xdg_toplevel_handle_close,
};

bool _maru_wayland_create_xdg_shell_objects(MARU_Window_WL *window,
                                             const MARU_WindowCreateInfo *create_info) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_WaylandDecorationMode mode = ctx->decoration_mode;

  bool try_ssd = (mode == MARU_WAYLAND_DECORATION_MODE_AUTO || mode == MARU_WAYLAND_DECORATION_MODE_SSD);
  bool try_csd = (mode == MARU_WAYLAND_DECORATION_MODE_AUTO || mode == MARU_WAYLAND_DECORATION_MODE_CSD);

  if (try_ssd && ctx->protocols.opt.zxdg_decoration_manager_v1) {
    window->xdg.surface = maru_xdg_wm_base_get_xdg_surface(
        ctx, ctx->protocols.xdg_wm_base, window->wl.surface);
    if (!window->xdg.surface) {
      return false;
    }

    maru_xdg_surface_add_listener(ctx, window->xdg.surface,
                                  &_maru_wayland_xdg_surface_listener, window);

    window->xdg.toplevel = maru_xdg_surface_get_toplevel(ctx, window->xdg.surface);
    if (!window->xdg.toplevel) {
      return false;
    }

    maru_xdg_toplevel_add_listener(ctx, window->xdg.toplevel,
                                   &_maru_wayland_xdg_toplevel_listener, window);

    if (create_info->attributes.title) {
      maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel,
                                  create_info->attributes.title);
    } else {
      maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel, "Maru");
    }

    if (create_info->app_id) {
      maru_xdg_toplevel_set_app_id(ctx, window->xdg.toplevel, create_info->app_id);
    }

    window->xdg.decoration = maru_zxdg_decoration_manager_v1_get_toplevel_decoration(
        ctx, ctx->protocols.opt.zxdg_decoration_manager_v1, window->xdg.toplevel);
    maru_zxdg_toplevel_decoration_v1_add_listener(ctx, window->xdg.decoration,
                                                  &_maru_wayland_xdg_decoration_listener, window);

    if (window->is_decorated) {
      maru_zxdg_toplevel_decoration_v1_set_mode(
          ctx, window->xdg.decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    } else {
      maru_zxdg_toplevel_decoration_v1_set_mode(
          ctx, window->xdg.decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    }

    window->decor_mode = MARU_WAYLAND_DECORATION_MODE_SSD;
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_AUTO && try_csd && ctx->libdecor.ctx) {
    if (_maru_wayland_create_libdecor_frame(window, create_info)) {
      window->decor_mode = MARU_WAYLAND_DECORATION_MODE_CSD;
    }
  }

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_AUTO) {
    window->xdg.surface = maru_xdg_wm_base_get_xdg_surface(
        ctx, ctx->protocols.xdg_wm_base, window->wl.surface);
    if (!window->xdg.surface) {
      return false;
    }

    maru_xdg_surface_add_listener(ctx, window->xdg.surface,
                                  &_maru_wayland_xdg_surface_listener, window);

    window->xdg.toplevel = maru_xdg_surface_get_toplevel(ctx, window->xdg.surface);
    if (!window->xdg.toplevel) {
      return false;
    }

    maru_xdg_toplevel_add_listener(ctx, window->xdg.toplevel,
                                   &_maru_wayland_xdg_toplevel_listener, window);

    if (create_info->attributes.title) {
      maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel,
                                  create_info->attributes.title);
    } else {
      maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel, "Maru");
    }

    window->decor_mode = MARU_WAYLAND_DECORATION_MODE_NONE;
  }

  maru_wl_surface_commit(ctx, window->wl.surface);
  return true;
}

void _maru_wayland_update_opaque_region(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (window->base.pub.flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH) {
    maru_wl_surface_set_opaque_region(ctx, window->wl.surface, NULL);
  } else {
    struct wl_region *region = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
    maru_wl_region_add(ctx, region, 0, 0, (int32_t)window->size.x, (int32_t)window->size.y);
    maru_wl_surface_set_opaque_region(ctx, window->wl.surface, region);
    maru_wl_region_destroy(ctx, region);
  }

  struct wl_region *input_region = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
  if (!(window->base.pub.flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH)) {
    maru_wl_region_add(ctx, input_region, 0, 0, (int32_t)window->size.x, (int32_t)window->size.y);
  }
  maru_wl_surface_set_input_region(ctx, window->wl.surface, input_region);
  maru_wl_region_destroy(ctx, input_region);

  if (window->xdg.surface) {
    maru_xdg_surface_set_window_geometry(ctx, window->xdg.surface, 0, 0,
                                         (int)window->size.x, (int)window->size.y);
  }
}

MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_Window_WL *window = (MARU_Window_WL *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_WL));
  if (!window) {
    return MARU_FAILURE;
  }
  
  _maru_init_window_base(&window->base, &ctx->base, create_info);
  memset(((uint8_t*)window) + sizeof(MARU_Window_Base), 0, sizeof(MARU_Window_WL) - sizeof(MARU_Window_Base));

#ifdef MARU_INDIRECT_BACKEND
  window->base.backend = ctx->base.backend;
#endif

  window->size = create_info->attributes.logical_size;
  window->scale = 1.0;
  window->cursor_mode = create_info->attributes.cursor_mode;
  window->is_decorated = create_info->attributes.decorated;
  window->is_resizable = create_info->attributes.resizable;

  window->wl.surface =
      maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);

  if (!window->wl.surface) {
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  maru_wl_surface_set_user_data(ctx, window->wl.surface, window);

  maru_wl_surface_add_listener(ctx, window->wl.surface, &_maru_wayland_surface_listener,
                               window);

  if (ctx->protocols.opt.wp_fractional_scale_manager_v1) {
    window->ext.fractional_scale = maru_wp_fractional_scale_manager_v1_get_fractional_scale(
        ctx, ctx->protocols.opt.wp_fractional_scale_manager_v1, window->wl.surface);
    maru_wp_fractional_scale_v1_add_listener(ctx, window->ext.fractional_scale,
                                             &_maru_wayland_fractional_scale_listener, window);
  }

  if (!_maru_wayland_create_xdg_shell_objects(window, create_info)) {
    maru_wl_surface_destroy(ctx, window->wl.surface);
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  if (ctx->protocols.opt.wp_content_type_manager_v1) {
    window->ext.content_type = maru_wp_content_type_manager_v1_get_surface_content_type(
        ctx, ctx->protocols.opt.wp_content_type_manager_v1, window->wl.surface);
    if (window->ext.content_type) {
      maru_wp_content_type_v1_set_content_type(
          ctx, window->ext.content_type, _maru_wayland_map_content_type(create_info->content_type));
    }
  }

  _maru_wayland_update_opaque_region(window);
  _maru_wayland_update_idle_inhibitor(window);

  if (create_info->attributes.mouse_passthrough) {
    struct wl_region *region = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
    maru_wl_surface_set_input_region(ctx, window->wl.surface, region);
    maru_wl_region_destroy(ctx, region);
  }

  _maru_register_window(&ctx->base, (MARU_Window *)window);

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;
}

MARU_Status maru_updateWindow_WL(MARU_Window *window_handle, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  _maru_update_window_base(&window->base, field_mask, attributes);

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
      _maru_wayland_libdecor_set_title(window, attributes->title);
    } else if (window->xdg.toplevel) {
      maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel, attributes->title);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    window->cursor_mode = attributes->cursor_mode;
    _maru_wayland_update_cursor(ctx, window, ctx->focused.pointer_serial);
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    window->base.pub.current_cursor = attributes->cursor;
    _maru_wayland_update_cursor(ctx, window, ctx->focused.pointer_serial);
  }

  if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
    if (window->size.x != attributes->logical_size.x || window->size.y != attributes->logical_size.y) {
      window->size = attributes->logical_size;
      
      MARU_Event evt = {0};
      evt.resized.geometry.logical_size = window->size;
      evt.resized.geometry.pixel_size.x = (int32_t)(window->size.x * window->scale);
      evt.resized.geometry.pixel_size.y = (int32_t)(window->size.y * window->scale);
      
      _maru_wayland_update_opaque_region(window);
      _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
      if (attributes->fullscreen) {
        maru_libdecor_frame_set_fullscreen(ctx, window->libdecor.frame, NULL);
      } else {
        maru_libdecor_frame_unset_fullscreen(ctx, window->libdecor.frame);
      }
    } else if (window->xdg.toplevel) {
      if (attributes->fullscreen) {
        maru_xdg_toplevel_set_fullscreen(ctx, window->xdg.toplevel, NULL);
      } else {
        maru_xdg_toplevel_unset_fullscreen(ctx, window->xdg.toplevel);
      }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
      if (attributes->maximized) {
        maru_libdecor_frame_set_maximized(ctx, window->libdecor.frame);
      } else {
        maru_libdecor_frame_unset_maximized(ctx, window->libdecor.frame);
      }
    } else if (window->xdg.toplevel) {
      if (attributes->maximized) {
        maru_xdg_toplevel_set_maximized(ctx, window->xdg.toplevel);
      } else {
        maru_xdg_toplevel_unset_maximized(ctx, window->xdg.toplevel);
      }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MIN_SIZE) {
    window->min_size = attributes->min_size;
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
      maru_libdecor_frame_set_min_content_size(ctx, window->libdecor.frame, (int)window->min_size.x, (int)window->min_size.y);
    } else if (window->xdg.toplevel) {
      maru_xdg_toplevel_set_min_size(ctx, window->xdg.toplevel, (int)window->min_size.x, (int)window->min_size.y);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MAX_SIZE) {
    window->max_size = attributes->max_size;
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
      maru_libdecor_frame_set_max_content_size(ctx, window->libdecor.frame, (int)window->max_size.x, (int)window->max_size.y);
    } else if (window->xdg.toplevel) {
      maru_xdg_toplevel_set_max_size(ctx, window->xdg.toplevel, (int)window->max_size.x, (int)window->max_size.y);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
    if (attributes->mouse_passthrough) {
      struct wl_region *region = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
      maru_wl_surface_set_input_region(ctx, window->wl.surface, region);
      maru_wl_region_destroy(ctx, region);
    } else {
      maru_wl_surface_set_input_region(ctx, window->wl.surface, NULL);
    }
    maru_wl_surface_commit(ctx, window->wl.surface);
  }

  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
    _maru_wayland_destroy_libdecor_frame(window);
  } else {
    if (window->xdg.decoration) {
      maru_zxdg_toplevel_decoration_v1_destroy(ctx, window->xdg.decoration);
    }
    if (window->xdg.toplevel) maru_xdg_toplevel_destroy(ctx, window->xdg.toplevel);
    if (window->xdg.surface) maru_xdg_surface_destroy(ctx, window->xdg.surface);
  }

  if (window->ext.idle_inhibitor) {
    maru_zwp_idle_inhibitor_v1_destroy(ctx, window->ext.idle_inhibitor);
  }

  if (window->ext.content_type) {
    maru_wp_content_type_v1_destroy(ctx, window->ext.content_type);
  }

  if (window->ext.fractional_scale) {
    maru_wp_fractional_scale_v1_destroy(ctx, window->ext.fractional_scale);
  }

  if (window->monitors) {
    maru_context_free(&ctx->base, window->monitors);
  }

  maru_wl_surface_destroy(ctx, window->wl.surface);

  _maru_unregister_window(&ctx->base, window_handle);
  _maru_cleanup_window_base(&window->base);
  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_WL *window = (const MARU_Window_WL *)window_handle;

  *out_geometry = (MARU_WindowGeometry){
      .origin = {0, 0},
      .logical_size = window->size,
      .pixel_size =
          {
              .x = (int32_t)(window->size.x * window->scale),
              .y = (int32_t)(window->size.y * window->scale),
          },
  };
}

void *_maru_getWindowNativeHandle_WL(MARU_Window *window) {
  MARU_Window_WL *win = (MARU_Window_WL *)window;
  return win->wl.surface;
}

MARU_Status maru_getWindowBackendHandle_WL(MARU_Window *window_handle,
                                         MARU_BackendType *out_type,
                                         MARU_BackendHandle *out_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  *out_type = MARU_BACKEND_WAYLAND;
#ifdef MARU_ENABLE_BACKEND_WAYLAND
  out_handle->wayland_surface = window->wl.surface;
#else
  (void)window;
  (void)out_handle;
#endif
  return MARU_SUCCESS;
}
