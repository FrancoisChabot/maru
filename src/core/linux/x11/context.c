// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "linux_internal.h"
#include "x11_internal.h"
#include "dlib/loader.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                    const MARU_ContextAttributes *attributes) {
  (void)context;
  (void)field_mask;
  (void)attributes;
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context);

MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_X11));
  if (!ctx) {
    return MARU_FAILURE;
  }

  ctx->base.pub.backend_type = MARU_BACKEND_X11;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }
  ctx->base.tuning = create_info->tuning;
  _maru_init_context_base(&ctx->base);
#ifdef MARU_GATHER_METRICS
  _maru_update_mem_metrics_alloc(&ctx->base, sizeof(MARU_Context_X11));
#endif

  if (!maru_load_x11_symbols(&ctx->base, &ctx->x11_lib)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->display = ctx->x11_lib.XOpenDisplay(NULL);
  if (!ctx->display) {
    maru_unload_x11_symbols(&ctx->x11_lib);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->screen = DefaultScreen(ctx->display);
  ctx->root = RootWindow(ctx->display, ctx->screen);

  ctx->wm_protocols = ctx->x11_lib.XInternAtom(ctx->display, "WM_PROTOCOLS", False);
  ctx->wm_delete_window = ctx->x11_lib.XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
  ctx->net_wm_name = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_NAME", False);
  ctx->net_wm_icon_name = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_ICON_NAME", False);
  ctx->net_wm_state = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE", False);
  ctx->net_wm_state_fullscreen = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_FULLSCREEN", False);
  ctx->net_wm_state_maximized_vert = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  ctx->net_wm_state_maximized_horz = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  ctx->net_wm_state_demands_attention = ctx->x11_lib.XInternAtom(ctx->display, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
  ctx->net_active_window = ctx->x11_lib.XInternAtom(ctx->display, "_NET_ACTIVE_WINDOW", False);

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    ctx->x11_lib.XCloseDisplay(ctx->display);
    maru_unload_x11_symbols(&ctx->x11_lib);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }
  
  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_mask = create_info->attributes.event_mask;
  ctx->base.inhibit_idle = create_info->attributes.inhibit_idle;

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_X11;
  ctx->base.backend = &maru_backend_X11;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  if (!_maru_linux_common_run(&ctx->linux_common)) {
    maru_destroyContext_X11((MARU_Context *)ctx);
    return MARU_FAILURE;
  }

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_linux_common_cleanup(&ctx->linux_common);
  
  if (ctx->display) {
    ctx->x11_lib.XCloseDisplay(ctx->display);
  }
  maru_unload_x11_symbols(&ctx->x11_lib);

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  uint64_t val = 1;
  if (write(ctx->linux_common.worker.event_fd, &val, sizeof(val)) < 0) {
      return MARU_FAILURE;
  }
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  (void)callback;
  (void)userdata;

  // 1. Drain hotplug events from worker thread
  _maru_linux_common_drain_internal_events(&ctx->linux_common);

  // 2. Process already pending X11 events
  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      // TODO: Process X11 event
  }

  // 3. Prepare poll set
  struct pollfd pfds[32];
  uint32_t nfds = 0;

  // Slot 0: Wake FD
  pfds[nfds].fd = ctx->linux_common.worker.event_fd;
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;
  nfds++;

  // Slot 1: X11 FD
  pfds[nfds].fd = ctx->x11_lib.XConnectionNumber(ctx->display);
  pfds[nfds].events = POLLIN;
  pfds[nfds].revents = 0;
  nfds++;

  // Controller FDs
  uint32_t ctrl_count = _maru_linux_common_fill_pollfds(&ctx->linux_common, &pfds[nfds], 32 - nfds);
  uint32_t ctrl_start_idx = nfds;
  nfds += ctrl_count;

  int poll_timeout = (timeout_ms == MARU_NEVER) ? -1 : (int)timeout_ms;
  int ret = poll(pfds, nfds, poll_timeout);

  if (ret > 0) {
    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      read(ctx->linux_common.worker.event_fd, &val, sizeof(val));
    }

    if (pfds[1].revents & POLLIN) {
        // X11 events available, will be processed in next XPending loop or immediately after poll
    }

    if (ctrl_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common, &pfds[ctrl_start_idx], ctrl_count);
    }
  }

  // Drain again to catch anything that came in during poll
  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      // TODO: Process X11 event
  }

  _maru_linux_common_drain_internal_events(&ctx->linux_common);

  return MARU_SUCCESS;
}

void *maru_getContextNativeHandle_X11(MARU_Context *context) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  return ctx->display;
}

void *maru_getWindowNativeHandle_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  return (void *)(uintptr_t)win->handle;
}

MARU_Status maru_createCursor_X11(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  (void)context;
  (void)create_info;
  (void)out_cursor;
  return MARU_FAILURE;
}

MARU_Status maru_destroyCursor_X11(MARU_Cursor *cursor) {
  (void)cursor;
  return MARU_FAILURE;
}

MARU_Status maru_createImage_X11(MARU_Context *context,
                                        const MARU_ImageCreateInfo *create_info,
                                        MARU_Image **out_image) {
  (void)context;
  (void)create_info;
  (void)out_image;
  return MARU_FAILURE;
}

MARU_Status maru_destroyImage_X11(MARU_Image *image) {
  (void)image;
  return MARU_FAILURE;
}

MARU_Monitor *const *maru_getMonitors_X11(MARU_Context *context, uint32_t *out_count) {
  (void)context;
  *out_count = 0;
  return NULL;
}

void maru_retainMonitor_X11(MARU_Monitor *monitor) {
  (void)monitor;
}

void maru_releaseMonitor_X11(MARU_Monitor *monitor) {
  (void)monitor;
}

const MARU_VideoMode *maru_getMonitorModes_X11(const MARU_Monitor *monitor, uint32_t *out_count) {
  (void)monitor;
  *out_count = 0;
  return NULL;
}

MARU_Status maru_setMonitorMode_X11(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  (void)monitor;
  (void)mode;
  return MARU_FAILURE;
}

void maru_resetMonitorMetrics_X11(MARU_Monitor *monitor) {
  (void)monitor;
}
