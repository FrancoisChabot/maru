// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <stdatomic.h>
#include <string.h>

static uint32_t _maru_x11_mode_refresh_mhz(const XRRModeInfo *mode_info) {
  if (!mode_info || mode_info->hTotal == 0 || mode_info->vTotal == 0 ||
      mode_info->dotClock == 0) {
    return 0;
  }
  const uint64_t num = (uint64_t)mode_info->dotClock * 1000ull;
  const uint64_t den = (uint64_t)mode_info->hTotal * (uint64_t)mode_info->vTotal;
  if (den == 0) {
    return 0;
  }
  return (uint32_t)(num / den);
}

static const XRRModeInfo *_maru_x11_find_mode_info(const XRRScreenResources *resources,
                                                    RRMode id) {
  if (!resources) {
    return NULL;
  }
  for (int i = 0; i < resources->nmode; ++i) {
    if (resources->modes[i].id == id) {
      return &resources->modes[i];
    }
  }
  return NULL;
}

static void _maru_x11_destroy_monitor(MARU_Monitor_X11 *monitor) {
  if (monitor->modes) {
    maru_context_free(monitor->base.ctx_base, monitor->modes);
    monitor->modes = NULL;
    monitor->mode_count = 0;
  }
  if (monitor->name_storage) {
    maru_context_free(monitor->base.ctx_base, monitor->name_storage);
    monitor->name_storage = NULL;
  }
  maru_context_free(monitor->base.ctx_base, monitor);
}

void _maru_x11_refresh_monitors(MARU_Context_X11 *ctx) {
  if (!ctx->xrandr_lib.base.available) {
    return;
  }

  for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
    ((MARU_Monitor_Base *)ctx->base.monitor_cache[i])->is_active = false;
  }

  XRRScreenResources *resources =
      ctx->xrandr_lib.XRRGetScreenResourcesCurrent(ctx->display, ctx->root);
  if (!resources) {
    return;
  }

  int nmonitors = 0;
  XRRMonitorInfo *monitors = ctx->xrandr_lib.XRRGetMonitors(
      ctx->display, ctx->root, True, &nmonitors);
  const RROutput primary_output =
      ctx->xrandr_lib.XRRGetOutputPrimary(ctx->display, ctx->root);

  if (monitors) {
    for (int i = 0; i < nmonitors; ++i) {
      XRRMonitorInfo *info = &monitors[i];
      if (info->noutput <= 0 || !info->outputs) {
        continue;
      }

      const RROutput output = info->outputs[0];
      MARU_Monitor_X11 *monitor = NULL;
      for (uint32_t j = 0; j < ctx->base.monitor_cache_count; ++j) {
        MARU_Monitor_X11 *candidate = (MARU_Monitor_X11 *)ctx->base.monitor_cache[j];
        if (candidate->output == output) {
          monitor = candidate;
          break;
        }
      }

      if (!monitor) {
        monitor = (MARU_Monitor_X11 *)maru_context_alloc(
            &ctx->base, sizeof(MARU_Monitor_X11));
        if (!monitor) {
          continue;
        }
        memset(monitor, 0, sizeof(*monitor));
        monitor->base.ctx_base = &ctx->base;
        monitor->base.pub.context = (MARU_Context *)ctx;
        monitor->base.pub.metrics = &monitor->base.metrics;
        monitor->base.pub.scale = (MARU_Scalar)1.0;
        atomic_init(&monitor->base.ref_count, 1u);
#ifdef MARU_INDIRECT_BACKEND
        monitor->base.backend = ctx->base.backend;
#endif
        monitor->output = output;

        if (ctx->base.monitor_cache_count >= ctx->base.monitor_cache_capacity) {
          const uint32_t old_cap = ctx->base.monitor_cache_capacity;
          const uint32_t new_cap = old_cap ? (old_cap * 2u) : 4u;
          MARU_Monitor **new_cache = (MARU_Monitor **)maru_context_realloc(
              &ctx->base, ctx->base.monitor_cache,
              (size_t)old_cap * sizeof(MARU_Monitor *),
              (size_t)new_cap * sizeof(MARU_Monitor *));
          if (!new_cache) {
            _maru_x11_destroy_monitor(monitor);
            continue;
          }
          ctx->base.monitor_cache = new_cache;
          ctx->base.monitor_cache_capacity = new_cap;
        }
        ctx->base.monitor_cache[ctx->base.monitor_cache_count++] =
            (MARU_Monitor *)monitor;
      }

      monitor->base.is_active = true;
      monitor->base.pub.flags &= ~((uint64_t)MARU_MONITOR_STATE_LOST);
      monitor->base.pub.is_primary = (output == primary_output);
      monitor->base.pub.logical_position.x = (MARU_Scalar)info->x;
      monitor->base.pub.logical_position.y = (MARU_Scalar)info->y;
      monitor->base.pub.logical_size.x = (MARU_Scalar)info->width;
      monitor->base.pub.logical_size.y = (MARU_Scalar)info->height;
      monitor->base.pub.physical_size.x = (MARU_Scalar)info->mwidth;
      monitor->base.pub.physical_size.y = (MARU_Scalar)info->mheight;

      XRROutputInfo *output_info =
          ctx->xrandr_lib.XRRGetOutputInfo(ctx->display, resources, output);
      if (!output_info) {
        continue;
      }

      monitor->output = output;
      if (output_info->nameLen > 0 && output_info->name) {
        const size_t name_len = (size_t)output_info->nameLen;
        char *name_copy = (char *)maru_context_alloc(&ctx->base, name_len + 1u);
        if (name_copy) {
          memcpy(name_copy, output_info->name, name_len);
          name_copy[name_len] = '\0';
          if (monitor->name_storage) {
            maru_context_free(&ctx->base, monitor->name_storage);
          }
          monitor->name_storage = name_copy;
          monitor->base.pub.name = monitor->name_storage;
        }
      }

      if (monitor->modes) {
        maru_context_free(&ctx->base, monitor->modes);
        monitor->modes = NULL;
        monitor->mode_count = 0;
      }
      if (output_info->nmode > 0 && output_info->modes) {
        monitor->modes = (MARU_VideoMode *)maru_context_alloc(
            &ctx->base, sizeof(MARU_VideoMode) * (size_t)output_info->nmode);
        if (monitor->modes) {
          for (int mode_i = 0; mode_i < output_info->nmode; ++mode_i) {
            const XRRModeInfo *mode_info =
                _maru_x11_find_mode_info(resources, output_info->modes[mode_i]);
            if (!mode_info) {
              continue;
            }
            bool duplicate = false;
            const MARU_VideoMode candidate = {
                .size = {(int32_t)mode_info->width, (int32_t)mode_info->height},
                .refresh_rate_mhz = _maru_x11_mode_refresh_mhz(mode_info)};
            for (uint32_t k = 0; k < monitor->mode_count; ++k) {
              if (monitor->modes[k].size.x == candidate.size.x &&
                  monitor->modes[k].size.y == candidate.size.y &&
                  monitor->modes[k].refresh_rate_mhz == candidate.refresh_rate_mhz) {
                duplicate = true;
                break;
              }
            }
            if (!duplicate) {
              monitor->modes[monitor->mode_count++] = candidate;
            }
          }
        }
      }

      monitor->crtc = output_info->crtc;
      if (output_info->crtc) {
        XRRCrtcInfo *crtc_info = ctx->xrandr_lib.XRRGetCrtcInfo(
            ctx->display, resources, output_info->crtc);
        if (crtc_info) {
          monitor->crtc = output_info->crtc;
          monitor->current_mode_id = crtc_info->mode;
          monitor->current_rotation = crtc_info->rotation;
          const XRRModeInfo *mode_info =
              _maru_x11_find_mode_info(resources, crtc_info->mode);
          if (mode_info) {
            monitor->base.pub.current_mode.size.x = (int32_t)mode_info->width;
            monitor->base.pub.current_mode.size.y = (int32_t)mode_info->height;
            monitor->base.pub.current_mode.refresh_rate_mhz =
                _maru_x11_mode_refresh_mhz(mode_info);
          }
          ctx->xrandr_lib.XRRFreeCrtcInfo(crtc_info);
        }
      }

      ctx->xrandr_lib.XRRFreeOutputInfo(output_info);
    }
    ctx->xrandr_lib.XRRFreeMonitors(monitors);
  }

  ctx->xrandr_lib.XRRFreeScreenResources(resources);

  for (uint32_t i = 0; i < ctx->base.monitor_cache_count;) {
    MARU_Monitor_Base *monitor = (MARU_Monitor_Base *)ctx->base.monitor_cache[i];
    if (monitor->is_active) {
      ++i;
      continue;
    }
    monitor->pub.flags |= MARU_MONITOR_STATE_LOST;
    for (uint32_t j = i; j + 1u < ctx->base.monitor_cache_count; ++j) {
      ctx->base.monitor_cache[j] = ctx->base.monitor_cache[j + 1u];
    }
    ctx->base.monitor_cache_count--;
    maru_releaseMonitor_X11((MARU_Monitor *)monitor);
  }
}

MARU_Monitor *const *maru_getMonitors_X11(MARU_Context *context, uint32_t *out_count) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_x11_refresh_monitors(ctx);
  *out_count = ctx->base.monitor_cache_count;
  return ctx->base.monitor_cache;
}

void maru_retainMonitor_X11(MARU_Monitor *monitor) {
  MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
  atomic_fetch_add_explicit(&base->ref_count, 1u, memory_order_relaxed);
}

void maru_releaseMonitor_X11(MARU_Monitor *monitor) {
  MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
  uint32_t current =
      atomic_load_explicit(&base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&base->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u && !base->is_active) {
        _maru_x11_destroy_monitor((MARU_Monitor_X11 *)monitor);
      }
      return;
    }
  }
}

const MARU_VideoMode *maru_getMonitorModes_X11(const MARU_Monitor *monitor, uint32_t *out_count) {
  const MARU_Monitor_X11 *mon = (const MARU_Monitor_X11 *)monitor;
  *out_count = mon->mode_count;
  return mon->modes;
}

MARU_Status maru_setMonitorMode_X11(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_Monitor_X11 *mon = (MARU_Monitor_X11 *)monitor;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)mon->base.ctx_base;
  if (!ctx->xrandr_lib.base.available || mon->output == (RROutput)0) {
    return MARU_FAILURE;
  }

  XRRScreenResources *resources =
      ctx->xrandr_lib.XRRGetScreenResourcesCurrent(ctx->display, ctx->root);
  if (!resources) {
    return MARU_FAILURE;
  }
  XRROutputInfo *output_info =
      ctx->xrandr_lib.XRRGetOutputInfo(ctx->display, resources, mon->output);
  if (!output_info) {
    ctx->xrandr_lib.XRRFreeScreenResources(resources);
    return MARU_FAILURE;
  }

  RRMode target_mode = (RRMode)0;
  for (int i = 0; i < output_info->nmode; ++i) {
    const XRRModeInfo *mode_info =
        _maru_x11_find_mode_info(resources, output_info->modes[i]);
    if (!mode_info) {
      continue;
    }
    if ((int32_t)mode_info->width != mode.size.x ||
        (int32_t)mode_info->height != mode.size.y) {
      continue;
    }
    const uint32_t refresh_mhz = _maru_x11_mode_refresh_mhz(mode_info);
    if (mode.refresh_rate_mhz == 0 || refresh_mhz == mode.refresh_rate_mhz) {
      target_mode = mode_info->id;
      mode.refresh_rate_mhz = refresh_mhz;
      break;
    }
  }
  if (target_mode == (RRMode)0) {
    ctx->xrandr_lib.XRRFreeOutputInfo(output_info);
    ctx->xrandr_lib.XRRFreeScreenResources(resources);
    return MARU_FAILURE;
  }

  const RRCrtc crtc = mon->crtc ? mon->crtc : output_info->crtc;
  if (!crtc) {
    ctx->xrandr_lib.XRRFreeOutputInfo(output_info);
    ctx->xrandr_lib.XRRFreeScreenResources(resources);
    return MARU_FAILURE;
  }

  XRRCrtcInfo *crtc_info = ctx->xrandr_lib.XRRGetCrtcInfo(ctx->display, resources, crtc);
  if (!crtc_info) {
    ctx->xrandr_lib.XRRFreeOutputInfo(output_info);
    ctx->xrandr_lib.XRRFreeScreenResources(resources);
    return MARU_FAILURE;
  }

  RROutput outputs[1];
  outputs[0] = mon->output;
  const Status set_status = ctx->xrandr_lib.XRRSetCrtcConfig(
      ctx->display, resources, crtc, CurrentTime, crtc_info->x, crtc_info->y,
      target_mode, crtc_info->rotation, outputs, 1);
  ctx->xrandr_lib.XRRFreeCrtcInfo(crtc_info);
  ctx->xrandr_lib.XRRFreeOutputInfo(output_info);
  ctx->xrandr_lib.XRRFreeScreenResources(resources);
  if (set_status != RRSetConfigSuccess) {
    return MARU_FAILURE;
  }

  mon->crtc = crtc;
  mon->current_mode_id = target_mode;
  mon->base.pub.current_mode = mode;
  ctx->x11_lib.XSync(ctx->display, False);
  _maru_x11_refresh_monitors(ctx);
  return MARU_SUCCESS;
}

void maru_resetMonitorMetrics_X11(MARU_Monitor *monitor) {
  (void)monitor;
}
