#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void _maru_wayland_dispatch_presentation_state(MARU_Window_WL *window, uint32_t changed_fields,
                                               bool icon_effective) {
  if (!window || changed_fields == 0u) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const uint64_t flags = window->base.pub.flags;

  MARU_Event evt = {0};
  evt.presentation.changed_fields = changed_fields;
  evt.presentation.visible = (flags & MARU_WINDOW_STATE_VISIBLE) != 0;
  evt.presentation.minimized = (flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
  evt.presentation.maximized = (flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
  evt.presentation.focused = (flags & MARU_WINDOW_STATE_FOCUSED) != 0;
  evt.presentation.icon_effective = icon_effective;
  _maru_dispatch_event(&ctx->base, MARU_WINDOW_PRESENTATION_STATE_CHANGED,
                       (MARU_Window *)window, &evt);
}

void _maru_wayland_cancel_activation(MARU_Context_WL *ctx) {
  if (!ctx) {
    return;
  }

  if (ctx->activation.token_obj) {
    maru_xdg_activation_token_v1_destroy(ctx, ctx->activation.token_obj);
    ctx->activation.token_obj = NULL;
  }
  if (ctx->activation.token_copy) {
    maru_context_free(&ctx->base, ctx->activation.token_copy);
    ctx->activation.token_copy = NULL;
  }

  ctx->activation.target_window = NULL;
  ctx->activation.pending = false;
  ctx->activation.done = false;
  ctx->activation.failed = false;
}

void _maru_wayland_cancel_activation_for_window(MARU_Context_WL *ctx,
                                                MARU_Window_WL *window) {
  if (!ctx || !window) {
    return;
  }
  if (ctx->activation.target_window == window) {
    _maru_wayland_cancel_activation(ctx);
  }
}

static void _maru_wayland_activation_token_done(void *data,
                                                struct xdg_activation_token_v1 *token_obj,
                                                const char *token) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || token_obj != ctx->activation.token_obj) {
    return;
  }

  if (ctx->activation.token_copy) {
    maru_context_free(&ctx->base, ctx->activation.token_copy);
    ctx->activation.token_copy = NULL;
  }

  size_t len = token ? strlen(token) : 0;
  char *copy = (char *)maru_context_alloc(&ctx->base, len + 1);
  if (!copy) {
    ctx->activation.failed = true;
    ctx->activation.done = true;
    return;
  }

  if (len > 0) {
    memcpy(copy, token, len);
  }
  copy[len] = '\0';
  ctx->activation.token_copy = copy;
  ctx->activation.done = true;
}

static const struct xdg_activation_token_v1_listener _maru_wayland_activation_token_listener = {
    .done = _maru_wayland_activation_token_done,
};

void _maru_wayland_check_activation(MARU_Context_WL *ctx) {
  if (!ctx || !ctx->activation.pending || !ctx->activation.done) {
    return;
  }

  MARU_Window_WL *window = ctx->activation.target_window;
  const char *token = ctx->activation.token_copy;
  bool can_activate = !ctx->activation.failed && window && window->wl.surface &&
                      ctx->protocols.opt.xdg_activation_v1 && token && token[0] != '\0';

  if (!can_activate) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           "Asynchronous activation request failed");
    _maru_wayland_cancel_activation(ctx);
    return;
  }

  maru_xdg_activation_v1_activate(ctx, ctx->protocols.opt.xdg_activation_v1, token,
                                  window->wl.surface);
  maru_wl_surface_commit(ctx, window->wl.surface);
  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           "wl_display_flush() failure while activating focus request");
  }
  _maru_wayland_cancel_activation(ctx);
}

void _maru_wayland_update_opaque_region(MARU_Window_WL *window) {
  if (!window || !window->wl.surface) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (window->is_transparent) {
    maru_wl_surface_set_opaque_region(ctx, window->wl.surface, NULL);
    return;
  }

  const int32_t width = (int32_t)window->base.attrs_effective.logical_size.x;
  const int32_t height = (int32_t)window->base.attrs_effective.logical_size.y;
  if (width <= 0 || height <= 0) {
    maru_wl_surface_set_opaque_region(ctx, window->wl.surface, NULL);
    return;
  }

  struct wl_region *opaque = maru_wl_compositor_create_region(ctx, ctx->protocols.wl_compositor);
  if (!opaque) {
    return;
  }

  maru_wl_region_add(ctx, opaque, 0, 0, width, height);
  maru_wl_surface_set_opaque_region(ctx, window->wl.surface, opaque);
  maru_wl_region_destroy(ctx, opaque);
}

void _maru_wayland_update_idle_inhibitor(MARU_Window_WL *window) {
  if (!window || !window->wl.surface) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const bool want_inhibit = ctx->base.inhibit_idle;

  if (want_inhibit) {
    if (!window->ext.idle_inhibitor) {
      if (ctx->protocols.opt.zwp_idle_inhibit_manager_v1) {
        window->ext.idle_inhibitor = maru_zwp_idle_inhibit_manager_v1_create_inhibitor(
            ctx, ctx->protocols.opt.zwp_idle_inhibit_manager_v1, window->wl.surface);
      } else {
        MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                               "Idle inhibit protocol unavailable");
      }
    }
  } else if (window->ext.idle_inhibitor) {
    maru_zwp_idle_inhibitor_v1_destroy(ctx, window->ext.idle_inhibitor);
    window->ext.idle_inhibitor = NULL;
  }
}

MARU_Status maru_requestWindowFocus_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  if (!window || !ctx || !window->wl.surface) {
    return MARU_FAILURE;
  }

  if (!ctx->protocols.opt.xdg_activation_v1) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                           "xdg_activation_v1 is unavailable; compositor focus request unsupported");
    return MARU_FAILURE;
  }

  struct xdg_activation_token_v1 *token_obj =
      maru_xdg_activation_v1_get_activation_token(ctx, ctx->protocols.opt.xdg_activation_v1);
  if (!token_obj) {
    return MARU_FAILURE;
  }

  // Cancel any previous in-flight request and replace it with the latest one.
  _maru_wayland_cancel_activation(ctx);
  ctx->activation.token_obj = token_obj;
  ctx->activation.target_window = window;
  ctx->activation.pending = true;
  ctx->activation.done = false;
  ctx->activation.failed = false;

  maru_xdg_activation_token_v1_add_listener(ctx, token_obj,
                                            &_maru_wayland_activation_token_listener, ctx);
  maru_xdg_activation_token_v1_set_surface(ctx, token_obj, window->wl.surface);
  if (ctx->protocols.opt.wl_seat && ctx->linux_common.pointer.enter_serial != 0) {
    maru_xdg_activation_token_v1_set_serial(
        ctx, token_obj, ctx->linux_common.pointer.enter_serial, ctx->protocols.opt.wl_seat);
  }
  maru_xdg_activation_token_v1_commit(ctx, token_obj);

  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    _maru_wayland_cancel_activation(ctx);
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  if (!window || !window->wl.surface) {
    return MARU_FAILURE;
  }

  _maru_wayland_request_frame(window);
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  if (!window) {
    return MARU_FAILURE;
  }
  return maru_requestWindowFocus_WL(window_handle);
}
