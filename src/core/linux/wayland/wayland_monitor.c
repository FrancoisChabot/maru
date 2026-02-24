#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void _maru_wayland_set_monitor_name(MARU_Monitor_WL *monitor,
                                           const char *name) {
  if (!name) {
    return;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;
  const size_t len = strlen(name);
  char *copy = (char *)maru_context_alloc(&ctx->base, len + 1);
  if (!copy) {
    return;
  }
  memcpy(copy, name, len + 1);

  if (monitor->name_storage) {
    maru_context_free(&ctx->base, monitor->name_storage);
  }

  monitor->name_storage = copy;
  monitor->base.pub.name = monitor->name_storage;
}

static void _maru_wayland_recompute_primary(MARU_Context_WL *ctx) {
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
    monitor->base.pub.is_primary = (i == 0);
  }
}

static void _output_handle_geometry(void *data, struct wl_output *wl_output,
                                    int32_t x, int32_t y, int32_t physical_width,
                                    int32_t physical_height, int32_t subpixel,
                                    const char *make, const char *model,
                                    int32_t transform) {
  (void)wl_output;
  (void)x;
  (void)y;
  (void)subpixel;
  (void)transform;

  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;

  monitor->base.pub.physical_size.x = (MARU_Scalar)physical_width;
  monitor->base.pub.physical_size.y = (MARU_Scalar)physical_height;

  if (!monitor->base.pub.name && make && model) {
    char full_name[512];
    snprintf(full_name, sizeof(full_name), "%s %s", make, model);
    _maru_wayland_set_monitor_name(monitor, full_name);
  }
}

static void _output_handle_mode(void *data, struct wl_output *wl_output,
                                uint32_t flags, int32_t width, int32_t height,
                                int32_t refresh) {
  (void)wl_output;

  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  MARU_VideoMode mode = {
      .size = {.x = width, .y = height},
      .refresh_rate_mhz = (uint32_t)refresh,
  };

  if ((flags & WL_OUTPUT_MODE_CURRENT) != 0U) {
    const MARU_VideoMode prev_mode = monitor->base.pub.current_mode;
    monitor->base.pub.current_mode = mode;
    monitor->current_mode = mode;
    if (prev_mode.size.x != mode.size.x || prev_mode.size.y != mode.size.y ||
        prev_mode.refresh_rate_mhz != mode.refresh_rate_mhz) {
      monitor->mode_changed_pending = true;
    }
  }

  for (uint32_t i = 0; i < monitor->mode_count; ++i) {
    const MARU_VideoMode existing = monitor->modes[i];
    if (existing.size.x == mode.size.x && existing.size.y == mode.size.y &&
        existing.refresh_rate_mhz == mode.refresh_rate_mhz) {
      return;
    }
  }

  if (monitor->mode_count >= monitor->mode_capacity) {
    const uint32_t old_capacity = monitor->mode_capacity;
    const uint32_t new_capacity = old_capacity ? old_capacity * 2 : 8;
    MARU_VideoMode *new_modes = (MARU_VideoMode *)maru_context_realloc(
        &ctx->base, monitor->modes, old_capacity * sizeof(MARU_VideoMode),
        new_capacity * sizeof(MARU_VideoMode));
    if (!new_modes) {
      return;
    }
    monitor->modes = new_modes;
    monitor->mode_capacity = new_capacity;
  }

  monitor->modes[monitor->mode_count++] = mode;
}

static void _output_handle_done(void *data, struct wl_output *wl_output) {
  (void)wl_output;

  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  if (monitor->base.pub.logical_size.x == 0 &&
      monitor->base.pub.current_mode.size.x > 0) {
    const MARU_Scalar scale =
        (monitor->base.pub.scale > (MARU_Scalar)0.0)
            ? monitor->base.pub.scale
            : (MARU_Scalar)1.0;
    monitor->base.pub.logical_size.x =
        (MARU_Scalar)monitor->base.pub.current_mode.size.x / scale;
    monitor->base.pub.logical_size.y =
        (MARU_Scalar)monitor->base.pub.current_mode.size.y / scale;
  }

  MARU_Event evt = {0};
  if (!monitor->announced) {
    monitor->announced = true;
    monitor->mode_changed_pending = false;
    evt.monitor_connection.monitor = (MARU_Monitor *)monitor;
    evt.monitor_connection.connected = true;
    _maru_dispatch_event(&ctx->base, MARU_MONITOR_CONNECTION_CHANGED, NULL, &evt);
    return;
  }

  if (!monitor->mode_changed_pending) {
    return;
  }
  monitor->mode_changed_pending = false;
  evt.monitor_mode.monitor = (MARU_Monitor *)monitor;
  _maru_dispatch_event(&ctx->base, MARU_MONITOR_MODE_CHANGED, NULL, &evt);
}

static void _output_handle_scale(void *data, struct wl_output *wl_output,
                                 int32_t factor) {
  (void)wl_output;
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.scale = (MARU_Scalar)factor;
  monitor->scale = (MARU_Scalar)factor;
}

static void _output_handle_name(void *data, struct wl_output *wl_output,
                                const char *name) {
  (void)wl_output;
  _maru_wayland_set_monitor_name((MARU_Monitor_WL *)data, name);
}

static void _output_handle_description(void *data, struct wl_output *wl_output,
                                       const char *description) {
  _output_handle_name(data, wl_output, description);
}

static void _xdg_output_handle_logical_position(void *data,
                                                struct zxdg_output_v1 *xdg_output,
                                                int32_t x, int32_t y) {
  (void)xdg_output;
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.logical_position.x = (MARU_Scalar)x;
  monitor->base.pub.logical_position.y = (MARU_Scalar)y;
}

static void _xdg_output_handle_logical_size(void *data,
                                            struct zxdg_output_v1 *xdg_output,
                                            int32_t width, int32_t height) {
  (void)xdg_output;
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.logical_size.x = (MARU_Scalar)width;
  monitor->base.pub.logical_size.y = (MARU_Scalar)height;
}

static void _xdg_output_handle_done(void *data, struct zxdg_output_v1 *xdg_output) {
  (void)data;
  (void)xdg_output;
}

static void _xdg_output_handle_name(void *data,
                                    struct zxdg_output_v1 *xdg_output,
                                    const char *name) {
  (void)xdg_output;
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  _maru_wayland_set_monitor_name(monitor, name);
}

static void _xdg_output_handle_description(void *data,
                                           struct zxdg_output_v1 *xdg_output,
                                           const char *description) {
  (void)xdg_output;
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  if (!monitor->base.pub.name || monitor->base.pub.name[0] == '\0') {
    _maru_wayland_set_monitor_name(monitor, description);
  }
}

static const struct zxdg_output_v1_listener _maru_wayland_xdg_output_listener = {
    .logical_position = _xdg_output_handle_logical_position,
    .logical_size = _xdg_output_handle_logical_size,
    .done = _xdg_output_handle_done,
    .name = _xdg_output_handle_name,
    .description = _xdg_output_handle_description,
};

const struct wl_output_listener _maru_wayland_output_listener = {
    .geometry = _output_handle_geometry,
    .mode = _output_handle_mode,
    .done = _output_handle_done,
    .scale = _output_handle_scale,
    .name = _output_handle_name,
    .description = _output_handle_description,
};

static void _maru_wayland_release_output_proxy(MARU_Context_WL *ctx,
                                               struct wl_output *output) {
  if (!output) {
    return;
  }

  if (maru_wl_output_get_version(ctx, output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
    maru_wl_output_release(ctx, output);
    return;
  }
  maru_wl_output_destroy(ctx, output);
}

static void _maru_wayland_destroy_monitor(MARU_Monitor_WL *monitor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  if (monitor->xdg_output) {
    maru_zxdg_output_v1_destroy(ctx, monitor->xdg_output);
    monitor->xdg_output = NULL;
  }
  if (monitor->output) {
    _maru_wayland_release_output_proxy(ctx, monitor->output);
    monitor->output = NULL;
  }
  if (monitor->modes) {
    maru_context_free(&ctx->base, monitor->modes);
    monitor->modes = NULL;
  }
  if (monitor->name_storage) {
    maru_context_free(&ctx->base, monitor->name_storage);
    monitor->name_storage = NULL;
  }

  maru_context_free(&ctx->base, monitor);
}

void _maru_wayland_bind_output(MARU_Context_WL *ctx, uint32_t name, uint32_t version) {
  const uint32_t bind_version = (version < 4U) ? version : 4U;
  struct wl_output *output = (struct wl_output *)maru_wl_registry_bind(
      ctx, ctx->wl.registry, name, &wl_output_interface, bind_version);
  if (!output) {
    return;
  }

  MARU_Monitor_WL *monitor =
      (MARU_Monitor_WL *)maru_context_alloc(&ctx->base, sizeof(MARU_Monitor_WL));
  if (!monitor) {
    maru_wl_output_destroy(ctx, output);
    return;
  }

  memset(monitor, 0, sizeof(MARU_Monitor_WL));
  monitor->base.ctx_base = &ctx->base;
  monitor->base.pub.context = (MARU_Context *)ctx;
  monitor->base.pub.metrics = &monitor->base.metrics;
  atomic_init(&monitor->base.ref_count, 1u);
  monitor->base.is_active = true;
#ifdef MARU_INDIRECT_BACKEND
  monitor->base.backend = ctx->base.backend;
#endif
  monitor->output = output;
  monitor->name = name;
  monitor->base.pub.scale = 1.0f;
  monitor->scale = 1.0f;

  if (ctx->base.monitor_cache_count >= ctx->base.monitor_cache_capacity) {
    const uint32_t old_capacity = ctx->base.monitor_cache_capacity;
    const uint32_t new_capacity = old_capacity ? old_capacity * 2 : 4;
    MARU_Monitor **new_cache = (MARU_Monitor **)maru_context_realloc(
        &ctx->base, ctx->base.monitor_cache,
        old_capacity * sizeof(MARU_Monitor *),
        new_capacity * sizeof(MARU_Monitor *));
    if (!new_cache) {
      _maru_wayland_destroy_monitor(monitor);
      return;
    }
    ctx->base.monitor_cache = new_cache;
    ctx->base.monitor_cache_capacity = new_capacity;
  }

  ctx->base.monitor_cache[ctx->base.monitor_cache_count++] = (MARU_Monitor *)monitor;
  _maru_wayland_recompute_primary(ctx);

  maru_wl_output_add_listener(ctx, output, &_maru_wayland_output_listener, monitor);
  _maru_wayland_register_xdg_output(ctx, monitor);
}

void _maru_wayland_remove_output(MARU_Context_WL *ctx, uint32_t name) {
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
    if (monitor->name != name) {
      continue;
    }

    for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
      MARU_Window_WL *window = (MARU_Window_WL *)it;
      _maru_wayland_window_drop_monitor(window, (MARU_Monitor *)monitor);
    }

    monitor->base.pub.flags |= MARU_MONITOR_STATE_LOST;
    monitor->base.is_active = false;

    MARU_Event evt = {0};
    evt.monitor_connection.monitor = (MARU_Monitor *)monitor;
    evt.monitor_connection.connected = false;
    _maru_dispatch_event(&ctx->base, MARU_MONITOR_CONNECTION_CHANGED, NULL, &evt);

    for (uint32_t j = i; j + 1 < ctx->base.monitor_cache_count; ++j) {
      ctx->base.monitor_cache[j] = ctx->base.monitor_cache[j + 1];
    }
    ctx->base.monitor_cache_count--;
    _maru_wayland_recompute_primary(ctx);
    maru_releaseMonitor((MARU_Monitor *)monitor);
    return;
  }
}

void _maru_wayland_register_xdg_output(MARU_Context_WL *ctx,
                                       MARU_Monitor_WL *monitor) {
  if (!ctx->protocols.opt.zxdg_output_manager_v1 || !monitor->output) {
    return;
  }

  monitor->xdg_output = maru_zxdg_output_manager_v1_get_xdg_output(
      ctx, ctx->protocols.opt.zxdg_output_manager_v1, monitor->output);
  if (!monitor->xdg_output) {
    return;
  }

  maru_zxdg_output_v1_add_listener(ctx, monitor->xdg_output,
                                   &_maru_wayland_xdg_output_listener, monitor);
}

MARU_Monitor *const *maru_getMonitors_WL(MARU_Context *context, uint32_t *out_count) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  *out_count = ctx->base.monitor_cache_count;
  return ctx->base.monitor_cache;
}

void maru_retainMonitor_WL(MARU_Monitor *monitor) {
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  atomic_fetch_add_explicit(&mon_base->ref_count, 1u, memory_order_relaxed);
}

void maru_releaseMonitor_WL(MARU_Monitor *monitor) {
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  uint32_t current = atomic_load_explicit(&mon_base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&mon_base->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u && !mon_base->is_active) {
        maru_destroyMonitor_WL(monitor);
      }
      return;
    }
  }
}

MARU_Status maru_updateMonitors_WL(MARU_Context *context) {
  (void)context;
  return MARU_SUCCESS;
}

const MARU_VideoMode *maru_getMonitorModes_WL(const MARU_Monitor *monitor_handle, uint32_t *out_count) {
  const MARU_Monitor_WL *monitor = (const MARU_Monitor_WL *)monitor_handle;
  *out_count = monitor->mode_count;
  return monitor->modes;
}

MARU_Status maru_setMonitorMode_WL(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  (void)monitor; (void)mode;
  return MARU_FAILURE; // Wayland doesn't support setting modes directly
}

void maru_destroyMonitor_WL(MARU_Monitor *monitor) {
  _maru_wayland_destroy_monitor((MARU_Monitor_WL *)monitor);
}
