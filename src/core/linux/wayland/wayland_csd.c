#include "wayland_internal.h"
#include "maru_api_constraints.h"

#include <stdlib.h>

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

    _maru_wayland_update_opaque_region(window);
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

bool _maru_wayland_init_libdecor(MARU_Context_WL *ctx) {
  if (ctx->dlib.opt.decor.base.available) {
    ctx->libdecor.ctx = maru_libdecor_new(ctx, ctx->wl.display, NULL);
    return ctx->libdecor.ctx != NULL;
  }
  return false;
}

void _maru_wayland_cleanup_libdecor(MARU_Context_WL *ctx) {
  if (ctx->libdecor.ctx) {
    maru_libdecor_unref(ctx, ctx->libdecor.ctx);
    ctx->libdecor.ctx = NULL;
  }
}

bool _maru_wayland_create_libdecor_frame(MARU_Window_WL *window,
                                          const MARU_WindowCreateInfo *create_info) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (!ctx->libdecor.ctx) {
    return false;
  }

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
    return true;
  }

  return false;
}

void _maru_wayland_destroy_libdecor_frame(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->libdecor.frame) {
    maru_libdecor_frame_unref(ctx, window->libdecor.frame);
    window->libdecor.frame = NULL;
  }
}

void _maru_wayland_libdecor_set_title(MARU_Window_WL *window, const char *title) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->libdecor.frame) {
    maru_libdecor_frame_set_title(ctx, window->libdecor.frame, title);
  }
}

void _maru_wayland_libdecor_set_capabilities(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->libdecor.frame) {
    enum libdecor_capabilities caps = LIBDECOR_ACTION_CLOSE | LIBDECOR_ACTION_MOVE | LIBDECOR_ACTION_MINIMIZE;
    if (window->is_resizable) {
      caps |= LIBDECOR_ACTION_RESIZE | LIBDECOR_ACTION_FULLSCREEN;
    }
    maru_libdecor_frame_set_capabilities(ctx, window->libdecor.frame, caps);
  }
}
