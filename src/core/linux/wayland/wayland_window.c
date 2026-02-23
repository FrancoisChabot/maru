#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static void _xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                           int32_t width, int32_t height,
                                           struct wl_array *states) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  
  if (width > 0 && height > 0) {
    window->size.x = (MARU_Scalar)width;
    window->size.y = (MARU_Scalar)height;
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
  window->size = create_info->attributes.logical_size;
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
  maru_wl_surface_set_user_data(ctx, window->wl.surface, window);

  if (window->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
    if (!_maru_wayland_create_libdecor_frame(window, create_info)) {
        goto cleanup_surface;
    }
  } else {
    if (!_maru_wayland_create_xdg_shell_objects(window, create_info)) {
      goto cleanup_surface;
    }
  }

  _maru_register_window(&ctx->base, (MARU_Window *)window);

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;

cleanup_surface:
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

  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (ctx->linux_common.pointer.focused_window == window_handle) {
    ctx->linux_common.pointer.focused_window = NULL;
  }

  _maru_unregister_window(&ctx->base, window_handle);

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