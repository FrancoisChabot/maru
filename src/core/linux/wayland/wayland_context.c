#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "dlib/loader.h"

#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

static void _maru_wayland_disconnect_display(MARU_Context_WL *ctx) {
  if (ctx->wl.display) {
    maru_wl_display_disconnect(ctx, ctx->wl.display);
    ctx->wl.display = NULL;
  }
}

static bool _maru_wayland_connect_display(MARU_Context_WL *ctx) {
  ctx->wl.display = maru_wl_display_connect(ctx, NULL);
  if (!ctx->wl.display) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to connect to Wayland display");
    return false;
  }
  return true;
}

static void _xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  maru_xdg_wm_base_pong(ctx, xdg_wm_base, serial);
}

const struct xdg_wm_base_listener _maru_xdg_wm_base_listener = {
  .ping = _xdg_wm_base_ping,
};

static bool _maru_wl_registry_try_bind(MARU_Context_WL *ctx, struct wl_registry *registry,
                                  uint32_t name, const char *interface, uint32_t version,
                                  const char *target_name,
                                  const struct wl_interface *target_interface,
                                  uint32_t target_version, void **storage, const void *listener,
                                  void *data) {
  if (strcmp(interface, target_name) != 0) return false;
  if (*storage) return true;

  uint32_t bind_version = (version < target_version) ? version : target_version;
  void *proxy = maru_wl_registry_bind(ctx, registry, name, target_interface, bind_version);
  if (!proxy) return true;

  if (listener) {
     ctx->dlib.wl.proxy_add_listener((struct wl_proxy *)proxy, (void (**)(void))listener, data);
  }

  *storage = proxy;
  return true;
}

static void _registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
                                    const char *interface, uint32_t version) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (_maru_wl_registry_try_bind(ctx, registry, name, interface, version, #iface_name, \
                            &iface_name##_interface, iface_version, \
                            (void **)&ctx->protocols.iface_name, listener, ctx)) \
    return;
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (_maru_wl_registry_try_bind(ctx, registry, name, interface, version, #iface_name, \
                            &iface_name##_interface, iface_version, \
                            (void **)&ctx->protocols.opt.iface_name, listener, ctx)) \
    return;
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY
}

static void _registry_handle_global_remove(void *data, struct wl_registry *registry,
                                           uint32_t name) {
  (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener _registry_listener = {
    .global = _registry_handle_global,
    .global_remove = _registry_handle_global_remove,
};

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_WL));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t*)ctx) + sizeof(MARU_Context_Base), 0, sizeof(MARU_Context_WL) - sizeof(MARU_Context_Base));

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }

  ctx->base.pub.userdata = create_info->userdata;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_cb = create_info->attributes.event_callback;
  ctx->base.event_mask = create_info->attributes.event_mask;

  if (create_info->tuning) {
    ctx->base.tuning = *create_info->tuning;
  } else {
    MARU_ContextTuning default_tuning = MARU_CONTEXT_TUNING_DEFAULT;
    ctx->base.tuning = default_tuning;
  }

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  ctx->base.backend = &maru_backend_WL;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  if (!maru_load_wayland_symbols(&ctx->base, &ctx->dlib.wl, &ctx->dlib.wlc, &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->linux_common.xkb.ctx = maru_xkb_context_new(ctx, XKB_CONTEXT_NO_FLAGS);
  if (!ctx->linux_common.xkb.ctx) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to create XKB context");
    goto cleanup_symbols;
  }

  if (!_maru_wayland_connect_display(ctx)) {
    goto cleanup_xkb;
  }

  ctx->wl.registry = maru_wl_display_get_registry(ctx, ctx->wl.display);
  if (!ctx->wl.registry) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to obtain Wayland registry");
    goto cleanup_display;
  }

  maru_wl_registry_add_listener(ctx, ctx->wl.registry, &_registry_listener, ctx);
  
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "wl_display_roundtrip() failure");
    goto cleanup_registry;
  }

  // Check if required interfaces are bound
  bool missing_required = false;
#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (!ctx->protocols.iface_name) { missing_required = true; }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

  if (missing_required) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, "Required Wayland interfaces missing");
    goto cleanup_registry;
  }

  ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, NULL, 24, ctx->protocols.wl_shm);
  ctx->wl.cursor_surface = maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;

cleanup_registry:
  maru_wl_registry_destroy(ctx, ctx->wl.registry);
cleanup_display:
  _maru_wayland_disconnect_display(ctx);
cleanup_xkb:
  if (ctx->linux_common.xkb.ctx) maru_xkb_context_unref(ctx, ctx->linux_common.xkb.ctx);
cleanup_symbols:
  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc, &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);
  maru_context_free(&ctx->base, ctx);
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  if (ctx->wl.cursor_surface) {
    maru_wl_surface_destroy(ctx, ctx->wl.cursor_surface);
  }
  if (ctx->wl.cursor_theme) {
    maru_wl_cursor_theme_destroy(ctx, ctx->wl.cursor_theme);
  }

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (ctx->protocols.iface_name) { maru_##iface_name##_destroy(ctx, ctx->protocols.iface_name); }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (ctx->protocols.opt.iface_name) { maru_##iface_name##_destroy(ctx, ctx->protocols.opt.iface_name); }
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

  if (ctx->wl.registry) {
    maru_wl_registry_destroy(ctx, ctx->wl.registry);
  }

  _maru_wayland_disconnect_display(ctx);

  if (ctx->linux_common.xkb.ctx) maru_xkb_context_unref(ctx, ctx->linux_common.xkb.ctx);

  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc, &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  // 1. Dispatch any pending events
  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
    return MARU_FAILURE;
  }

  // 2. Prepare to read from the display socket
  while (maru_wl_display_prepare_read(ctx, ctx->wl.display) != 0) {
      if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
          return MARU_FAILURE;
      }
  }

  // 3. Flush any outgoing requests before blocking/polling
  // If flush fails with EAGAIN, we need to poll for writing as well.
  int flush_ret = maru_wl_display_flush(ctx, ctx->wl.display);
  if (flush_ret < 0 && errno != EAGAIN) {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      return MARU_FAILURE;
  }

  // 4. Poll
  struct pollfd pfd;
  pfd.fd = maru_wl_display_get_fd(ctx, ctx->wl.display);
  pfd.events = POLLIN;
  if (flush_ret < 0 && errno == EAGAIN) {
      pfd.events |= POLLOUT;
  }

  int ret = poll(&pfd, 1, (int)timeout_ms);

  if (ret > 0) {
      // If we have data to read
      if (pfd.revents & POLLIN) {
          if (maru_wl_display_read_events(ctx, ctx->wl.display) < 0) {
              return MARU_FAILURE;
          }
      } else {
          // Only writable or other event, cancel read since we didn't read
          maru_wl_display_cancel_read(ctx, ctx->wl.display);
      }
  } else {
      // Timeout or Error
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      if (ret < 0 && errno != EINTR) {
          return MARU_FAILURE;
      }
  }

  // 5. Dispatch any events we just read
  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
      return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}
