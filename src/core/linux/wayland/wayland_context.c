#define _GNU_SOURCE
#include "maru/c/instrumentation.h"
#include <dlfcn.h>
#include "dlib/loader.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "wayland_internal.h"

#include <errno.h>
#include <linux/input-event-codes.h>
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

static bool _maru_wayland_init_context_mouse_channels(MARU_Context_WL *ctx) {
  static const struct {
    const char *name;
    uint32_t native_code;
  } channel_defs[] = {
      {"left", BTN_LEFT},       {"right", BTN_RIGHT},     {"middle", BTN_MIDDLE},
      {"side", BTN_SIDE},       {"extra", BTN_EXTRA},     {"forward", BTN_FORWARD},
      {"back", BTN_BACK},       {"task", BTN_TASK},
  };
  const uint32_t channel_count = (uint32_t)(sizeof(channel_defs) / sizeof(channel_defs[0]));

  ctx->base.mouse_button_channels =
      (MARU_MouseButtonChannelInfo *)maru_context_alloc(&ctx->base,
                                                        sizeof(MARU_MouseButtonChannelInfo) * channel_count);
  if (!ctx->base.mouse_button_channels) {
    return false;
  }

  ctx->base.mouse_button_states =
      (MARU_ButtonState8 *)maru_context_alloc(&ctx->base, sizeof(MARU_ButtonState8) * channel_count);
  if (!ctx->base.mouse_button_states) {
    maru_context_free(&ctx->base, ctx->base.mouse_button_channels);
    ctx->base.mouse_button_channels = NULL;
    return false;
  }

  memset(ctx->base.mouse_button_states, 0, sizeof(MARU_ButtonState8) * channel_count);
  for (uint32_t i = 0; i < channel_count; ++i) {
    ctx->base.mouse_button_channels[i].name = channel_defs[i].name;
    ctx->base.mouse_button_channels[i].native_code = channel_defs[i].native_code;
    ctx->base.mouse_button_channels[i].is_default = false;
  }

  ctx->base.pub.mouse_button_count = channel_count;
  ctx->base.pub.mouse_button_channels = ctx->base.mouse_button_channels;
  ctx->base.pub.mouse_button_state = ctx->base.mouse_button_states;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_LEFT] = 0;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_RIGHT] = 1;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_MIDDLE] = 2;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_BACK] = 6;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_FORWARD] = 5;

  ctx->base.mouse_button_channels[0].is_default = true;
  ctx->base.mouse_button_channels[1].is_default = true;
  ctx->base.mouse_button_channels[2].is_default = true;
  ctx->base.mouse_button_channels[5].is_default = true;
  ctx->base.mouse_button_channels[6].is_default = true;

  return true;
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
      (void)_maru_wayland_update_cursor_mode(window);
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
    ctx->cursor_animation.active = false;
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

  _maru_wayland_dataexchange_onSeatCapabilities(ctx, wl_seat, caps);

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

static void _maru_wayland_record_pump_duration_ns(MARU_Context_WL *ctx,
                                                  uint64_t duration_ns) {
  MARU_ContextMetrics *metrics = &ctx->base.metrics;
  metrics->pump_call_count_total++;
  if (metrics->pump_call_count_total == 1) {
    metrics->pump_duration_avg_ns = duration_ns;
    metrics->pump_duration_peak_ns = duration_ns;
    return;
  }

  if (duration_ns > metrics->pump_duration_peak_ns) {
    metrics->pump_duration_peak_ns = duration_ns;
  }

  if (duration_ns >= metrics->pump_duration_avg_ns) {
    metrics->pump_duration_avg_ns +=
        (duration_ns - metrics->pump_duration_avg_ns) /
        metrics->pump_call_count_total;
  } else {
    metrics->pump_duration_avg_ns -=
        (metrics->pump_duration_avg_ns - duration_ns) /
        metrics->pump_call_count_total;
  }
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

static bool _wl_proxy_matches_registry_name(MARU_Context_WL *ctx, void *proxy,
                                            uint32_t name) {
  if (!proxy) {
    return false;
  }
  return ctx->dlib.wl.proxy_get_id((struct wl_proxy *)proxy) == name;
}

static void _wl_clear_idle_notification_only(MARU_Context_WL *ctx) {
  if (ctx->wl.idle_notification) {
    maru_ext_idle_notification_v1_destroy(ctx, ctx->wl.idle_notification);
    ctx->wl.idle_notification = NULL;
  }
}

static void _wl_teardown_seat_state(MARU_Context_WL *ctx) {
  ctx->wl.seat = NULL;

  if (ctx->wl.cursor_shape_device) {
    maru_wp_cursor_shape_device_v1_destroy(ctx, ctx->wl.cursor_shape_device);
    ctx->wl.cursor_shape_device = NULL;
  }

  _wl_clear_locked_window_pointers(ctx);

  if (ctx->wl.pointer) {
    maru_wl_pointer_destroy(ctx, ctx->wl.pointer);
    ctx->wl.pointer = NULL;
  }
  if (ctx->wl.keyboard) {
    maru_wl_keyboard_destroy(ctx, ctx->wl.keyboard);
    ctx->wl.keyboard = NULL;
  }

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    if ((window->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0) {
      window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
      memset(window->base.keyboard_state, 0, sizeof(window->base.keyboard_state));
      _maru_wayland_dispatch_presentation_state(
          window, MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED, true);
    }

    if (window->base.mouse_button_states && window->base.pub.mouse_button_count > 0) {
      memset(window->base.mouse_button_states, 0,
             sizeof(MARU_ButtonState8) * window->base.pub.mouse_button_count);
    }

    if (window->ext.text_input) {
      maru_zwp_text_input_v3_destroy(ctx, window->ext.text_input);
      window->ext.text_input = NULL;
    }
    window->ime_preedit_active = false;
    _maru_wayland_clear_text_input_pending(window);
  }

  ctx->linux_common.pointer.focused_window = NULL;
  ctx->linux_common.pointer.x = 0.0;
  ctx->linux_common.pointer.y = 0.0;
  ctx->linux_common.pointer.enter_serial = 0;
  ctx->linux_common.xkb.focused_window = NULL;
  if (ctx->base.mouse_button_states && ctx->base.pub.mouse_button_count > 0) {
    memset(ctx->base.mouse_button_states, 0,
           sizeof(MARU_ButtonState8) * ctx->base.pub.mouse_button_count);
  }

  ctx->repeat.repeat_key = 0;
  ctx->repeat.next_repeat_ns = 0;
  ctx->repeat.interval_ns = 0;
  ctx->cursor_animation.active = false;
  _maru_wayland_dataexchange_onSeatRemoved(ctx);

  _wl_clear_idle_notification_only(ctx);
  _wl_update_cursor_shape_device(ctx);
}

static void _wl_handle_required_global_removed(MARU_Context_WL *ctx,
                                               const char *iface_name) {
  MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                         iface_name);
  _maru_wayland_mark_lost(ctx, "Required Wayland global removed at runtime");
}

static void _wl_handle_optional_global_removed(MARU_Context_WL *ctx,
                                               const char *iface_name) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;

    if (strcmp(iface_name, "zwp_text_input_manager_v3") == 0) {
      if (window->ext.text_input) {
        maru_zwp_text_input_v3_destroy(ctx, window->ext.text_input);
        window->ext.text_input = NULL;
      }
      window->ime_preedit_active = false;
      _maru_wayland_clear_text_input_pending(window);
    } else if (strcmp(iface_name, "zwp_pointer_constraints_v1") == 0) {
      if (window->ext.locked_pointer) {
        maru_zwp_locked_pointer_v1_destroy(ctx, window->ext.locked_pointer);
        window->ext.locked_pointer = NULL;
      }
    } else if (strcmp(iface_name, "zwp_relative_pointer_manager_v1") == 0) {
      if (window->ext.relative_pointer) {
        maru_zwp_relative_pointer_v1_destroy(ctx, window->ext.relative_pointer);
        window->ext.relative_pointer = NULL;
      }
    } else if (strcmp(iface_name, "wp_viewporter") == 0) {
      if (window->ext.viewport) {
        maru_wp_viewport_destroy(ctx, window->ext.viewport);
        window->ext.viewport = NULL;
      }
    } else if (strcmp(iface_name, "wp_fractional_scale_manager_v1") == 0) {
      if (window->ext.fractional_scale) {
        maru_wp_fractional_scale_v1_destroy(ctx, window->ext.fractional_scale);
        window->ext.fractional_scale = NULL;
      }
    } else if (strcmp(iface_name, "wp_content_type_manager_v1") == 0) {
      if (window->ext.content_type) {
        maru_wp_content_type_v1_destroy(ctx, window->ext.content_type);
        window->ext.content_type = NULL;
      }
    } else if (strcmp(iface_name, "zxdg_decoration_manager_v1") == 0) {
      if (window->xdg.decoration) {
        maru_zxdg_toplevel_decoration_v1_destroy(ctx, window->xdg.decoration);
        window->xdg.decoration = NULL;
      }
    } else if (strcmp(iface_name, "zwp_idle_inhibit_manager_v1") == 0) {
      if (window->ext.idle_inhibitor) {
        maru_zwp_idle_inhibitor_v1_destroy(ctx, window->ext.idle_inhibitor);
        window->ext.idle_inhibitor = NULL;
      }
    }
  }

  if (strcmp(iface_name, "wl_seat") == 0) {
    _wl_teardown_seat_state(ctx);
  } else if (strcmp(iface_name, "wp_cursor_shape_manager_v1") == 0) {
    if (ctx->wl.cursor_shape_device) {
      maru_wp_cursor_shape_device_v1_destroy(ctx, ctx->wl.cursor_shape_device);
      ctx->wl.cursor_shape_device = NULL;
    }
    _wl_update_cursor_shape_device(ctx);
  } else if (strcmp(iface_name, "ext_idle_notifier_v1") == 0) {
    _wl_clear_idle_notification_only(ctx);
  } else if (strcmp(iface_name, "xdg_activation_v1") == 0) {
    _maru_wayland_cancel_activation(ctx);
  } else if (strcmp(iface_name, "zxdg_output_manager_v1") == 0) {
    for (uint32_t i = 0; i < ctx->base.monitor_cache_count; ++i) {
      MARU_Monitor_WL *monitor = (MARU_Monitor_WL *)ctx->base.monitor_cache[i];
      if (monitor->xdg_output) {
        maru_zxdg_output_v1_destroy(ctx, monitor->xdg_output);
        monitor->xdg_output = NULL;
      }
    }
  } else if (strcmp(iface_name, "wl_data_device_manager") == 0) {
    _maru_wayland_dataexchange_onSeatRemoved(ctx);
  }

  MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                         iface_name);
}

static void _idle_notification_handle_idled(void *data, struct ext_idle_notification_v1 *notification) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  MARU_Event evt = {0};
  evt.idle.is_idle = true;
  evt.idle.timeout_ms = ctx->base.tuning.idle_timeout_ms;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_IDLE_STATE_CHANGED, NULL, &evt);
}

static void _idle_notification_handle_resumed(void *data, struct ext_idle_notification_v1 *notification) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  MARU_Event evt = {0};
  evt.idle.is_idle = false;
  evt.idle.timeout_ms = ctx->base.tuning.idle_timeout_ms;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_IDLE_STATE_CHANGED, NULL, &evt);
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

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)      \
  if (_wl_proxy_matches_registry_name(ctx, ctx->protocols.iface_name, name)) {    \
    maru_##iface_name##_destroy(ctx, ctx->protocols.iface_name);                  \
    ctx->protocols.iface_name = NULL;                                              \
    _wl_handle_required_global_removed(ctx, #iface_name);                          \
    return;                                                                        \
  }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener)      \
  if (_wl_proxy_matches_registry_name(ctx, ctx->protocols.opt.iface_name, name)) { \
    maru_##iface_name##_destroy(ctx, ctx->protocols.opt.iface_name);              \
    ctx->protocols.opt.iface_name = NULL;                                          \
    _wl_handle_optional_global_removed(ctx, #iface_name);                          \
    return;                                                                        \
  }
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY
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
#ifdef MARU_GATHER_METRICS
  _maru_update_mem_metrics_alloc(&ctx->base, sizeof(MARU_Context_WL));
#endif
  if (!_maru_wayland_init_context_mouse_channels(ctx)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

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
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (ctx->wake_fd < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to create wake eventfd");
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to initialize Linux common context");
    close(ctx->wake_fd);
    ctx->wake_fd = -1;
    _maru_cleanup_context_base(&ctx->base);
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
  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_linux_common_cleanup(&ctx->linux_common);
  _maru_cleanup_context_base(&ctx->base);
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

  _maru_wayland_cancel_activation(ctx);
  if (ctx->pump_pollfds.fds) {
    maru_context_free(&ctx->base, ctx->pump_pollfds.fds);
    ctx->pump_pollfds.fds = NULL;
    ctx->pump_pollfds.capacity = 0;
  }

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_linux_common_cleanup(&ctx->linux_common);
  _maru_cleanup_context_base(&ctx->base);
  _maru_wayland_dataexchange_destroy(ctx);

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
  if (ctx->wl.cursor_surface) {
    maru_wl_surface_destroy(ctx, ctx->wl.cursor_surface);
    ctx->wl.cursor_surface = NULL;
  }
  if (ctx->wl.cursor_theme) {
    maru_wl_cursor_theme_destroy(ctx, ctx->wl.cursor_theme);
    ctx->wl.cursor_theme = NULL;
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
  if (maru_isContextLost(context)) {
    return MARU_ERROR_CONTEXT_LOST;
  }

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
  if (maru_isContextLost(context)) {
    return MARU_ERROR_CONTEXT_LOST;
  }
  if (ctx->wake_fd < 0) {
    return MARU_FAILURE;
  }
  const uint64_t one = 1;
  const ssize_t written = write(ctx->wake_fd, &one, sizeof(one));
  if (written == (ssize_t)sizeof(one) || (written < 0 && errno == EAGAIN)) {
    return MARU_SUCCESS;
  }
  return MARU_FAILURE;
}

typedef struct MARU_WLPumpStepState {
  int display_fd;
  int libdecor_fd;
  bool have_libdecor_fd;
  bool separate_libdecor_fd;
  int libdecor_index;
  int transfer_fd_count;
  int transfer_pfds_index;
  int controller_fd_count;
  int controller_pfds_index;
  nfds_t nfds;
  struct pollfd *fds;
} MARU_WLPumpStepState;

static bool _maru_wayland_pump_prepare_pollfds(MARU_Context_WL *ctx,
                                               MARU_WLPumpStepState *step,
                                               MARU_Status *status) {
  // Keep Wayland/display first and wake-fd second so poll-consume logic can use fixed slots.
  step->display_fd = maru_wl_display_get_fd(ctx, ctx->wl.display);
  step->libdecor_fd = -1;
  step->have_libdecor_fd =
      (ctx->decor_mode == MARU_WAYLAND_DECORATION_MODE_CSD) &&
      (ctx->libdecor_context != NULL);
  if (step->have_libdecor_fd) {
    step->libdecor_fd = maru_libdecor_get_fd(ctx, ctx->libdecor_context);
    if (step->libdecor_fd < 0) {
      step->have_libdecor_fd = false;
    }
  }

  step->transfer_fd_count = 0;
  for (MARU_LinuxDataTransfer *it = ctx->data_transfers; it; it = it->next) {
    ++step->transfer_fd_count;
  }
  
  step->controller_fd_count = (int)ctx->linux_common.controller_count;

  step->nfds = 2;
  step->separate_libdecor_fd =
      step->have_libdecor_fd && (step->libdecor_fd != step->display_fd);
  step->libdecor_index = -1;
  if (step->separate_libdecor_fd) {
    step->nfds++;
  }
  step->nfds += (nfds_t)step->transfer_fd_count;
  step->nfds += (nfds_t)step->controller_fd_count;

  if ((uint32_t)step->nfds > ctx->pump_pollfds.capacity) {
    const size_t old_size =
        (size_t)ctx->pump_pollfds.capacity * sizeof(struct pollfd);
    const size_t new_size = (size_t)step->nfds * sizeof(struct pollfd);
    struct pollfd *new_fds = (struct pollfd *)maru_context_realloc(
        &ctx->base, ctx->pump_pollfds.fds, old_size, new_size);
    if (!new_fds) {
      *status = MARU_FAILURE;
      return false;
    }
    ctx->pump_pollfds.fds = new_fds;
    ctx->pump_pollfds.capacity = (uint32_t)step->nfds;
  }

  step->fds = ctx->pump_pollfds.fds;
  step->fds[0].fd = step->display_fd;
  step->fds[0].events = POLLIN;
  step->fds[0].revents = 0;
  step->fds[1].fd = ctx->wake_fd;
  step->fds[1].events = POLLIN;
  step->fds[1].revents = 0;

  nfds_t write_index = 2;
  if (step->separate_libdecor_fd) {
    step->libdecor_index = (int)write_index;
    step->fds[write_index].fd = step->libdecor_fd;
    step->fds[write_index].events = POLLIN;
    step->fds[write_index].revents = 0;
    write_index++;
  }

  step->transfer_pfds_index = (int)write_index;
  if (step->transfer_fd_count > 0) {
    write_index += (nfds_t)maru_linux_dataexchange_fillPollFds(
        ctx->data_transfers, &step->fds[write_index], step->transfer_fd_count);
  }

  step->controller_pfds_index = (int)write_index;
  if (step->controller_fd_count > 0) {
    write_index += _maru_linux_common_fill_pollfds(&ctx->linux_common, &step->fds[write_index], (uint32_t)step->controller_fd_count);
  }
  (void)write_index;

  return true;
}

static bool _maru_wayland_pump_prepare_wayland_read(MARU_Context_WL *ctx,
                                                    MARU_Status *status) {
  // Wayland read protocol invariant: successful prepare_read must be followed by
  // read_events() or cancel_read() in the same pump iteration.
  while (maru_wl_display_prepare_read(ctx, ctx->wl.display) != 0) {
    if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
      _maru_wayland_mark_lost(
          ctx, "wl_display_dispatch_pending() failure during prepare_read");
      *status = MARU_ERROR_CONTEXT_LOST;
      return false;
    }
  }

  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    _maru_wayland_mark_lost(ctx, "wl_display_flush() failure");
    *status = MARU_ERROR_CONTEXT_LOST;
    return false;
  }

  return true;
}

static int _maru_wayland_pump_compute_timeout_ms(MARU_Context_WL *ctx,
                                                 uint32_t timeout_ms) {
  // Poll timeout is clamped by synthetic deadlines (key-repeat and cursor animation).
  int timeout = (timeout_ms == MARU_NEVER) ? -1 : (int)timeout_ms;
  if (ctx->repeat.repeat_key != 0 && ctx->repeat.rate > 0 &&
      ctx->repeat.next_repeat_ns != 0) {
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
  {
    const uint64_t cursor_next_ns = _maru_wayland_cursor_next_frame_ns(ctx);
    if (cursor_next_ns != 0) {
      const uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
      if (now_ns != 0) {
        if (cursor_next_ns <= now_ns) {
          timeout = 0;
        } else {
          const uint64_t wait_ns = cursor_next_ns - now_ns;
          const int cursor_timeout_ms = (int)((wait_ns + 999999ull) / 1000000ull);
          if (timeout < 0 || cursor_timeout_ms < timeout) {
            timeout = cursor_timeout_ms;
          }
        }
      }
    }
  }
  return timeout;
}

static bool _maru_wayland_pump_poll_and_consume(MARU_Context_WL *ctx,
                                                const MARU_WLPumpStepState *step,
                                                int timeout_ms,
                                                MARU_Status *status) {
  int poll_result = poll(step->fds, step->nfds, timeout_ms);
  if (poll_result > 0) {
    const short display_revents = step->fds[0].revents;
    if ((display_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      _maru_wayland_mark_lost(
          ctx, "Wayland display fd reported POLLERR/POLLHUP/POLLNVAL");
      *status = MARU_ERROR_CONTEXT_LOST;
      return false;
    }

    bool display_ready = (display_revents & POLLIN) != 0;
    if (display_ready) {
      if (maru_wl_display_read_events(ctx, ctx->wl.display) < 0) {
        _maru_wayland_mark_lost(ctx, "wl_display_read_events() failure");
        *status = MARU_ERROR_CONTEXT_LOST;
        return false;
      }
    } else {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
    }

    if ((step->fds[1].revents & POLLIN) != 0) {
      _maru_wayland_drain_wake_fd(ctx);
    }

    if (step->libdecor_index >= 0) {
      const short libdecor_revents = step->fds[step->libdecor_index].revents;
      if ((libdecor_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        _maru_wayland_mark_lost(ctx,
                                "libdecor fd reported POLLERR/POLLHUP/POLLNVAL");
        *status = MARU_ERROR_CONTEXT_LOST;
        return false;
      }
    }
    if (step->transfer_fd_count > 0) {
      maru_linux_dataexchange_processTransfers(
          &ctx->base, &ctx->data_transfers, step->fds, step->transfer_pfds_index,
          step->transfer_fd_count);
    }
    if (step->controller_fd_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common, &step->fds[step->controller_pfds_index], (uint32_t)step->controller_fd_count);
    }
  } else if (poll_result == 0) {
    maru_wl_display_cancel_read(ctx, ctx->wl.display);
  } else {
    maru_wl_display_cancel_read(ctx, ctx->wl.display);
    if (errno != EINTR) {
      _maru_wayland_mark_lost(ctx, "poll() failure on Wayland display fd");
      *status = MARU_ERROR_CONTEXT_LOST;
      return false;
    }
  }

  return true;
}

static bool _maru_wayland_pump_dispatch_and_validate(
    MARU_Context_WL *ctx, const MARU_WLPumpStepState *step, MARU_Status *status) {
  // Dispatch protocol events after poll/read, then verify terminal connection errors.
  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
    _maru_wayland_mark_lost(ctx, "wl_display_dispatch_pending() failure");
    *status = MARU_ERROR_CONTEXT_LOST;
    return false;
  }

  if (step->have_libdecor_fd) {
    if (maru_libdecor_dispatch(ctx, ctx->libdecor_context, 0) < 0) {
      _maru_wayland_mark_lost(ctx, "libdecor_dispatch() failure");
      *status = MARU_ERROR_CONTEXT_LOST;
      return false;
    }
  }

  if (maru_wl_display_get_error(ctx, ctx->wl.display) != 0) {
    _maru_wayland_mark_lost(
        ctx,
        "Wayland display reported a fatal protocol/connection error");
    *status = MARU_ERROR_CONTEXT_LOST;
    return false;
  }

  return true;
}

static void _maru_wayland_pump_dispatch_repeats(MARU_Context_WL *ctx) {
  if (ctx->repeat.repeat_key != 0 && ctx->repeat.rate > 0 &&
      ctx->repeat.next_repeat_ns != 0 && ctx->linux_common.xkb.state &&
      ctx->linux_common.xkb.focused_window) {
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
          _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT,
                               (MARU_Window *)window, &text_evt);
        }

        ctx->repeat.next_repeat_ns += interval_ns;
        dispatch_count++;
      }

      if (dispatch_count == max_dispatch_per_pump &&
          now_ns >= ctx->repeat.next_repeat_ns) {
        ctx->repeat.next_repeat_ns = now_ns + interval_ns;
      }
    }
  }
}

static void _maru_wayland_pump_dispatch_deferred_resizes(MARU_Context_WL *ctx) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_WL *window = (MARU_Window_WL *)it;
    if (window && window->pending_resized_event &&
        maru_isWindowReady((MARU_Window *)window)) {
      _maru_wayland_dispatch_window_resized(window);
    }
  }
}

static void _maru_wayland_pump_post_tick(MARU_Context_WL *ctx) {
  _maru_wayland_check_activation(ctx);
  _maru_wayland_pump_dispatch_repeats(ctx);
  _maru_wayland_advance_cursor_animation(ctx);
  _maru_wayland_pump_dispatch_deferred_resizes(ctx);
}

MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms,
                               MARU_EventCallback callback, void *userdata) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  if (!callback) {
    return MARU_FAILURE;
  }
  if (maru_isContextLost(context)) {
    return MARU_ERROR_CONTEXT_LOST;
  }
  const uint64_t pump_start_ns = _maru_wayland_get_monotonic_time_ns();
  MARU_Status status = MARU_SUCCESS;

  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_drain_queued_events(&ctx->base);
  MARU_WLPumpStepState step = {0};
  if (!_maru_wayland_pump_prepare_pollfds(ctx, &step, &status)) {
    goto pump_exit;
  }
  if (!_maru_wayland_pump_prepare_wayland_read(ctx, &status)) {
    goto pump_exit;
  }
  const int timeout = _maru_wayland_pump_compute_timeout_ms(ctx, timeout_ms);
  if (!_maru_wayland_pump_poll_and_consume(ctx, &step, timeout, &status)) {
    goto pump_exit;
  }
  if (!_maru_wayland_pump_dispatch_and_validate(ctx, &step, &status)) {
    goto pump_exit;
  }
  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_wayland_pump_post_tick(ctx);

pump_exit:;
  const uint64_t pump_end_ns = _maru_wayland_get_monotonic_time_ns();
  if (pump_start_ns != 0 && pump_end_ns != 0 && pump_end_ns >= pump_start_ns) {
    _maru_wayland_record_pump_duration_ns(ctx, pump_end_ns - pump_start_ns);
  }
  if (status == MARU_SUCCESS && maru_isContextLost(context)) {
    status = MARU_ERROR_CONTEXT_LOST;
  }
  ctx->base.pump_ctx = NULL;
  return status;
}

void *maru_getContextNativeHandle_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return ctx->wl.display;
}
