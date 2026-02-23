#include "wayland_internal.h"

static void _zxdg_toplevel_decoration_v1_handle_configure(void *data,
                                                         struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1,
                                                         uint32_t mode) {
  MARU_Window_WL *window = (MARU_Window_WL *)data;
  // This tells us what the compositor decided (SSD vs CSD)
}

static const struct zxdg_toplevel_decoration_v1_listener _zxdg_toplevel_decoration_v1_listener = {
    .configure = _zxdg_toplevel_decoration_v1_handle_configure,
};

bool _maru_wayland_create_ssd_decoration(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (!ctx->protocols.opt.zxdg_decoration_manager_v1) {
    return false;
  }

  window->xdg.decoration = maru_zxdg_decoration_manager_v1_get_toplevel_decoration(
      ctx, ctx->protocols.opt.zxdg_decoration_manager_v1, window->xdg.toplevel);

  if (!window->xdg.decoration) {
    return false;
  }

  maru_zxdg_toplevel_decoration_v1_add_listener(ctx, window->xdg.decoration,
                                               &_zxdg_toplevel_decoration_v1_listener, window);

  // Request SSD
  maru_zxdg_toplevel_decoration_v1_set_mode(ctx, window->xdg.decoration,
                                           ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

  return true;
}

void _maru_wayland_destroy_ssd_decoration(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (window->xdg.decoration) {
    maru_zxdg_toplevel_decoration_v1_destroy(ctx, window->xdg.decoration);
    window->xdg.decoration = NULL;
  }
}
