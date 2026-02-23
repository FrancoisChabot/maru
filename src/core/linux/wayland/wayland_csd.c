#include "wayland_internal.h"
#include <string.h>

static void _libdecor_handle_error(struct libdecor *context,
                                   enum libdecor_error error,
                                   const char *message) {
  // TODO: Log error
}

static struct libdecor_interface _libdecor_interface = {
    .error = _libdecor_handle_error,
};

bool _maru_wayland_init_libdecor(MARU_Context_WL *ctx) {
  if (!ctx->dlib.opt.decor.base.available) {
    return false;
  }

  ctx->libdecor_context = maru_libdecor_new(ctx, ctx->wl.display, &_libdecor_interface);
  if (!ctx->libdecor_context) {
    return false;
  }

  return true;
}

void _maru_wayland_cleanup_libdecor(MARU_Context_WL *ctx) {
  if (ctx->libdecor_context) {
    maru_libdecor_unref(ctx, ctx->libdecor_context);
    ctx->libdecor_context = NULL;
  }
}

static void _libdecor_frame_handle_configure(struct libdecor_frame *frame,
                                              struct libdecor_configuration *configuration,
                                              void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  int width, height;
  enum libdecor_window_state window_state;

  if (!maru_libdecor_configuration_get_content_size(ctx, configuration, frame, &width, &height)) {
    width = (int)window->size.x;
    height = (int)window->size.y;
  }

  window->size.x = (MARU_Scalar)width;
  window->size.y = (MARU_Scalar)height;

  struct libdecor_state *state = maru_libdecor_state_new(ctx, width, height);
  maru_libdecor_frame_commit(ctx, frame, state, configuration);
  maru_libdecor_state_free(ctx, state);

  if (!maru_isWindowReady((MARU_Window *)window)) {
    window->base.pub.flags |= MARU_WINDOW_STATE_READY;
    MARU_Event evt = {0};
    _maru_dispatch_event(&ctx->base, MARU_WINDOW_READY, (MARU_Window *)window, &evt);
  }

  maru_wl_surface_commit(ctx, window->wl.surface);
}

static void _libdecor_frame_handle_close(struct libdecor_frame *frame, void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Event evt = {0};
  _maru_dispatch_event(&ctx->base, MARU_CLOSE_REQUESTED, (MARU_Window *)window, &evt);
}

static void _libdecor_frame_handle_commit(struct libdecor_frame *frame, void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  maru_wl_surface_commit(ctx, window->wl.surface);
}

static struct libdecor_frame_interface _libdecor_frame_interface = {
    .configure = _libdecor_frame_handle_configure,
    .close = _libdecor_frame_handle_close,
    .commit = _libdecor_frame_handle_commit,
};

bool _maru_wayland_create_libdecor_frame(MARU_Window_WL *window,
                                          const MARU_WindowCreateInfo *create_info) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (!ctx->libdecor_context) {
    return false;
  }

  window->libdecor.frame = maru_libdecor_decorate(ctx, ctx->libdecor_context,
                                                 window->wl.surface,
                                                 &_libdecor_frame_interface,
                                                 window);
  if (!window->libdecor.frame) {
    return false;
  }

  if (create_info->attributes.title) {
    maru_libdecor_frame_set_title(ctx, window->libdecor.frame, create_info->attributes.title);
  }

  if (create_info->app_id) {
    maru_libdecor_frame_set_app_id(ctx, window->libdecor.frame, create_info->app_id);
  }

  maru_libdecor_frame_map(ctx, window->libdecor.frame);

  return true;
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
