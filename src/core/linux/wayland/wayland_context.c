#include "maru/c/instrumentation.h"
#define _GNU_SOURCE
#include "dlib/loader.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "wayland_internal.h"

#include <errno.h>
#include <linux/memfd.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>


#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1
#endif

static void _xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  maru_xdg_wm_base_pong(ctx, xdg_wm_base, serial);
}

const struct xdg_wm_base_listener _maru_xdg_wm_base_listener = {
  .ping = _xdg_wm_base_ping,
};


static void _wl_seat_handle_capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
}

static void _wl_seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name) {
  (void)data; (void)wl_seat; (void)name;
}

const struct wl_seat_listener _maru_wayland_seat_listener = {
  .capabilities = _wl_seat_handle_capabilities,
  .name = _wl_seat_handle_name,
};


static void _maru_wayland_disconnect_display(MARU_Context_WL *ctx) {
  if (ctx->wl.display) {
    maru_wl_display_disconnect(ctx, ctx->wl.display);
    ctx->wl.display = NULL;
  }
}

static bool _maru_wayland_connect_display(MARU_Context_WL *ctx) {
  ctx->wl.display = maru_wl_display_connect(ctx, NULL);
  if (!ctx->wl.display) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to connect to Wayland display");
    return false;
  }
  return true;
}

static bool _maru_wl_registry_try_bind(
    MARU_Context_WL *ctx, struct wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version, const char *target_name,
    const struct wl_interface *target_interface, uint32_t target_version,
    void **storage, const void *listener, void *data) {
  if (strcmp(interface, target_name) != 0)
    return false;
  if (*storage)
    return true;

  uint32_t bind_version = (version < target_version) ? version : target_version;
  void *proxy = maru_wl_registry_bind(ctx, registry, name, target_interface,
                                      bind_version);
  if (!proxy)
    return true;

  if (listener) {
    ctx->dlib.wl.proxy_add_listener((struct wl_proxy *)proxy,
                                    (void (**)(void))listener, data);
  }

  *storage = proxy;
  return true;
}

static void _registry_handle_global(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;

  if (strcmp(interface, "wl_output") == 0) {
    _maru_wayland_bind_output(ctx, name, version);
    return;
  }

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)    \
  if (_maru_wl_registry_try_bind(                                              \
          ctx, registry, name, interface, version, #iface_name,                \
          &iface_name##_interface, iface_version,                              \
          (void **)&ctx->protocols.iface_name, listener, ctx))                 \
    return;
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)    \
  if (_maru_wl_registry_try_bind(                                              \
          ctx, registry, name, interface, version, #iface_name,                \
          &iface_name##_interface, iface_version,                              \
          (void **)&ctx->protocols.opt.iface_name, listener, ctx))             \
    return;
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY
}

static void _registry_handle_global_remove(void *data,
                                           struct wl_registry *registry,
                                           uint32_t name) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)registry;
}

static const struct wl_registry_listener _registry_listener = {
    .global = _registry_handle_global,
    .global_remove = _registry_handle_global_remove,
};

MARU_WaylandDecorationMode _maru_wayland_get_decoration_mode(MARU_Context_WL *ctx) {
  MARU_WaylandDecorationMode mode = ctx->base.tuning.wayland.decoration_mode;

  if (mode != MARU_WAYLAND_DECORATION_MODE_AUTO) {
    return mode;
  }

  if (ctx->protocols.opt.zxdg_decoration_manager_v1) {
    return MARU_WAYLAND_DECORATION_MODE_SSD;
  }

  if (ctx->dlib.opt.decor.base.available) {
    return MARU_WAYLAND_DECORATION_MODE_CSD;
  }

  return MARU_WAYLAND_DECORATION_MODE_NONE;
}

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_WL));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t *)ctx) + sizeof(MARU_Context_Base), 0,
         sizeof(MARU_Context_WL) - sizeof(MARU_Context_Base));

  ctx->base.pub.backend_type = MARU_BACKEND_WAYLAND;
  ctx->base.tuning = create_info->tuning;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }

  _maru_init_context_base(&ctx->base);

  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_mask = create_info->attributes.event_mask;

  MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_INFO,
                         "Beginning Wayland initialization...");

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  ctx->base.backend = &maru_backend_WL;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  if (!maru_load_wayland_symbols(&ctx->base, &ctx->dlib.wl, &ctx->dlib.wlc,
                                 &ctx->linux_common.xkb_lib,
                                 &ctx->dlib.opt.decor)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->linux_common.xkb.ctx = maru_xkb_context_new(ctx, XKB_CONTEXT_NO_FLAGS);
  if (!ctx->linux_common.xkb.ctx) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to create XKB context");
    goto cleanup_symbols;
  }

  if (!_maru_wayland_connect_display(ctx)) {
    goto cleanup_symbols;
  }

  ctx->wl.registry = maru_wl_display_get_registry(ctx, ctx->wl.display);
  if (!ctx->wl.registry) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to obtain Wayland registry");
    goto cleanup_display;
  }

  fprintf(stderr, "DEBUG: About to add registry listener\n");
  maru_wl_registry_add_listener(ctx, ctx->wl.registry, &_registry_listener,
                                ctx);

  fprintf(stderr, "DEBUG: About to do first roundtrip\n");
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "wl_display_roundtrip() failure");
    goto cleanup_registry;
  }

  fprintf(stderr, "DEBUG: First roundtrip done\n");

  // Check if required interfaces are bound
  bool missing_required = false;
#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)    \
  if (!ctx->protocols.iface_name) {                                            \
    missing_required = true;                                                   \
  }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

  if (missing_required) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                           "Required Wayland interfaces missing");
    goto cleanup_registry;
  }

  // Second roundtrip to process additional events
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "wl_display_roundtrip() failure (second pass)");
    goto cleanup_registry;
  }

  ctx->decor_mode = _maru_wayland_get_decoration_mode(ctx);

  if (ctx->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
    if (!_maru_wayland_init_libdecor(ctx)) {
      ctx->decor_mode = MARU_WAYLAND_DECORATION_MODE_NONE;
    }
  }

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;

cleanup_registry:
  maru_wl_registry_destroy(ctx, ctx->wl.registry);
cleanup_display:
  _maru_wayland_disconnect_display(ctx);
cleanup_symbols:
  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc,
                              &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);
  maru_context_free(&ctx->base, ctx);
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  if (ctx->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
    _maru_wayland_cleanup_libdecor(ctx);
  }

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)    \
  if (ctx->protocols.iface_name) {                                             \
    maru_##iface_name##_destroy(ctx, ctx->protocols.iface_name);               \
  }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)    \
  if (ctx->protocols.opt.iface_name) {                                         \
    maru_##iface_name##_destroy(ctx, ctx->protocols.opt.iface_name);           \
  }
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

  if (ctx->wl.registry) {
    maru_wl_registry_destroy(ctx, ctx->wl.registry);
  }

  _maru_wayland_disconnect_display(ctx);

  if (ctx->linux_common.xkb.state)
    maru_xkb_state_unref(ctx, ctx->linux_common.xkb.state);
  if (ctx->linux_common.xkb.keymap)
    maru_xkb_keymap_unref(ctx, ctx->linux_common.xkb.keymap);
  if (ctx->linux_common.xkb.ctx)
    maru_xkb_context_unref(ctx, ctx->linux_common.xkb.ctx);

  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc,
                              &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);

  _maru_cleanup_context_base(&ctx->base);

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_updateContext_WL(MARU_Context *context, uint64_t field_mask,
                                  const MARU_ContextAttributes *attributes) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  (void)attributes;
  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  
  return MARU_FAILURE;
}

MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms,
                               MARU_EventCallback callback, void *userdata) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  _maru_drain_user_events(&ctx->base);

  int display_fd = maru_wl_display_get_fd(ctx, ctx->wl.display);
  struct pollfd fds[] = {
      {display_fd, POLLIN, 0},
  };

  while (maru_wl_display_prepare_read(ctx, ctx->wl.display) != 0) {
    maru_wl_display_dispatch_pending(ctx, ctx->wl.display);
  }

  maru_wl_display_flush(ctx, ctx->wl.display);

  int timeout = (timeout_ms == MARU_NEVER) ? -1 : (int)timeout_ms;
  if (poll(fds, 1, timeout) > 0) {
    maru_wl_display_read_events(ctx, ctx->wl.display);
  } else {
    maru_wl_display_cancel_read(ctx, ctx->wl.display);
  }

  maru_wl_display_dispatch_pending(ctx, ctx->wl.display);

  ctx->base.pump_ctx = NULL;
  return MARU_SUCCESS;
}

void *maru_getContextNativeHandle_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return ctx->wl.display;
}
