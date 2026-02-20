#define _GNU_SOURCE
#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "dlib/loader.h"

#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/memfd.h>
#include <unistd.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1
#endif

#define WL_SEAT_CAPABILITY_POINTER 1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_SEAT_CAPABILITY_TOUCH 4

#define WL_KEYBOARD_KEY_STATE_PRESSED 1
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 1
#define WL_POINTER_BUTTON_STATE_PRESSED 1
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1

extern const struct wl_keyboard_listener _maru_wayland_keyboard_listener;
extern const struct wl_pointer_listener _maru_wayland_pointer_listener;
extern const struct wl_seat_listener _maru_wayland_seat_listener;

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

static MARU_Key _maru_wl_keycode_to_maru(uint32_t wl_keycode) {
  static const MARU_Key evdev_to_maru[] = {
    [0x00] = MARU_KEY_UNKNOWN,
    [0x01] = MARU_KEY_ESCAPE,
    [0x02] = MARU_KEY_1,
    [0x03] = MARU_KEY_2,
    [0x04] = MARU_KEY_3,
    [0x05] = MARU_KEY_4,
    [0x06] = MARU_KEY_5,
    [0x07] = MARU_KEY_6,
    [0x08] = MARU_KEY_7,
    [0x09] = MARU_KEY_8,
    [0x0a] = MARU_KEY_9,
    [0x0b] = MARU_KEY_0,
    [0x0c] = MARU_KEY_MINUS,
    [0x0d] = MARU_KEY_EQUAL,
    [0x0e] = MARU_KEY_BACKSPACE,
    [0x0f] = MARU_KEY_TAB,
    [0x10] = MARU_KEY_Q,
    [0x11] = MARU_KEY_W,
    [0x12] = MARU_KEY_E,
    [0x13] = MARU_KEY_R,
    [0x14] = MARU_KEY_T,
    [0x15] = MARU_KEY_Y,
    [0x16] = MARU_KEY_U,
    [0x17] = MARU_KEY_I,
    [0x18] = MARU_KEY_O,
    [0x19] = MARU_KEY_P,
    [0x1a] = MARU_KEY_LEFT_BRACKET,
    [0x1b] = MARU_KEY_RIGHT_BRACKET,
    [0x1c] = MARU_KEY_ENTER,
    [0x1d] = MARU_KEY_LEFT_CONTROL,
    [0x1e] = MARU_KEY_A,
    [0x1f] = MARU_KEY_S,
    [0x20] = MARU_KEY_D,
    [0x21] = MARU_KEY_F,
    [0x22] = MARU_KEY_G,
    [0x23] = MARU_KEY_H,
    [0x24] = MARU_KEY_J,
    [0x25] = MARU_KEY_K,
    [0x26] = MARU_KEY_L,
    [0x27] = MARU_KEY_SEMICOLON,
    [0x28] = MARU_KEY_APOSTROPHE,
    [0x29] = MARU_KEY_GRAVE_ACCENT,
    [0x2a] = MARU_KEY_LEFT_SHIFT,
    [0x2b] = MARU_KEY_BACKSLASH,
    [0x2c] = MARU_KEY_Z,
    [0x2d] = MARU_KEY_X,
    [0x2e] = MARU_KEY_C,
    [0x2f] = MARU_KEY_V,
    [0x30] = MARU_KEY_B,
    [0x31] = MARU_KEY_N,
    [0x32] = MARU_KEY_M,
    [0x33] = MARU_KEY_COMMA,
    [0x34] = MARU_KEY_PERIOD,
    [0x35] = MARU_KEY_SLASH,
    [0x36] = MARU_KEY_RIGHT_SHIFT,
    [0x37] = MARU_KEY_KP_MULTIPLY,
    [0x38] = MARU_KEY_LEFT_ALT,
    [0x39] = MARU_KEY_SPACE,
    [0x3a] = MARU_KEY_CAPS_LOCK,
    [0x3b] = MARU_KEY_F1,
    [0x3c] = MARU_KEY_F2,
    [0x3d] = MARU_KEY_F3,
    [0x3e] = MARU_KEY_F4,
    [0x3f] = MARU_KEY_F5,
    [0x40] = MARU_KEY_F6,
    [0x41] = MARU_KEY_F7,
    [0x42] = MARU_KEY_F8,
    [0x43] = MARU_KEY_F9,
    [0x44] = MARU_KEY_F10,
    [0x45] = MARU_KEY_NUM_LOCK,
    [0x46] = MARU_KEY_SCROLL_LOCK,
    [0x47] = MARU_KEY_KP_7,
    [0x48] = MARU_KEY_KP_8,
    [0x49] = MARU_KEY_KP_9,
    [0x4a] = MARU_KEY_KP_SUBTRACT,
    [0x4b] = MARU_KEY_KP_4,
    [0x4c] = MARU_KEY_KP_5,
    [0x4d] = MARU_KEY_KP_6,
    [0x4e] = MARU_KEY_KP_ADD,
    [0x4f] = MARU_KEY_KP_1,
    [0x50] = MARU_KEY_KP_2,
    [0x51] = MARU_KEY_KP_3,
    [0x52] = MARU_KEY_KP_0,
    [0x53] = MARU_KEY_KP_DECIMAL,
    [0x54] = 0,
    [0x55] = 0,
    [0x56] = MARU_KEY_ESCAPE,
    [0x57] = MARU_KEY_F11,
    [0x58] = MARU_KEY_F12,
    [0x59] = MARU_KEY_KP_ENTER,
    [0x5a] = MARU_KEY_RIGHT_CONTROL,
    [0x5b] = MARU_KEY_KP_DIVIDE,
    [0x5c] = MARU_KEY_RIGHT_ALT,
    [0x5d] = MARU_KEY_HOME,
    [0x5e] = MARU_KEY_UP,
    [0x5f] = MARU_KEY_PAGE_UP,
    [0x60] = MARU_KEY_LEFT,
    [0x61] = MARU_KEY_RIGHT,
    [0x62] = MARU_KEY_END,
    [0x63] = MARU_KEY_DOWN,
    [0x64] = MARU_KEY_PAGE_DOWN,
    [0x65] = MARU_KEY_INSERT,
    [0x66] = MARU_KEY_DELETE,
    [0x67] = MARU_KEY_PAUSE,
  };
  uint32_t evdev_code = wl_keycode - 8;
  if (evdev_code >= (sizeof(evdev_to_maru) / sizeof(evdev_to_maru[0]))) {
    return MARU_KEY_UNKNOWN;
  }
  return evdev_to_maru[evdev_code];
}

static MARU_ModifierFlags _maru_wayland_get_modifiers(MARU_Context_WL *ctx) {
  MARU_ModifierFlags mods = 0;
  struct xkb_state *state = ctx->linux_common.xkb.state;
  if (!state) return 0;

  if (ctx->linux_common.xkb_lib.state_mod_name_is_active(state, "Shift", XKB_STATE_MODS_EFFECTIVE))
    mods |= MARU_MODIFIER_SHIFT;
  if (ctx->linux_common.xkb_lib.state_mod_name_is_active(state, "Control", XKB_STATE_MODS_EFFECTIVE))
    mods |= MARU_MODIFIER_CONTROL;
  if (ctx->linux_common.xkb_lib.state_mod_name_is_active(state, "Alt", XKB_STATE_MODS_EFFECTIVE))
    mods |= MARU_MODIFIER_ALT;
  if (ctx->linux_common.xkb_lib.state_mod_name_is_active(state, "Meta", XKB_STATE_MODS_EFFECTIVE))
    mods |= MARU_MODIFIER_META;

  return mods;
}

static const char *_maru_cursor_shape_to_name(MARU_CursorShape shape) {
  switch (shape) {
    case MARU_CURSOR_SHAPE_DEFAULT: return "left_ptr";
    case MARU_CURSOR_SHAPE_HELP: return "help";
    case MARU_CURSOR_SHAPE_HAND: return "hand2";
    case MARU_CURSOR_SHAPE_WAIT: return "watch";
    case MARU_CURSOR_SHAPE_CROSSHAIR: return "crosshair";
    case MARU_CURSOR_SHAPE_TEXT: return "xterm";
    case MARU_CURSOR_SHAPE_MOVE: return "move";
    case MARU_CURSOR_SHAPE_NOT_ALLOWED: return "not-allowed";
    case MARU_CURSOR_SHAPE_EW_RESIZE: return "ew-resize";
    case MARU_CURSOR_SHAPE_NS_RESIZE: return "ns-resize";
    case MARU_CURSOR_SHAPE_NESW_RESIZE: return "nesw-resize";
    case MARU_CURSOR_SHAPE_NWSE_RESIZE: return "nwse-resize";
    default: return "left_ptr";
  }
}

void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial) {
  if (!ctx->input.pointer) {
    return;
  }

  // Allow update even without focus when serial is 0 (used during cursor destruction)
  if (serial != 0 && ctx->focused.pointer_focus != (MARU_Window *)window) {
    return;
  }

  if (window->cursor_mode == MARU_CURSOR_HIDDEN || window->cursor_mode == MARU_CURSOR_LOCKED) {
    maru_wl_pointer_set_cursor(ctx, ctx->input.pointer, serial, NULL, 0, 0);
    return;
  }

  const MARU_Cursor *cursor = window->base.pub.current_cursor;
  MARU_Cursor_WL *cursor_wl = NULL;

  if (cursor && !maru_isCursorSystem(cursor)) {
    cursor_wl = (MARU_Cursor_WL *)cursor;
    if (cursor_wl->buffer) {
      maru_wl_pointer_set_cursor(ctx, ctx->input.pointer, serial, ctx->wl.cursor_surface,
                                 cursor_wl->hotspot_x, cursor_wl->hotspot_y);
      maru_wl_surface_attach(ctx, ctx->wl.cursor_surface, cursor_wl->buffer, 0, 0);
      maru_wl_surface_damage(ctx, ctx->wl.cursor_surface, 0, 0, (int32_t)cursor_wl->width, (int32_t)cursor_wl->height);
      maru_wl_surface_commit(ctx, ctx->wl.cursor_surface);
      return;
    }
  }

  MARU_CursorShape shape = MARU_CURSOR_SHAPE_DEFAULT;
  if (cursor && maru_isCursorSystem(cursor)) {
    cursor_wl = (MARU_Cursor_WL *)cursor;
    shape = cursor_wl->cursor_shape;
  }

  const char *name = _maru_cursor_shape_to_name(shape);
  struct wl_cursor *wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, name);
  if (!wl_cursor && shape != MARU_CURSOR_SHAPE_DEFAULT) {
    wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, "left_ptr");
  }

  if (wl_cursor) {
    struct wl_cursor_image *image = wl_cursor->images[0];
    struct wl_buffer *buffer = maru_wl_cursor_image_get_buffer(ctx, image);
    if (buffer) {
      maru_wl_pointer_set_cursor(ctx, ctx->input.pointer, serial, ctx->wl.cursor_surface,
                                 (int32_t)image->hotspot_x, (int32_t)image->hotspot_y);
      maru_wl_surface_attach(ctx, ctx->wl.cursor_surface, buffer, 0, 0);
      maru_wl_surface_damage(ctx, ctx->wl.cursor_surface, 0, 0, (int32_t)image->width, (int32_t)image->height);
      maru_wl_surface_commit(ctx, ctx->wl.cursor_surface);
    }
  }
}

static void _wl_seat_handle_capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !ctx->input.keyboard) {
    ctx->input.keyboard = maru_wl_seat_get_keyboard(ctx, wl_seat);
    if (ctx->input.keyboard) {
      maru_wl_keyboard_add_listener(ctx, ctx->input.keyboard, &_maru_wayland_keyboard_listener, ctx);
    }
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && ctx->input.keyboard) {
    maru_wl_keyboard_release(ctx, ctx->input.keyboard);
    ctx->input.keyboard = NULL;
  }

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !ctx->input.pointer) {
    ctx->input.pointer = maru_wl_seat_get_pointer(ctx, wl_seat);
    if (ctx->input.pointer) {
      maru_wl_pointer_add_listener(ctx, ctx->input.pointer, &_maru_wayland_pointer_listener, ctx);
    }
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && ctx->input.pointer) {
    maru_wl_pointer_release(ctx, ctx->input.pointer);
    ctx->input.pointer = NULL;
  }
}

static void _wl_seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name) {
  (void)data; (void)wl_seat; (void)name;
}

const struct wl_seat_listener _maru_wayland_seat_listener = {
  .capabilities = _wl_seat_handle_capabilities,
  .name = _wl_seat_handle_name,
};

static void _wl_keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
                                       uint32_t format, int32_t fd, uint32_t size) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }

  char *keymap_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (keymap_str == MAP_FAILED) {
    close(fd);
    return;
  }

  close(fd);

  if (ctx->linux_common.xkb.keymap) {
    maru_xkb_keymap_unref(ctx, ctx->linux_common.xkb.keymap);
  }
  if (ctx->linux_common.xkb.state) {
    maru_xkb_state_unref(ctx, ctx->linux_common.xkb.state);
  }

  ctx->linux_common.xkb.keymap = maru_xkb_keymap_new_from_string(ctx, ctx->linux_common.xkb.ctx, keymap_str);
  munmap(keymap_str, size);

  if (!ctx->linux_common.xkb.keymap) {
    return;
  }

  ctx->linux_common.xkb.state = maru_xkb_state_new(ctx, ctx->linux_common.xkb.keymap);
}

static void _wl_keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                      uint32_t serial, struct wl_surface *surface,
                                      struct wl_array *keys) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_keyboard; (void)serial; (void)keys;

  ctx->focused.keyboard_focus = (MARU_Window *)maru_wl_surface_get_user_data(ctx, surface);
}

static void _wl_keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                     uint32_t serial, struct wl_surface *surface) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_keyboard; (void)serial; (void)surface;

  ctx->focused.keyboard_focus = NULL;
}

static void _wl_keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                    uint32_t serial, uint32_t time, uint32_t key,
                                    uint32_t state) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)serial; (void)time;

  if (!ctx->focused.keyboard_focus) return;

  MARU_Key raw_key = _maru_wl_keycode_to_maru(key);
  MARU_ButtonState button_state = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;

  MARU_Event evt = {0};
  evt.key.raw_key = raw_key;
  evt.key.state = button_state;
  evt.key.modifiers = _maru_wayland_get_modifiers(ctx);

  _maru_dispatch_event(&ctx->base, MARU_KEY_STATE_CHANGED, ctx->focused.keyboard_focus, &evt);
}

static void _wl_keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                          uint32_t serial, uint32_t mods_depressed,
                                          uint32_t mods_latched, uint32_t mods_locked,
                                          uint32_t group) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)serial;

  if (ctx->linux_common.xkb.state) {
    maru_xkb_state_update_mask(ctx, ctx->linux_common.xkb.state, mods_depressed, mods_latched, mods_locked, group);
  }
}

static void _wl_keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                            int32_t rate, int32_t delay) {
  (void)data; (void)wl_keyboard; (void)rate; (void)delay;
}

const struct wl_keyboard_listener _maru_wayland_keyboard_listener = {
  .keymap = _wl_keyboard_handle_keymap,
  .enter = _wl_keyboard_handle_enter,
  .leave = _wl_keyboard_handle_leave,
  .key = _wl_keyboard_handle_key,
  .modifiers = _wl_keyboard_handle_modifiers,
  .repeat_info = _wl_keyboard_handle_repeat_info,
};

static void _wl_pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
                                      uint32_t serial, struct wl_surface *surface,
                                      wl_fixed_t surface_x, wl_fixed_t surface_y) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_pointer; (void)surface_x; (void)surface_y;

  ctx->focused.pointer_serial = serial;
  ctx->focused.pointer_surface = surface;
  MARU_Window *window = (MARU_Window *)maru_wl_surface_get_user_data(ctx, surface);
  ctx->focused.pointer_focus = window;

  if (window) {
    MARU_Window_WL *window_wl = (MARU_Window_WL *)window;
    _maru_wayland_update_cursor(ctx, window_wl, serial);
  }
}

static void _wl_pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
                                     uint32_t serial, struct wl_surface *surface) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_pointer; (void)serial; (void)surface;

  ctx->focused.pointer_surface = NULL;
  ctx->focused.pointer_focus = NULL;
}

static void _wl_pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
                                      uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_pointer; (void)time;

  int32_t x = wl_fixed_to_int(surface_x);
  int32_t y = wl_fixed_to_int(surface_y);

  MARU_Event evt = {0};
  evt.mouse_motion.position.x = (MARU_Scalar)x;
  evt.mouse_motion.position.y = (MARU_Scalar)y;

  MARU_Window *window = ctx->focused.pointer_focus;
  if (!window && ctx->focused.pointer_surface) {
    window = (MARU_Window *)maru_wl_surface_get_user_data(ctx, ctx->focused.pointer_surface);
  }

  _maru_dispatch_event(&ctx->base, MARU_MOUSE_MOVED, window, &evt);
}

static void _wl_pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                       uint32_t serial, uint32_t time, uint32_t button,
                                       uint32_t state) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)serial; (void)time;

  MARU_MouseButton maru_button;
  switch (button) {
    case 0x110: maru_button = MARU_MOUSE_BUTTON_LEFT; break;
    case 0x111: maru_button = MARU_MOUSE_BUTTON_RIGHT; break;
    case 0x112: maru_button = MARU_MOUSE_BUTTON_MIDDLE; break;
    case 0x113: maru_button = MARU_MOUSE_BUTTON_4; break;
    case 0x114: maru_button = MARU_MOUSE_BUTTON_5; break;
    default: maru_button = MARU_MOUSE_BUTTON_LEFT; break;
  }

  MARU_Event evt = {0};
  evt.mouse_button.button = maru_button;
  evt.mouse_button.state = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
  evt.mouse_button.modifiers = _maru_wayland_get_modifiers(ctx);

  MARU_Window *window = ctx->focused.pointer_focus;
  if (!window && ctx->focused.pointer_surface) {
    window = (MARU_Window *)maru_wl_surface_get_user_data(ctx, ctx->focused.pointer_surface);
  }

  _maru_dispatch_event(&ctx->base, MARU_MOUSE_BUTTON_STATE_CHANGED, window, &evt);
}

static void _wl_pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                                     uint32_t time, uint32_t axis, wl_fixed_t value) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  (void)wl_pointer; (void)time;

  MARU_Event evt = {0};

  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    evt.mouse_scroll.delta.y = (MARU_Scalar)((double)wl_fixed_to_int(value) / 10.0);
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    evt.mouse_scroll.delta.x = (MARU_Scalar)((double)wl_fixed_to_int(value) / 10.0);
  }

  MARU_Window *window = ctx->focused.pointer_focus;
  if (!window && ctx->focused.pointer_surface) {
    window = (MARU_Window *)maru_wl_surface_get_user_data(ctx, ctx->focused.pointer_surface);
  }

  _maru_dispatch_event(&ctx->base, MARU_MOUSE_SCROLLED, window, &evt);
}

static void _wl_pointer_handle_frame(void *data, struct wl_pointer *wl_pointer) {
  (void)data; (void)wl_pointer;
}

const struct wl_pointer_listener _maru_wayland_pointer_listener = {
  .enter = _wl_pointer_handle_enter,
  .leave = _wl_pointer_handle_leave,
  .motion = _wl_pointer_handle_motion,
  .button = _wl_pointer_handle_button,
  .axis = _wl_pointer_handle_axis,
  .frame = _wl_pointer_handle_frame,
};

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_WL));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t*)ctx) + sizeof(MARU_Context_Base), 0, sizeof(MARU_Context_WL) - sizeof(MARU_Context_Base));

  ctx->base.backend_type = MARU_BACKEND_WAYLAND;

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
  ctx->base.event_mask = create_info->attributes.event_mask;

  if (create_info->tuning) {
    ctx->base.tuning = *create_info->tuning;
    ctx->decoration_mode = create_info->tuning->wayland.decoration_mode;
  } else {
    MARU_ContextTuning default_tuning = MARU_CONTEXT_TUNING_DEFAULT;
    ctx->base.tuning = default_tuning;
    ctx->decoration_mode = MARU_WAYLAND_DECORATION_MODE_AUTO;
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
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to create XKB context");
    goto cleanup_symbols;
  }

  if (!_maru_wayland_connect_display(ctx)) {
    goto cleanup_xkb;
  }

  ctx->wl.registry = maru_wl_display_get_registry(ctx, ctx->wl.display);
  if (!ctx->wl.registry) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to obtain Wayland registry");
    goto cleanup_display;
  }

  maru_wl_registry_add_listener(ctx, ctx->wl.registry, &_registry_listener, ctx);
  
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "wl_display_roundtrip() failure");
    goto cleanup_registry;
  }

  // Check if required interfaces are bound
  bool missing_required = false;
#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (!ctx->protocols.iface_name) { missing_required = true; }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_BINDING_ENTRY

  if (missing_required) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, "Required Wayland interfaces missing");
    goto cleanup_registry;
  }

  // Second roundtrip to process additional events
  if (maru_wl_display_roundtrip(ctx, ctx->wl.display) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "wl_display_roundtrip() failure (second pass)");
    goto cleanup_registry;
  }

  ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, NULL, 24, ctx->protocols.wl_shm);
  ctx->wl.cursor_surface = maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);

  if (ctx->dlib.opt.decor.base.available) {
    ctx->libdecor.ctx = maru_libdecor_new(ctx, ctx->wl.display, NULL);
  }

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

  if (ctx->libdecor.ctx) {
    maru_libdecor_unref(ctx, ctx->libdecor.ctx);
  }

  if (ctx->input.keyboard) {
    maru_wl_keyboard_release(ctx, ctx->input.keyboard);
  }
  if (ctx->input.pointer) {
    maru_wl_pointer_release(ctx, ctx->input.pointer);
  }

  if (ctx->wl.cursor_surface) {
    maru_wl_surface_destroy(ctx, ctx->wl.cursor_surface);
  }
  if (ctx->wl.cursor_theme) {
    maru_wl_cursor_theme_destroy(ctx, ctx->wl.cursor_theme);
  }

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (ctx->protocols.iface_name) { maru_##iface_name##_destroy(ctx, ctx->protocols.iface_name); }
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
#undef MARU_WL_REGISTRY_REQUIRED_BINDINGS

#define MARU_WL_REGISTRY_BINDING_ENTRY(iface_name, iface_version, listener) \
  if (ctx->protocols.opt.iface_name) { maru_##iface_name##_destroy(ctx, ctx->protocols.opt.iface_name); }
  MARU_WL_REGISTRY_OPTIONAL_BINDINGS
#undef MARU_WL_REGISTRY_OPTIONAL_BINDINGS

  if (ctx->wl.registry) {
    maru_wl_registry_destroy(ctx, ctx->wl.registry);
  }

  _maru_wayland_disconnect_display(ctx);

  if (ctx->linux_common.xkb.state) maru_xkb_state_unref(ctx, ctx->linux_common.xkb.state);
  if (ctx->linux_common.xkb.keymap) maru_xkb_keymap_unref(ctx, ctx->linux_common.xkb.keymap);
  if (ctx->linux_common.xkb.ctx) maru_xkb_context_unref(ctx, ctx->linux_common.xkb.ctx);

  maru_unload_wayland_symbols(&ctx->dlib.wl, &ctx->dlib.wlc, &ctx->linux_common.xkb_lib, &ctx->dlib.opt.decor);

  _maru_cleanup_context_base(&ctx->base);

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_PumpContext pump_ctx = {
    .callback = callback,
    .userdata = userdata
  };
  ctx->base.pump_ctx = &pump_ctx;

  // TODO: Implement frame collapsing for Wayland events.
  // Currently, we dispatch all events as they arrive (direct-dispatch model).
  // For better performance with the compositor, we should batch related events
  // (e.g., multiple pointer motion events, or motion + button + scroll within a frame)
  // using the wl_pointer.frame, wl_keyboard.enter/leave events to identify event groups.

  MARU_Status status = MARU_SUCCESS;

  // 1. Dispatch any pending events
  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
    status = MARU_FAILURE;
    goto cleanup;
  }

  // 2. Prepare to read from the display socket
  while (maru_wl_display_prepare_read(ctx, ctx->wl.display) != 0) {
      if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
          status = MARU_FAILURE;
          goto cleanup;
      }
  }

  // 3. Flush any outgoing requests before blocking/polling
  // If flush fails with EAGAIN, we need to poll for writing as well.
  int flush_ret = maru_wl_display_flush(ctx, ctx->wl.display);
  if (flush_ret < 0 && errno != EAGAIN) {
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      status = MARU_FAILURE;
      goto cleanup;
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
              status = MARU_FAILURE;
              goto cleanup;
          }
      } else {
          // Only writable or other event, cancel read since we didn't read
          maru_wl_display_cancel_read(ctx, ctx->wl.display);
      }
  } else {
      // Timeout or Error
      maru_wl_display_cancel_read(ctx, ctx->wl.display);
      if (ret < 0 && errno != EINTR) {
          status = MARU_FAILURE;
          goto cleanup;
      }
  }

  // 5. Dispatch any events we just read
  if (maru_wl_display_dispatch_pending(ctx, ctx->wl.display) < 0) {
    status = MARU_FAILURE;
  }

cleanup:
  ctx->base.pump_ctx = NULL;
  return status;
}

MARU_Status maru_getStandardCursor_WL(MARU_Context *context, MARU_CursorShape shape,
                                     MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_Cursor_WL *cursor = (MARU_Cursor_WL *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) return MARU_FAILURE;

  memset(cursor, 0, sizeof(MARU_Cursor_WL));
  cursor->base.ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  cursor->base.backend = ctx->base.backend;
#endif
  cursor->base.pub.flags = MARU_CURSOR_FLAG_SYSTEM;
  cursor->cursor_shape = shape;

  const char *name = _maru_cursor_shape_to_name(shape);
  cursor->wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, name);
  if (!cursor->wl_cursor && shape != MARU_CURSOR_SHAPE_DEFAULT) {
    cursor->wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, "left_ptr");
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  MARU_Cursor_WL *cursor = (MARU_Cursor_WL *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) return MARU_FAILURE;

  memset(cursor, 0, sizeof(MARU_Cursor_WL));
  cursor->base.ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  cursor->base.backend = ctx->base.backend;
#endif
  cursor->base.pub.flags = 0;
  cursor->width = (uint32_t)create_info->size.x;
  cursor->height = (uint32_t)create_info->size.y;
  cursor->hotspot_x = (int32_t)create_info->hot_spot.x;
  cursor->hotspot_y = (int32_t)create_info->hot_spot.y;

  size_t size = (size_t)(cursor->width * cursor->height * 4);

  int fd = memfd_create("maru-cursor", MFD_CLOEXEC);
  if (fd < 0) {
    maru_context_free(&ctx->base, cursor);
    return MARU_FAILURE;
  }

  if (ftruncate(fd, (off_t)size) < 0) {
    close(fd);
    maru_context_free(&ctx->base, cursor);
    return MARU_FAILURE;
  }

  uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    close(fd);
    maru_context_free(&ctx->base, cursor);
    return MARU_FAILURE;
  }

  for (size_t i = 0; i < cursor->width * cursor->height; ++i) {
    uint32_t rgba = create_info->pixels[i];
    uint32_t r = (rgba >> 0) & 0xFF;
    uint32_t g = (rgba >> 8) & 0xFF;
    uint32_t b = (rgba >> 16) & 0xFF;
    uint32_t a = (rgba >> 24) & 0xFF;
    data[i] = (a << 24) | (r << 16) | (g << 8) | b;
  }

  munmap(data, size);

  struct wl_shm_pool *pool = maru_wl_shm_create_pool(ctx, ctx->protocols.wl_shm, fd, (int32_t)size);
  cursor->buffer = maru_wl_shm_pool_create_buffer(ctx, pool, 0, (int32_t)cursor->width, (int32_t)cursor->height, (int32_t)(cursor->width * 4), 0);

  maru_wl_shm_pool_destroy(ctx, pool);
  close(fd);

  if (!cursor->buffer) {
    maru_context_free(&ctx->base, cursor);
    return MARU_FAILURE;
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor) {
  MARU_Cursor_WL *cursor_wl = (MARU_Cursor_WL *)cursor;
  MARU_Context_WL *ctx = (MARU_Context_WL *)cursor_wl->base.ctx_base;

  for (uint32_t i = 0; i < ctx->base.window_cache_count; i++) {
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->base.window_cache[i];
    if (window->base.pub.current_cursor == cursor) {
      window->base.pub.current_cursor = NULL;
      _maru_wayland_update_cursor(ctx, window, 0);
    }
  }

  if (cursor_wl->buffer) {
    maru_wl_buffer_destroy(ctx, cursor_wl->buffer);
  }

  maru_context_free(cursor_wl->base.ctx_base, cursor_wl);
  return MARU_SUCCESS;
}

void *_maru_getContextNativeHandle_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return ctx->wl.display;
}
