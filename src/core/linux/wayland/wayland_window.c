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

static void _libdecor_frame_handle_configure(struct libdecor_frame *frame,
                                             struct libdecor_configuration *configuration,
                                             void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  int width = 0;
  int height = 0;

  enum libdecor_window_state state_flags;
  bool maximized = false;
  bool fullscreen = false;

  if (maru_libdecor_configuration_get_window_state(ctx, configuration, &state_flags)) {
    if (state_flags & LIBDECOR_WINDOW_STATE_MAXIMIZED) maximized = true;
    if (state_flags & LIBDECOR_WINDOW_STATE_FULLSCREEN) fullscreen = true;
  }

  if (!maru_libdecor_configuration_get_content_size(ctx, configuration, frame, &width, &height)) {
    width = (int)window->size.x;
    height = (int)window->size.y;
  }

  if (window->size.x != (MARU_Scalar)width || window->size.y != (MARU_Scalar)height) {
    window->size.x = (MARU_Scalar)width;
    window->size.y = (MARU_Scalar)height;

    MARU_Event evt = {0};
    evt.resized.geometry.logical_size = window->size;
    evt.resized.geometry.pixel_size.x = (int32_t)window->size.x;
    evt.resized.geometry.pixel_size.y = (int32_t)window->size.y;

    _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
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

  struct libdecor_state *state = maru_libdecor_state_new(ctx, width, height);
  maru_libdecor_frame_commit(ctx, frame, state, configuration);
  maru_libdecor_state_free(ctx, state);

  if (!(window->base.pub.flags & MARU_WINDOW_STATE_READY)) {
    window->base.pub.flags |= MARU_WINDOW_STATE_READY;
    
    MARU_Event evt = {0};
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_READY, (MARU_Window*)window, &evt);
  }

  window->libdecor.last_configuration = configuration;
}

static void _libdecor_frame_handle_close(struct libdecor_frame *frame, void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Event evt = {0};
  _maru_dispatch_event(&ctx->base, MARU_CLOSE_REQUESTED, (MARU_Window*)window, &evt);
  (void)frame;
}

static void _libdecor_frame_handle_commit(struct libdecor_frame *frame, void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  maru_wl_surface_commit(ctx, window->wl.surface);
  (void)frame;
}

static struct libdecor_frame_interface _maru_libdecor_frame_interface = {
    .configure = _libdecor_frame_handle_configure,
    .close = _libdecor_frame_handle_close,
    .commit = _libdecor_frame_handle_commit,
};

MARU_WaylandDecorationMode _maru_wayland_get_decoration_mode(const MARU_ContextCreateInfo *create_info) {
  if (create_info->tuning) {
    return create_info->tuning->wayland.decoration_mode;
  }
  return MARU_WAYLAND_DECORATION_MODE_AUTO;
}

static void _wl_surface_handle_enter(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  (void)data; (void)surface; (void)output;
}
static void _wl_surface_handle_leave(void *data, struct wl_surface *surface,
                                     struct wl_output *output) {
  (void)data; (void)surface; (void)output;
}

const struct wl_surface_listener _maru_wayland_surface_listener = {
    .enter = _wl_surface_handle_enter,
    .leave = _wl_surface_handle_leave,
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
  
  // Basic resize handling
  if (width > 0 && height > 0) {
    if (window->size.x != (MARU_Scalar)width || window->size.y != (MARU_Scalar)height) {
      window->size.x = (MARU_Scalar)width;
      window->size.y = (MARU_Scalar)height;
      
      MARU_Event evt = {0};
      evt.resized.geometry.logical_size = window->size;
      evt.resized.geometry.pixel_size.x = (int32_t)window->size.x;
      evt.resized.geometry.pixel_size.y = (int32_t)window->size.y;
      
      _maru_dispatch_event(&ctx->base, MARU_WINDOW_RESIZED, (MARU_Window*)window, &evt);
    }
  }
  
  (void)toplevel; (void)states;
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
    window->libdecor.frame = maru_libdecor_decorate(ctx, ctx->libdecor.ctx, window->wl.surface,
                                                    &_maru_libdecor_frame_interface, window);

    if (window->libdecor.frame) {
      if (create_info->attributes.title) {
        maru_libdecor_frame_set_title(ctx, window->libdecor.frame,
                                      create_info->attributes.title);
      } else {
        maru_libdecor_frame_set_title(ctx, window->libdecor.frame, "Maru");
      }

      if (create_info->app_id) {
        maru_libdecor_frame_set_app_id(ctx, window->libdecor.frame,
                                       create_info->app_id);
      }

      enum libdecor_capabilities caps = LIBDECOR_ACTION_CLOSE | LIBDECOR_ACTION_MOVE | LIBDECOR_ACTION_MINIMIZE;
      if (window->is_resizable) {
        caps |= LIBDECOR_ACTION_RESIZE | LIBDECOR_ACTION_FULLSCREEN;
      }
      maru_libdecor_frame_set_capabilities(ctx, window->libdecor.frame, caps);
      maru_libdecor_frame_set_visibility(ctx, window->libdecor.frame, window->is_decorated);

      maru_libdecor_frame_map(ctx, window->libdecor.frame);
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

  if (!_maru_wayland_create_xdg_shell_objects(window, create_info)) {
    maru_wl_surface_destroy(ctx, window->wl.surface);
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  _maru_wayland_update_opaque_region(window);

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
    if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
      maru_libdecor_frame_set_title(ctx, window->libdecor.frame, attributes->title);
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

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD && window->libdecor.frame) {
    maru_libdecor_frame_unref(ctx, window->libdecor.frame);
  } else {
    if (window->xdg.decoration) {
      maru_zxdg_toplevel_decoration_v1_destroy(ctx, window->xdg.decoration);
    }
    if (window->xdg.toplevel) maru_xdg_toplevel_destroy(ctx, window->xdg.toplevel);
    if (window->xdg.surface) maru_xdg_surface_destroy(ctx, window->xdg.surface);
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
              .x = (int32_t)window->size.x, // TODO: apply scale
              .y = (int32_t)window->size.y,
          },
  };
}

void *_maru_getWindowNativeHandle_WL(MARU_Window *window) {
  MARU_Window_WL *win = (MARU_Window_WL *)window;
  return win->wl.surface;
}
