#include "wayland_internal.h"
#include <string.h>
#include <stdio.h>

static void *(*_libdecor_get_userdata_fn)(struct libdecor *context) = NULL;

static void _libdecor_handle_error(struct libdecor *context,
                                   enum libdecor_error error,
                                   const char *message) {
  MARU_Context_WL *ctx = NULL;
  if (_libdecor_get_userdata_fn && context) {
    ctx = (MARU_Context_WL *)_libdecor_get_userdata_fn(context);
  }

  char detail[256];
  snprintf(detail, sizeof(detail), "libdecor error (%d): %s", (int)error,
           message ? message : "unknown");

  if (ctx) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           detail);
    return;
  }

  fprintf(stderr, "MARU: %s\n", detail);
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

  _libdecor_get_userdata_fn = ctx->dlib.opt.decor.opt.get_userdata;
  if (ctx->dlib.opt.decor.opt.set_userdata) {
    ctx->dlib.opt.decor.opt.set_userdata(ctx->libdecor_context, ctx);
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
  MARU_WindowAttributes *effective = &window->base.attrs_effective;

  int width, height;
  enum libdecor_window_state window_state;
  bool is_maximized = false;
  bool is_fullscreen = false;

  if (maru_libdecor_configuration_get_window_state(ctx, configuration, &window_state)) {
    is_maximized = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
    is_fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
  }

  if (effective->maximized != is_maximized) {
    effective->maximized = is_maximized;
    if (is_maximized) {
      window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    } else {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
    }
    _maru_wayland_dispatch_presentation_state(
        window, MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED, true);
  }

  effective->fullscreen = is_fullscreen;
  if (is_fullscreen) {
    window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  } else {
    window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
  }

  if (!maru_libdecor_configuration_get_content_size(ctx, configuration, frame, &width, &height)) {
    width = (int)effective->logical_size.x;
    height = (int)effective->logical_size.y;
  }

  uint32_t content_width = width > 0 ? (uint32_t)width : (uint32_t)effective->logical_size.x;
  uint32_t content_height = height > 0 ? (uint32_t)height : (uint32_t)effective->logical_size.y;

  if (!is_maximized && !is_fullscreen) {
    _maru_wayland_enforce_aspect_ratio(&content_width, &content_height, window);
  }
  const bool size_changed = (effective->logical_size.x != (MARU_Scalar)content_width) ||
                            (effective->logical_size.y != (MARU_Scalar)content_height);

  effective->logical_size.x = (MARU_Scalar)content_width;
  effective->logical_size.y = (MARU_Scalar)content_height;

  struct libdecor_state *state = maru_libdecor_state_new(ctx, (int)content_width, (int)content_height);
  if (!state) {
    return;
  }
  maru_libdecor_frame_commit(ctx, frame, state, configuration);
  maru_libdecor_state_free(ctx, state);

  if (!maru_isWindowReady((MARU_Window *)window)) {
    window->base.pub.flags |= MARU_WINDOW_STATE_READY;
    MARU_Event evt = {0};
    maru_getWindowGeometry_WL((MARU_Window *)window, &evt.window_ready.geometry);
    _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_READY, (MARU_Window *)window, &evt);
  }

  if (size_changed) {
    _maru_wayland_dispatch_window_resized(window);
  }

  _maru_wayland_update_opaque_region(window);
  maru_wl_surface_commit(ctx, window->wl.surface);
}

static void _libdecor_frame_handle_close(struct libdecor_frame *frame, void *user_data) {
  MARU_Window_WL *window = (MARU_Window_WL *)user_data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  MARU_Event evt = {0};
  _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)window, &evt);
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
  const MARU_WindowAttributes *attrs = &window->base.attrs_effective;

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

  if (attrs->fullscreen) {
    struct wl_output *output = NULL;
    if (attrs->monitor &&
        maru_getMonitorContext(attrs->monitor) == window->base.pub.context) {
      MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)attrs->monitor;
      output = monitor->output;
    }
    maru_libdecor_frame_set_fullscreen(ctx, window->libdecor.frame, output);
  }

  if (attrs->maximized) {
    maru_libdecor_frame_set_maximized(ctx, window->libdecor.frame);
  }
  if (attrs->minimized || !attrs->visible) {
    struct xdg_toplevel *toplevel =
        maru_libdecor_frame_get_xdg_toplevel(ctx, window->libdecor.frame);
    if (toplevel) {
      maru_xdg_toplevel_set_minimized(ctx, toplevel);
    }
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
