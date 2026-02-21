#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static void _xdg_activation_token_handle_done(void *data, struct xdg_activation_token_v1 *token,
                                              const char *token_str) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (ctx->activation_token) {
    maru_context_free(&ctx->base, ctx->activation_token);
  }
  size_t len = strlen(token_str);
  ctx->activation_token = maru_context_alloc(&ctx->base, len + 1);
  if (ctx->activation_token) {
    memcpy(ctx->activation_token, token_str, len + 1);
  }
  maru_xdg_activation_token_v1_destroy(ctx, token);
}

static const struct xdg_activation_token_v1_listener _xdg_activation_token_listener = {
    .done = _xdg_activation_token_handle_done,
};

MARU_Status maru_requestWindowFocus_WL(MARU_Window *window_handle) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (!ctx->protocols.opt.xdg_activation_v1) {
    return MARU_FAILURE;
  }

  struct xdg_activation_token_v1 *token =
      maru_xdg_activation_v1_get_activation_token(ctx, ctx->protocols.opt.xdg_activation_v1);
  
  maru_xdg_activation_token_v1_add_listener(ctx, token, &_xdg_activation_token_listener, ctx);
  
  if (ctx->focused.activation_serial != 0 && ctx->input.seat) {
    maru_xdg_activation_token_v1_set_serial(ctx, token, ctx->focused.activation_serial, ctx->input.seat);
  }
  
  maru_xdg_activation_token_v1_set_surface(ctx, token, window->wl.surface);
  maru_xdg_activation_token_v1_commit(ctx, token);
  
  return MARU_SUCCESS;
}

void _maru_wayland_check_activation(MARU_Context_WL *ctx) {
  if (ctx->activation_token && ctx->protocols.opt.xdg_activation_v1) {
    // We need a surface to activate. For now, let's just pick the first ready window.
    MARU_Window_WL *target = NULL;
    for (uint32_t i = 0; i < ctx->base.window_cache_count; i++) {
      MARU_Window_WL *win = (MARU_Window_WL *)ctx->base.window_cache[i];
      if (win->base.pub.flags & MARU_WINDOW_STATE_READY) {
        target = win;
        break;
      }
    }

    if (target) {
      maru_xdg_activation_v1_activate(ctx, ctx->protocols.opt.xdg_activation_v1, ctx->activation_token, target->wl.surface);
      maru_context_free(&ctx->base, ctx->activation_token);
      ctx->activation_token = NULL;
    }
  }
}

void _maru_wayland_update_idle_inhibitor(MARU_Window_WL *window) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  if (ctx->base.inhibit_idle && ctx->protocols.opt.zwp_idle_inhibit_manager_v1) {
    if (!window->ext.idle_inhibitor) {
      window->ext.idle_inhibitor = maru_zwp_idle_inhibit_manager_v1_create_inhibitor(
          ctx, ctx->protocols.opt.zwp_idle_inhibit_manager_v1, window->wl.surface);
    }
  } else {
    if (window->ext.idle_inhibitor) {
      maru_zwp_idle_inhibitor_v1_destroy(ctx, window->ext.idle_inhibitor);
      window->ext.idle_inhibitor = NULL;
    }
  }
}
