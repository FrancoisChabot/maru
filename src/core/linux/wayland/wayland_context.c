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

static void _wl_update_cursor_shape_device(MARU_Context_WL *ctx) {
  if (ctx->wl.cursor_shape_device &&
      (!ctx->wl.pointer || !ctx->protocols.opt.wp_cursor_shape_manager_v1)) {
    maru_wp_cursor_shape_device_v1_destroy(ctx, ctx->wl.cursor_shape_device);
    ctx->wl.cursor_shape_device = NULL;
  }

  if (!ctx->wl.cursor_shape_device && ctx->wl.pointer &&
      ctx->protocols.opt.wp_cursor_shape_manager_v1) {
    ctx->wl.cursor_shape_device = maru_wp_cursor_shape_manager_v1_get_pointer(
        ctx, ctx->protocols.opt.wp_cursor_shape_manager_v1, ctx->wl.pointer);
  }
}

static void _wl_refresh_locked_window_pointers(MARU_Context_WL *ctx) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    if (window->base.pub.cursor_mode == MARU_CURSOR_LOCKED) {
      _maru_wayland_update_cursor_mode(window);
    }
  }
}

static void _wl_clear_locked_window_pointers(MARU_Context_WL *ctx) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    if (window->ext.relative_pointer) {
      maru_zwp_relative_pointer_v1_destroy(ctx, window->ext.relative_pointer);
      window->ext.relative_pointer = NULL;
    }
    if (window->ext.locked_pointer) {
      maru_zwp_locked_pointer_v1_destroy(ctx, window->ext.locked_pointer);
      window->ext.locked_pointer = NULL;
    }
  }
}

static void _wl_seat_handle_capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  ctx->wl.seat = wl_seat;

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !ctx->wl.pointer) {
    ctx->wl.pointer = maru_wl_seat_get_pointer(ctx, wl_seat);
    ctx->dlib.wl.proxy_add_listener((struct wl_proxy *)ctx->wl.pointer,
                                    (void (**)(void))&_maru_wayland_pointer_listener, ctx);
    _wl_update_cursor_shape_device(ctx);
    _wl_refresh_locked_window_pointers(ctx);
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && ctx->wl.pointer) {
    _wl_clear_locked_window_pointers(ctx);
    _wl_update_cursor_shape_device(ctx);
    maru_wl_pointer_destroy(ctx, ctx->wl.pointer);
    ctx->wl.pointer = NULL;
    _wl_update_cursor_shape_device(ctx);
  }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !ctx->wl.keyboard) {
    ctx->wl.keyboard = maru_wl_seat_get_keyboard(ctx, wl_seat);
    ctx->dlib.wl.proxy_add_listener((struct wl_proxy *)ctx->wl.keyboard,
                                    (void (**)(void))&_maru_wayland_keyboard_listener, ctx);
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && ctx->wl.keyboard) {
    maru_wl_keyboard_destroy(ctx, ctx->wl.keyboard);
    ctx->wl.keyboard = NULL;
    ctx->repeat.repeat_key = 0;
    ctx->repeat.next_repeat_ns = 0;
    ctx->repeat.interval_ns = 0;
  }

  // Seat capabilities often arrive after windows are created; refresh text-input objects/state.
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    _maru_wayland_update_text_input(window);
  }
}

static void _wl_seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name) {
  (void)data; (void)wl_seat; (void)name;
}

const struct wl_seat_listener _maru_wayland_seat_listener = {
  .capabilities = _wl_seat_handle_capabilities,
  .name = _wl_seat_handle_name,
};

static void _maru_wayland_drain_wake_fd(MARU_Context_WL *ctx) {
  uint64_t pending = 0;
  while (read(ctx->wake_fd, &pending, sizeof(pending)) == (ssize_t)sizeof(pending)) {
  }
}

uint64_t _maru_wayland_get_monotonic_time_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static void _maru_wayland_disconnect_display(MARU_Context_WL *ctx) {
  if (ctx->wl.display) {
    maru_wl_display_disconnect(ctx, ctx->wl.display);
    ctx->wl.display = NULL;
  }
}

static void _maru_wayland_mark_lost(MARU_Context_WL *ctx, const char *message) {
  ctx->base.pub.flags |= MARU_CONTEXT_STATE_LOST;
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    it->pub.flags |= MARU_WINDOW_STATE_LOST;
  }
  if (message) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE, message);
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

static void _idle_notification_handle_idled(void *data, struct ext_idle_notification_v1 *notification) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  MARU_Event evt = {0};
  evt.idle.is_idle = true;
  evt.idle.timeout_ms = ctx->base.tuning.idle_timeout_ms;
  _maru_dispatch_event(&ctx->base, MARU_IDLE_STATE_CHANGED, NULL, &evt);
}

static void _idle_notification_handle_resumed(void *data, struct ext_idle_notification_v1 *notification) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  MARU_Event evt = {0};
  evt.idle.is_idle = false;
  evt.idle.timeout_ms = ctx->base.tuning.idle_timeout_ms;
  _maru_dispatch_event(&ctx->base, MARU_IDLE_STATE_CHANGED, NULL, &evt);
}

static const struct ext_idle_notification_v1_listener _idle_notification_listener = {
  .idled = _idle_notification_handle_idled,
  .resumed = _idle_notification_handle_resumed,
};

static void _maru_wayland_update_idle_notification(MARU_Context_WL *ctx) {
  if (ctx->wl.idle_notification) {
    maru_ext_idle_notification_v1_destroy(ctx, ctx->wl.idle_notification);
    ctx->wl.idle_notification = NULL;
  }

  if (ctx->wl.cursor_surface) {
    maru_wl_surface_destroy(ctx, ctx->wl.cursor_surface);
    ctx->wl.cursor_surface = NULL;
  }
  if (ctx->wl.cursor_theme) {
    maru_wl_cursor_theme_destroy(ctx, ctx->wl.cursor_theme);
    ctx->wl.cursor_theme = NULL;
  }

  uint32_t timeout_ms = ctx->base.tuning.idle_timeout_ms;
  if (timeout_ms > 0 && ctx->protocols.opt.ext_idle_notifier_v1 && ctx->protocols.opt.wl_seat) {
    ctx->wl.idle_notification = maru_ext_idle_notifier_v1_get_idle_notification(
        ctx, ctx->protocols.opt.ext_idle_notifier_v1, timeout_ms, ctx->protocols.opt.wl_seat);
    if (ctx->wl.idle_notification) {
      maru_ext_idle_notification_v1_add_listener(ctx, ctx->wl.idle_notification,
                                                  &_idle_notification_listener, ctx);
    }
  }
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

  if (strcmp(interface, "wl_seat") == 0) {
    ctx->wl.seat = ctx->protocols.opt.wl_seat;
    _maru_wayland_update_idle_notification(ctx);
  }
}

static void _registry_handle_global_remove(void *data,
                                           struct wl_registry *registry,
                                           uint32_t name) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)registry;
  _maru_wayland_remove_output(ctx, name);
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
  ctx->wake_fd = -1;

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

  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = MARU_CONTEXT_ATTR_ALL;
  ctx->base.diagnostic_cb = ctx->base.attrs_effective.diagnostic_cb;
  ctx->base.diagnostic_userdata = ctx->base.attrs_effective.diagnostic_userdata;
  ctx->base.event_mask = ctx->base.attrs_effective.event_mask;
  ctx->base.inhibit_idle = ctx->base.attrs_effective.inhibit_idle;
  ctx->base.tuning.idle_timeout_ms = ctx->base.attrs_effective.idle_timeout_ms;

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

  ctx->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (ctx->wake_fd < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to create wake eventfd");
    goto cleanup_symbols;
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

  maru_wl_registry_add_listener(ctx, ctx->wl.registry, &_registry_listener,
                                ctx);

  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "wl_display_roundtrip() failure");
    goto cleanup_registry;
  }

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

  _maru_wayland_update_idle_notification(ctx);
  _wl_update_cursor_shape_device(ctx);
  ctx->base.attrs_dirty_mask = 0;

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;

  return MARU_SUCCESS;

cleanup_registry:
  maru_wl_registry_destroy(ctx, ctx->wl.registry);
cleanup_display:
  _maru_wayland_disconnect_display(ctx);
cleanup_symbols:
  if (ctx->wake_fd >= 0) {
    close(ctx->wake_fd);
    ctx->wake_fd = -1;
  }
  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc,
                              &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);
  maru_context_free(&ctx->base, ctx);
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  while (ctx->base.window_list_head) {
    maru_destroyWindow_WL((MARU_Window *)ctx->base.window_list_head);
  }

  _maru_cleanup_context_base(&ctx->base);

  if (ctx->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) {
    _maru_wayland_cleanup_libdecor(ctx);
  }

  if (ctx->wl.pointer) {
    if (ctx->wl.cursor_shape_device) {
      maru_wp_cursor_shape_device_v1_destroy(ctx, ctx->wl.cursor_shape_device);
      ctx->wl.cursor_shape_device = NULL;
    }
    if (ctx->wl.pointer) maru_wl_pointer_destroy(ctx, ctx->wl.pointer);
    ctx->wl.pointer = NULL;
  }
  if (ctx->wl.keyboard) {
    if (ctx->wl.keyboard) maru_wl_keyboard_destroy(ctx, ctx->wl.keyboard);
    ctx->wl.keyboard = NULL;
  }

  if (ctx->wl.idle_notification) {
    maru_ext_idle_notification_v1_destroy(ctx, ctx->wl.idle_notification);
    ctx->wl.idle_notification = NULL;
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


  close(ctx->wake_fd);
  ctx->wake_fd = -1;

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_updateContext_WL(MARU_Context *context, uint64_t field_mask,
                                  const MARU_ContextAttributes *attributes) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
      MARU_Window_WL *window = (MARU_Window_WL *)it;
      _maru_wayland_update_idle_inhibitor(window);
    }
  }

  _maru_wayland_update_idle_notification(ctx);

  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  const uint64_t one = 1;
  const ssize_t written = write(ctx->wake_fd, &one, sizeof(one));
  if (written == (ssize_t)sizeof(one) || (written < 0 && errno == EAGAIN)) {
    return MARU_SUCCESS;
  }
  return MARU_FAILURE;
}

MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms,
                               MARU_EventCallback callback, void *userdata) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  MARU_Status status = MARU_SUCCESS;

  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  _maru_drain_user_events(&ctx->base);

  int display_fd = maru_wl_display_get_fd(ctx, ctx->wl.display);
  int libdecor_fd = -1;
  bool have_libdecor_fd =
      (ctx->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) &&
      (ctx->libdecor_context != NULL);
  if (have_libdecor_fd) {
    libdecor_fd = maru_libdecor_get_fd(ctx, ctx->libdecor_context);
    if (libdecor_fd < 0) {
      have_libdecor_fd = false;
    }
  }

  struct pollfd fds[3];
  nfds_t nfds = 2;
  fds[0].fd = display_fd;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  fds[1].fd = ctx->wake_fd;
  fds[1].events = POLLIN;
  fds[1].revents = 0;
  int libdecor_index = -1;
  if (have_libdecor_fd) {
    libdecor_index = (int)nfds;
    fds[nfds].fd = libdecor_fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    nfds++;
  }

  while (maru_wl_display_prepare_read(ctx, ctx->wl.display) != 0) {
    if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
      _maru_wayland_mark_lost(ctx, "wl_display_dispatch_pending() failure during prepare_read");
      status = MARU_FAILURE;
      goto pump_exit;
    }
  }

  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    _maru_wayland_mark_lost(ctx, "wl_display_flush() failure");
    status = MARU_FAILURE;
    goto pump_exit;
  }

  int timeout = (timeout_ms == MARU_NEVER) ? -1 : (int)timeout_ms;
  if (ctx->repeat.repeat_key != 0 && ctx->repeat.rate > 0 && ctx->repeat.next_repeat_ns != 0) {
    const uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
    if (now_ns != 0) {
      if (ctx->repeat.next_repeat_ns <= now_ns) {
        timeout = 0;
      } else {
        const uint64_t wait_ns = ctx->repeat.next_repeat_ns - now_ns;
        const int repeat_timeout_ms = (int)((wait_ns + 999999ull) / 1000000ull);
        if (timeout < 0 || repeat_timeout_ms < timeout) {
          timeout = repeat_timeout_ms;
        }
      }
    }
  }

  int poll_result = poll(fds, nfds, timeout);
  if (poll_result > 0) {
    const short display_revents = fds[0].revents;
    if ((display_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      _maru_wayland_mark_lost(ctx, "Wayland display fd reported POLLERR/POLLHUP/POLLNVAL");
      status = MARU_FAILURE;
      goto pump_exit;
    }

    bool display_ready = (display_revents & POLLIN) != 0;
    if (display_ready) {
      if (maru_wl_display_read_events(ctx, ctx->wl.display) < 0) {
        _maru_wayland_mark_lost(ctx, "wl_display_read_events() failure");
        status = MARU_FAILURE;
        goto pump_exit;
      }
    } else {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
    }

    if ((fds[1].revents & POLLIN) != 0) {
      _maru_wayland_drain_wake_fd(ctx);
    }

    if (libdecor_index >= 0) {
      const short libdecor_revents = fds[libdecor_index].revents;
      if ((libdecor_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        _maru_wayland_mark_lost(
            ctx, "libdecor fd reported POLLERR/POLLHUP/POLLNVAL");
        status = MARU_FAILURE;
        goto pump_exit;
      }

      if ((libdecor_revents & POLLIN) != 0) {
        if (maru_libdecor_dispatch(ctx, ctx->libdecor_context, 0) < 0) {
          _maru_wayland_mark_lost(ctx, "libdecor_dispatch() failure");
          status = MARU_FAILURE;
          goto pump_exit;
        }
      }
    }
  } else if (poll_result == 0) {
    maru_wl_display_cancel_read(ctx, ctx->wl.display);
  } else {
    maru_wl_display_cancel_read(ctx, ctx->wl.display);
    if (errno != EINTR) {
      _maru_wayland_mark_lost(ctx, "poll() failure on Wayland display fd");
      status = MARU_FAILURE;
      goto pump_exit;
    }
  }

  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
    _maru_wayland_mark_lost(ctx, "wl_display_dispatch_pending() failure");
    status = MARU_FAILURE;
    goto pump_exit;
  }

  if (maru_wl_display_get_error(ctx, ctx->wl.display) != 0) {
    _maru_wayland_mark_lost(ctx, "Wayland display reported a fatal protocol/connection error");
    status = MARU_FAILURE;
    goto pump_exit;
  }

  if (ctx->repeat.repeat_key != 0 && ctx->repeat.rate > 0 && ctx->repeat.next_repeat_ns != 0 &&
      ctx->linux_common.xkb.state && ctx->linux_common.xkb.focused_window) {
    uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
    if (now_ns != 0 && now_ns >= ctx->repeat.next_repeat_ns) {
      uint32_t dispatch_count = 0;
      const uint32_t max_dispatch_per_pump = 16;
      const uint64_t interval_ns =
          (ctx->repeat.interval_ns > 0) ? ctx->repeat.interval_ns : 1000000000ull;

      while (ctx->repeat.repeat_key != 0 && now_ns >= ctx->repeat.next_repeat_ns &&
             dispatch_count < max_dispatch_per_pump) {
        MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.xkb.focused_window;
        if (!window) {
          break;
        }
        const bool text_input_active =
            (window->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE) &&
            (window->ext.text_input != NULL) &&
            (ctx->protocols.opt.zwp_text_input_manager_v3 != NULL) &&
            window->ime_preedit_active;
        if (text_input_active) {
          ctx->repeat.repeat_key = 0;
          ctx->repeat.next_repeat_ns = 0;
          ctx->repeat.interval_ns = 0;
          break;
        }

        char buf[32];
        int n = maru_xkb_state_key_get_utf8(ctx, ctx->linux_common.xkb.state,
                                            ctx->repeat.repeat_key, buf, sizeof(buf));
        if (n > 0) {
          MARU_Event text_evt = {0};
          text_evt.text_edit_commit.committed_utf8 = buf;
          text_evt.text_edit_commit.committed_length = (uint32_t)n;
          _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_COMMIT, (MARU_Window *)window, &text_evt);
        }

        ctx->repeat.next_repeat_ns += interval_ns;
        dispatch_count++;
      }

      if (dispatch_count == max_dispatch_per_pump && now_ns >= ctx->repeat.next_repeat_ns) {
        ctx->repeat.next_repeat_ns = now_ns + interval_ns;
      }
    }
  }

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    if (window && window->pending_resized_event && maru_isWindowReady((MARU_Window *)window)) {
      _maru_wayland_dispatch_window_resized(window);
    }
  }

pump_exit:
  ctx->base.pump_ctx = NULL;
  return status;
}

void *maru_getContextNativeHandle_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return ctx->wl.display;
}
