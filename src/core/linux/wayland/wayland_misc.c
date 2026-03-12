#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void _maru_wayland_dispatch_presentation_state(MARU_Window_WL *window, uint32_t changed_fields,
                                               bool icon) {
  if (changed_fields == 0u) {
    return;
  }
  MARU_ASSUME(window != NULL);

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
  const uint64_t flags = window->base.pub.flags;

  MARU_Event evt = {0};
  evt.presentation.changed_fields = changed_fields;
  evt.presentation.visible = (flags & MARU_WINDOW_STATE_VISIBLE) != 0;
  evt.presentation.minimized = (flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
  evt.presentation.maximized = (flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
  evt.presentation.focused = (flags & MARU_WINDOW_STATE_FOCUSED) != 0;
  evt.presentation.icon = icon;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED,
                       (MARU_Window *)window, &evt);
}


void _maru_wayland_update_opaque_region(MARU_Window_WL *window) {
  MARU_ASSUME(window != NULL);
  if (!window->wl.surface) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
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

