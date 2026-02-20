#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

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
    evt.type = MARU_EVENT_TYPE_WINDOW_READY;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_TYPE_WINDOW_READY, (MARU_Window*)window, &evt);
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
      evt.type = MARU_EVENT_TYPE_WINDOW_RESIZED;
      evt.resized.geometry.logical_size = window->size;
      evt.resized.geometry.pixel_size.x = (int32_t)window->size.x;
      evt.resized.geometry.pixel_size.y = (int32_t)window->size.y;
      
      _maru_dispatch_event(&ctx->base, MARU_EVENT_TYPE_WINDOW_RESIZED, (MARU_Window*)window, &evt);
    }
  }
  
  (void)toplevel; (void)states;
}

static void _xdg_toplevel_handle_close(void *data, struct xdg_toplevel *toplevel) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  
  MARU_Event evt = {0};
  evt.type = MARU_EVENT_TYPE_CLOSE_REQUESTED;
  
  _maru_dispatch_event(&ctx->base, MARU_EVENT_TYPE_CLOSE_REQUESTED, (MARU_Window*)window, &evt);
  (void)toplevel;
}

const struct xdg_toplevel_listener _maru_wayland_xdg_toplevel_listener = {
    .configure = _xdg_toplevel_handle_configure,
    .close = _xdg_toplevel_handle_close,
};

bool _maru_wayland_create_xdg_shell_objects(MARU_Window_WL *window,
                                            const MARU_WindowCreateInfo *create_info) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

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
  }

  maru_wl_surface_commit(ctx, window->wl.surface);
  return true;
}

void _maru_wayland_update_opaque_region(MARU_Window_WL *window) {
  (void)window;
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

  window->wl.surface =
      maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);

  if (!window->wl.surface) {
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

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

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;
}

MARU_Status maru_updateWindow_WL(MARU_Window *window_handle, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  _maru_update_window_base(&window->base, field_mask, attributes);

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    maru_xdg_toplevel_set_title(ctx, window->xdg.toplevel, attributes->title);
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

  if (window->xdg.toplevel) maru_xdg_toplevel_destroy(ctx, window->xdg.toplevel);
  if (window->xdg.surface) maru_xdg_surface_destroy(ctx, window->xdg.surface);

  maru_wl_surface_destroy(ctx, window->wl.surface);

  _maru_cleanup_window_base(&window->base);
  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

MARU_Status maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
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

  return MARU_SUCCESS;
}
