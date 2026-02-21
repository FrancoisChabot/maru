#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static void _output_handle_geometry(void *data, struct wl_output *wl_output,
                                    int32_t x, int32_t y, int32_t physical_width,
                                    int32_t physical_height, int32_t subpixel,
                                    const char *make, const char *model,
                                    int32_t transform) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.logical_position.x = (MARU_Scalar)x;
  monitor->base.pub.logical_position.y = (MARU_Scalar)y;
  monitor->base.pub.physical_size.x = (MARU_Scalar)physical_width;
  monitor->base.pub.physical_size.y = (MARU_Scalar)physical_height;
  (void)wl_output; (void)subpixel; (void)make; (void)model; (void)transform;
}

static void _output_handle_mode(void *data, struct wl_output *wl_output,
                                uint32_t flags, int32_t width, int32_t height,
                                int32_t refresh) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  if (monitor->mode_count >= monitor->mode_capacity) {
    uint32_t old_cap = monitor->mode_capacity;
    uint32_t new_cap = old_cap ? old_cap * 2 : 4;
    monitor->modes = maru_context_realloc(&ctx->base, monitor->modes, old_cap * sizeof(MARU_VideoMode), new_cap * sizeof(MARU_VideoMode));
    monitor->mode_capacity = new_cap;
  }

  MARU_VideoMode mode = {
    .size = {(int32_t)width, (int32_t)height},
    .refresh_rate_mhz = (uint32_t)refresh
  };

  monitor->modes[monitor->mode_count++] = mode;

  if (flags & WL_OUTPUT_MODE_CURRENT) {
    monitor->current_mode = mode;
    monitor->base.pub.current_mode = mode;
    
    // Fallback logical size if zxdg_output is not available
    if (!monitor->xdg_output) {
      monitor->base.pub.logical_size.x = (MARU_Scalar)width / monitor->base.pub.scale;
      monitor->base.pub.logical_size.y = (MARU_Scalar)height / monitor->base.pub.scale;
    }
  }
  (void)wl_output;
}

static void _output_handle_done(void *data, struct wl_output *wl_output) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  MARU_Event evt = {0};
  evt.monitor_mode.monitor = (MARU_Monitor *)monitor;
  _maru_dispatch_event(&ctx->base, MARU_MONITOR_MODE_CHANGED, NULL, &evt);
  (void)wl_output;
}

static void _output_handle_scale(void *data, struct wl_output *wl_output,
                                 int32_t factor) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  monitor->scale = (MARU_Scalar)factor;
  monitor->base.pub.scale = (MARU_Scalar)factor;

  // Update logical size fallback
  if (!monitor->xdg_output && monitor->current_mode.size.x > 0) {
    monitor->base.pub.logical_size.x = (MARU_Scalar)monitor->current_mode.size.x / monitor->base.pub.scale;
    monitor->base.pub.logical_size.y = (MARU_Scalar)monitor->current_mode.size.y / monitor->base.pub.scale;
  }

  (void)wl_output;
}

static const struct wl_output_listener _output_listener = {
    .geometry = _output_handle_geometry,
    .mode = _output_handle_mode,
    .done = _output_handle_done,
    .scale = _output_handle_scale,
};

static void _xdg_output_handle_logical_position(void *data, struct zxdg_output_v1 *xdg_output,
                                                 int32_t x, int32_t y) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.logical_position.x = (MARU_Scalar)x;
  monitor->base.pub.logical_position.y = (MARU_Scalar)y;
  (void)xdg_output;
}

static void _xdg_output_handle_logical_size(void *data, struct zxdg_output_v1 *xdg_output,
                                             int32_t width, int32_t height) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  monitor->base.pub.logical_size.x = (MARU_Scalar)width;
  monitor->base.pub.logical_size.y = (MARU_Scalar)height;
  (void)xdg_output;
}

static void _xdg_output_handle_done(void *data, struct zxdg_output_v1 *xdg_output) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  MARU_Event evt = {0};
  evt.monitor_mode.monitor = (MARU_Monitor *)monitor;
  _maru_dispatch_event(&ctx->base, MARU_MONITOR_MODE_CHANGED, NULL, &evt);
  (void)xdg_output;
}

static void _xdg_output_handle_name(void *data, struct zxdg_output_v1 *xdg_output,
                                    const char *name) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)data;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  if (monitor->base.pub.name) {
    maru_context_free(&ctx->base, (void *)monitor->base.pub.name);
  }
  
  size_t len = strlen(name);
  char *new_name = maru_context_alloc(&ctx->base, len + 1);
  memcpy(new_name, name, len + 1);
  monitor->base.pub.name = new_name;
  (void)xdg_output;
}

static void _xdg_output_handle_description(void *data, struct zxdg_output_v1 *xdg_output,
                                           const char *description) {
  (void)data; (void)xdg_output; (void)description;
}

static const struct zxdg_output_v1_listener _xdg_output_listener = {
    .logical_position = _xdg_output_handle_logical_position,
    .logical_size = _xdg_output_handle_logical_size,
    .done = _xdg_output_handle_done,
    .name = _xdg_output_handle_name,
    .description = _xdg_output_handle_description,
};

void _maru_wayland_register_xdg_output(MARU_Context_WL *ctx, MARU_Monitor_WL *monitor) {
  if (ctx->protocols.opt.zxdg_output_manager_v1 && !monitor->xdg_output) {
    monitor->xdg_output = maru_zxdg_output_manager_v1_get_xdg_output(
        ctx, ctx->protocols.opt.zxdg_output_manager_v1, monitor->output);
    maru_zxdg_output_v1_add_listener(ctx, monitor->xdg_output, &_xdg_output_listener, monitor);
  }
}

void _maru_wayland_bind_output(MARU_Context_WL *ctx, uint32_t name, uint32_t version) {
  MARU_Monitor_WL *monitor = maru_context_alloc(&ctx->base, sizeof(MARU_Monitor_WL));
  memset(monitor, 0, sizeof(MARU_Monitor_WL));

  monitor->base.ctx_base = &ctx->base;
  monitor->base.pub.context = (MARU_Context *)ctx;
#ifdef MARU_INDIRECT_BACKEND
  monitor->base.backend = ctx->base.backend;
#endif
  monitor->base.is_active = true;
  monitor->name = name;
  monitor->scale = 1.0;
  monitor->base.pub.scale = 1.0;
  monitor->base.pub.is_primary = (ctx->base.monitor_cache_count == 0);

  uint32_t bind_version = (version < 4) ? version : 4; 
  monitor->output = maru_wl_registry_bind(ctx, ctx->wl.registry, name, &wl_output_interface, bind_version);
  
  if (monitor->output) {
    maru_wl_output_add_listener(ctx, monitor->output, &_output_listener, monitor);
    _maru_wayland_register_xdg_output(ctx, monitor);
    
    // Add to context cache
    if (ctx->base.monitor_cache_count >= ctx->base.monitor_cache_capacity) {
      uint32_t old_cap = ctx->base.monitor_cache_capacity;
      uint32_t new_cap = old_cap ? old_cap * 2 : 4;
      ctx->base.monitor_cache = maru_context_realloc(&ctx->base, ctx->base.monitor_cache, old_cap * sizeof(MARU_Monitor *), new_cap * sizeof(MARU_Monitor *));
      ctx->base.monitor_cache_capacity = new_cap;
    }
    ctx->base.monitor_cache[ctx->base.monitor_cache_count++] = (MARU_Monitor *)monitor;

    MARU_Event evt = {0};
    evt.monitor_connection.monitor = (MARU_Monitor *)monitor;
    evt.monitor_connection.connected = true;
    _maru_dispatch_event(&ctx->base, MARU_MONITOR_CONNECTION_CHANGED, NULL, &evt);
  } else {
    maru_context_free(&ctx->base, monitor);
  }
}

void _maru_wayland_remove_output(MARU_Context_WL *ctx, uint32_t name) {
  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; i++) {
    MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
    if (monitor->name == name) {
      monitor->base.is_active = false;
      monitor->base.pub.flags |= MARU_MONITOR_STATE_LOST;
      
      MARU_Event evt = {0};
      evt.monitor_connection.monitor = (MARU_Monitor *)monitor;
      evt.monitor_connection.connected = false;
      _maru_dispatch_event(&ctx->base, MARU_MONITOR_CONNECTION_CHANGED, NULL, &evt);

      if (monitor->xdg_output) {
        maru_zxdg_output_v1_destroy(ctx, monitor->xdg_output);
        monitor->xdg_output = NULL;
      }
      maru_wl_output_destroy(ctx, monitor->output);
      monitor->output = NULL;
      
      // Remove from cache
      for (uint32_t j = i; j < ctx->base.monitor_cache_count - 1; j++) {
        ctx->base.monitor_cache[j] = ctx->base.monitor_cache[j + 1];
      }
      ctx->base.monitor_cache_count--;
      
      maru_releaseMonitor((MARU_Monitor *)monitor);
      return;
    }
  }
}

MARU_Monitor *const *maru_getMonitors_WL(MARU_Context *context, uint32_t *out_count) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  *out_count = ctx->base.monitor_cache_count;
  return ctx->base.monitor_cache;
}

MARU_Status maru_updateMonitors_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    return MARU_FAILURE;
  }
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

void maru_destroyMonitor_WL(MARU_Monitor *monitor_handle) {
  MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)monitor_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)monitor->base.ctx_base;

  if (monitor->base.pub.name) {
    maru_context_free(&ctx->base, (void *)monitor->base.pub.name);
  }
  if (monitor->modes) {
    maru_context_free(&ctx->base, monitor->modes);
  }
  if (monitor->xdg_output) {
    maru_zxdg_output_v1_destroy(ctx, monitor->xdg_output);
  }
  if (monitor->output) {
    maru_wl_output_destroy(ctx, monitor->output);
  }
  maru_context_free(&ctx->base, monitor);
}

