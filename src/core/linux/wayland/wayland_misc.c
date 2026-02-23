#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct MARU_WL_ActivationTokenResult {
  bool done;
  const char *token;
} MARU_WL_ActivationTokenResult;

static void _maru_wayland_activation_token_done(void *data,
                                                struct xdg_activation_token_v1 *token_obj,
                                                const char *token) {
  (void)token_obj;
  MARU_WL_ActivationTokenResult *result = (MARU_WL_ActivationTokenResult *)data;
  result->token = token;
  result->done = true;
}

static const struct xdg_activation_token_v1_listener _maru_wayland_activation_token_listener = {
    .done = _maru_wayland_activation_token_done,
};

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

  MARU_WL_ActivationTokenResult result = {0};
  maru_xdg_activation_token_v1_add_listener(ctx, token_obj,
                                            &_maru_wayland_activation_token_listener, &result);
  maru_xdg_activation_token_v1_set_surface(ctx, token_obj, window->wl.surface);
  if (ctx->wl.seat && ctx->linux_common.pointer.enter_serial != 0) {
    maru_xdg_activation_token_v1_set_serial(
        ctx, token_obj, ctx->linux_common.pointer.enter_serial, ctx->wl.seat);
  }
  maru_xdg_activation_token_v1_commit(ctx, token_obj);

  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0 || !result.done || !result.token) {
    maru_xdg_activation_token_v1_destroy(ctx, token_obj);
    return MARU_FAILURE;
  }

  maru_xdg_activation_v1_activate(ctx, ctx->protocols.opt.xdg_activation_v1, result.token,
                                  window->wl.surface);
  maru_wl_surface_commit(ctx, window->wl.surface);
  maru_wl_display_flush(ctx, ctx->wl.display);
  maru_xdg_activation_token_v1_destroy(ctx, token_obj);
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
