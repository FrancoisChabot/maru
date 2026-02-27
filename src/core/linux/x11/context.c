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
#include <limits.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <time.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

bool _maru_x11_apply_window_cursor_mode(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_release_pointer_lock(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
static bool _maru_x11_enable_xi2_raw_motion(MARU_Context_X11 *ctx);

MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                    const MARU_ContextAttributes *attributes) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    // X11 idle inhibition plumbing is not implemented yet.
  }

  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context);

static bool _maru_x11_init_context_mouse_channels(MARU_Context_X11 *ctx) {
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

  if (!_maru_x11_init_context_mouse_channels(ctx)) {
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  if (!maru_load_x11_symbols(&ctx->base, &ctx->x11_lib)) {
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }
  (void)maru_load_xcursor_symbols(&ctx->base, &ctx->xcursor_lib);
  (void)maru_load_xi2_symbols(&ctx->base, &ctx->xi2_lib);

  ctx->display = ctx->x11_lib.XOpenDisplay(NULL);
  if (!ctx->display) {
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
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
  (void)_maru_x11_enable_xi2_raw_motion(ctx);

  if (!_maru_linux_common_init(&ctx->linux_common, &ctx->base)) {
    ctx->x11_lib.XCloseDisplay(ctx->display);
    maru_unload_xi2_symbols(&ctx->xi2_lib);
    maru_unload_xcursor_symbols(&ctx->xcursor_lib);
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
  _maru_x11_release_pointer_lock(ctx, NULL);
  
  while (ctx->base.window_list_head) {
    extern MARU_Status maru_destroyWindow_X11(MARU_Window * window);
    maru_destroyWindow_X11((MARU_Window *)ctx->base.window_list_head);
  }

  _maru_linux_common_cleanup(&ctx->linux_common);
  
  if (ctx->display) {
    if (ctx->hidden_cursor) {
      ctx->x11_lib.XFreeCursor(ctx->display, ctx->hidden_cursor);
      ctx->hidden_cursor = (Cursor)0;
    }
    ctx->x11_lib.XCloseDisplay(ctx->display);
  }
  maru_unload_xcursor_symbols(&ctx->xcursor_lib);
  maru_unload_xi2_symbols(&ctx->xi2_lib);
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

static MARU_Window_X11 *_maru_x11_find_window(MARU_Context_X11 *ctx, Window handle) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)it;
    if (win->handle == handle) {
      return win;
    }
  }
  return NULL;
}

static bool _maru_x11_enable_xi2_raw_motion(MARU_Context_X11 *ctx) {
  if (!ctx->xi2_lib.base.available) {
    return false;
  }

  int xi_event = 0;
  int xi_error = 0;
  int xi_opcode = 0;
  if (!ctx->x11_lib.XQueryExtension(ctx->display, "XInputExtension", &xi_opcode,
                                    &xi_event, &xi_error)) {
    return false;
  }

  int xi_major = 2;
  int xi_minor = 0;
  if (ctx->xi2_lib.XIQueryVersion(ctx->display, &xi_major, &xi_minor) != Success) {
    return false;
  }

  unsigned char mask[(XI_LASTEVENT + 7) / 8];
  memset(mask, 0, sizeof(mask));
  XISetMask(mask, XI_RawMotion);

  XIEventMask event_mask;
  event_mask.deviceid = XIAllMasterDevices;
  event_mask.mask_len = (int)sizeof(mask);
  event_mask.mask = mask;
  if (ctx->xi2_lib.XISelectEvents(ctx->display, ctx->root, &event_mask, 1) != Success) {
    return false;
  }

  ctx->xi2_opcode = xi_opcode;
  ctx->xi2_raw_motion_enabled = true;
  ctx->x11_lib.XFlush(ctx->display);
  return true;
}

extern void maru_getWindowGeometry_X11(MARU_Window *window,
                               MARU_WindowGeometry *out_geometry);

static MARU_ModifierFlags _maru_x11_get_modifiers(unsigned int state) {
  MARU_ModifierFlags mods = 0;
  if (state & ShiftMask) mods |= MARU_MODIFIER_SHIFT;
  if (state & ControlMask) mods |= MARU_MODIFIER_CONTROL;
  if (state & Mod1Mask) mods |= MARU_MODIFIER_ALT;
  if (state & Mod4Mask) mods |= MARU_MODIFIER_META;
  if (state & LockMask) mods |= MARU_MODIFIER_CAPS_LOCK;
  if (state & Mod2Mask) mods |= MARU_MODIFIER_NUM_LOCK;
  return mods;
}

static MARU_Key _maru_x11_map_keysym(KeySym keysym) {
  switch (keysym) {
    case XK_space: return MARU_KEY_SPACE;
    case XK_apostrophe: return MARU_KEY_APOSTROPHE;
    case XK_comma: return MARU_KEY_COMMA;
    case XK_minus: return MARU_KEY_MINUS;
    case XK_period: return MARU_KEY_PERIOD;
    case XK_slash: return MARU_KEY_SLASH;
    case XK_0: return MARU_KEY_0;
    case XK_1: return MARU_KEY_1;
    case XK_2: return MARU_KEY_2;
    case XK_3: return MARU_KEY_3;
    case XK_4: return MARU_KEY_4;
    case XK_5: return MARU_KEY_5;
    case XK_6: return MARU_KEY_6;
    case XK_7: return MARU_KEY_7;
    case XK_8: return MARU_KEY_8;
    case XK_9: return MARU_KEY_9;
    case XK_semicolon: return MARU_KEY_SEMICOLON;
    case XK_equal: return MARU_KEY_EQUAL;
    case XK_a: case XK_A: return MARU_KEY_A;
    case XK_b: case XK_B: return MARU_KEY_B;
    case XK_c: case XK_C: return MARU_KEY_C;
    case XK_d: case XK_D: return MARU_KEY_D;
    case XK_e: case XK_E: return MARU_KEY_E;
    case XK_f: case XK_F: return MARU_KEY_F;
    case XK_g: case XK_G: return MARU_KEY_G;
    case XK_h: case XK_H: return MARU_KEY_H;
    case XK_i: case XK_I: return MARU_KEY_I;
    case XK_j: case XK_J: return MARU_KEY_J;
    case XK_k: case XK_K: return MARU_KEY_K;
    case XK_l: case XK_L: return MARU_KEY_L;
    case XK_m: case XK_M: return MARU_KEY_M;
    case XK_n: case XK_N: return MARU_KEY_N;
    case XK_o: case XK_O: return MARU_KEY_O;
    case XK_p: case XK_P: return MARU_KEY_P;
    case XK_q: case XK_Q: return MARU_KEY_Q;
    case XK_r: case XK_R: return MARU_KEY_R;
    case XK_s: case XK_S: return MARU_KEY_S;
    case XK_t: case XK_T: return MARU_KEY_T;
    case XK_u: case XK_U: return MARU_KEY_U;
    case XK_v: case XK_V: return MARU_KEY_V;
    case XK_w: case XK_W: return MARU_KEY_W;
    case XK_x: case XK_X: return MARU_KEY_X;
    case XK_y: case XK_Y: return MARU_KEY_Y;
    case XK_z: case XK_Z: return MARU_KEY_Z;
    case XK_bracketleft: return MARU_KEY_LEFT_BRACKET;
    case XK_backslash: return MARU_KEY_BACKSLASH;
    case XK_bracketright: return MARU_KEY_RIGHT_BRACKET;
    case XK_grave: return MARU_KEY_GRAVE_ACCENT;
    case XK_Escape: return MARU_KEY_ESCAPE;
    case XK_Return: return MARU_KEY_ENTER;
    case XK_Tab: return MARU_KEY_TAB;
    case XK_BackSpace: return MARU_KEY_BACKSPACE;
    case XK_Insert: return MARU_KEY_INSERT;
    case XK_Delete: return MARU_KEY_DELETE;
    case XK_Right: return MARU_KEY_RIGHT;
    case XK_Left: return MARU_KEY_LEFT;
    case XK_Down: return MARU_KEY_DOWN;
    case XK_Up: return MARU_KEY_UP;
    case XK_Page_Up: return MARU_KEY_PAGE_UP;
    case XK_Page_Down: return MARU_KEY_PAGE_DOWN;
    case XK_Home: return MARU_KEY_HOME;
    case XK_End: return MARU_KEY_END;
    case XK_Caps_Lock: return MARU_KEY_CAPS_LOCK;
    case XK_Scroll_Lock: return MARU_KEY_SCROLL_LOCK;
    case XK_Num_Lock: return MARU_KEY_NUM_LOCK;
    case XK_Print: return MARU_KEY_PRINT_SCREEN;
    case XK_Pause: return MARU_KEY_PAUSE;
    case XK_F1: return MARU_KEY_F1;
    case XK_F2: return MARU_KEY_F2;
    case XK_F3: return MARU_KEY_F3;
    case XK_F4: return MARU_KEY_F4;
    case XK_F5: return MARU_KEY_F5;
    case XK_F6: return MARU_KEY_F6;
    case XK_F7: return MARU_KEY_F7;
    case XK_F8: return MARU_KEY_F8;
    case XK_F9: return MARU_KEY_F9;
    case XK_F10: return MARU_KEY_F10;
    case XK_F11: return MARU_KEY_F11;
    case XK_F12: return MARU_KEY_F12;
    case XK_KP_0: return MARU_KEY_KP_0;
    case XK_KP_1: return MARU_KEY_KP_1;
    case XK_KP_2: return MARU_KEY_KP_2;
    case XK_KP_3: return MARU_KEY_KP_3;
    case XK_KP_4: return MARU_KEY_KP_4;
    case XK_KP_5: return MARU_KEY_KP_5;
    case XK_KP_6: return MARU_KEY_KP_6;
    case XK_KP_7: return MARU_KEY_KP_7;
    case XK_KP_8: return MARU_KEY_KP_8;
    case XK_KP_9: return MARU_KEY_KP_9;
    case XK_KP_Decimal: return MARU_KEY_KP_DECIMAL;
    case XK_KP_Divide: return MARU_KEY_KP_DIVIDE;
    case XK_KP_Multiply: return MARU_KEY_KP_MULTIPLY;
    case XK_KP_Subtract: return MARU_KEY_KP_SUBTRACT;
    case XK_KP_Add: return MARU_KEY_KP_ADD;
    case XK_KP_Enter: return MARU_KEY_KP_ENTER;
    case XK_KP_Equal: return MARU_KEY_KP_EQUAL;
    case XK_Shift_L: return MARU_KEY_LEFT_SHIFT;
    case XK_Control_L: return MARU_KEY_LEFT_CONTROL;
    case XK_Alt_L: return MARU_KEY_LEFT_ALT;
    case XK_Super_L: return MARU_KEY_LEFT_META;
    case XK_Shift_R: return MARU_KEY_RIGHT_SHIFT;
    case XK_Control_R: return MARU_KEY_RIGHT_CONTROL;
    case XK_Alt_R: return MARU_KEY_RIGHT_ALT;
    case XK_Super_R: return MARU_KEY_RIGHT_META;
    case XK_Menu: return MARU_KEY_MENU;
    default: return MARU_KEY_UNKNOWN;
  }
}

static uint32_t _maru_x11_button_to_native_code(unsigned int x_button) {
  switch (x_button) {
    case Button1: return BTN_LEFT;
    case Button2: return BTN_MIDDLE;
    case Button3: return BTN_RIGHT;
    case 8: return BTN_SIDE;
    case 9: return BTN_EXTRA;
    default: return 0;
  }
}

static uint32_t _maru_x11_find_mouse_button_id(const MARU_Context_X11 *ctx,
                                               uint32_t native_code) {
  for (uint32_t i = 0; i < ctx->base.pub.mouse_button_count; ++i) {
    if (ctx->base.pub.mouse_button_channels[i].native_code == native_code) {
      return i;
    }
  }
  return UINT32_MAX;
}

static uint64_t _maru_x11_get_monotonic_time_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static uint64_t _maru_x11_get_monotonic_time_ms(void) {
  const uint64_t ns = _maru_x11_get_monotonic_time_ns();
  if (ns == 0) {
    return 0;
  }
  return ns / 1000000ull;
}

static uint32_t _maru_x11_cursor_frame_delay_ms(uint32_t delay_ms) {
  return (delay_ms == 0) ? 1u : delay_ms;
}

static void _maru_x11_clear_locked_raw_accum(MARU_Context_X11 *ctx) {
  ctx->locked_raw_dx_accum = (MARU_Scalar)0.0;
  ctx->locked_raw_dy_accum = (MARU_Scalar)0.0;
  ctx->locked_raw_pending = false;
}

static bool _maru_x11_ensure_hidden_cursor(MARU_Context_X11 *ctx) {
  if (ctx->hidden_cursor) {
    return true;
  }

  static const char empty_data[1] = {0};
  Pixmap bitmap = ctx->x11_lib.XCreateBitmapFromData(ctx->display, ctx->root,
                                                     empty_data, 1u, 1u);
  if (!bitmap) {
    return false;
  }

  XColor black;
  memset(&black, 0, sizeof(black));
  Cursor cursor = ctx->x11_lib.XCreatePixmapCursor(
      ctx->display, bitmap, bitmap, &black, &black, 0u, 0u);
  ctx->x11_lib.XFreePixmap(ctx->display, bitmap);
  if (!cursor) {
    return false;
  }

  ctx->hidden_cursor = cursor;
  return true;
}

void _maru_x11_release_pointer_lock(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (ctx->locked_window && (!win || ctx->locked_window == win)) {
    ctx->x11_lib.XUngrabPointer(ctx->display, CurrentTime);
    if (ctx->locked_window) {
      ctx->locked_window->suppress_lock_warp_event = false;
    }
    ctx->locked_window = NULL;
    _maru_x11_clear_locked_raw_accum(ctx);
  }
}

static void _maru_x11_recenter_locked_pointer(MARU_Context_X11 *ctx,
                                              MARU_Window_X11 *win) {
  if (!win->handle) {
    return;
  }
  win->suppress_lock_warp_event = true;
  ctx->x11_lib.XWarpPointer(ctx->display, None, win->handle, 0, 0, 0, 0,
                            win->lock_center_x, win->lock_center_y);
}

bool _maru_x11_apply_window_cursor_mode(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (!ctx || !win || !win->handle) {
    return false;
  }

  if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED) {
    if (!_maru_x11_ensure_hidden_cursor(ctx)) {
      return false;
    }

    const int32_t width = (int32_t)win->server_logical_size.x;
    const int32_t height = (int32_t)win->server_logical_size.y;
    win->lock_center_x = (width > 0) ? (width / 2) : 0;
    win->lock_center_y = (height > 0) ? (height / 2) : 0;

    const bool focused =
        (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    if (focused) {
      if (ctx->locked_window != win) {
        _maru_x11_release_pointer_lock(ctx, NULL);
        const int grab_result = ctx->x11_lib.XGrabPointer(
            ctx->display, win->handle, True,
            (unsigned int)(PointerMotionMask | ButtonPressMask | ButtonReleaseMask),
            GrabModeAsync, GrabModeAsync, win->handle, ctx->hidden_cursor, CurrentTime);
        if (grab_result != GrabSuccess) {
          return false;
        }
        ctx->locked_window = win;
        _maru_x11_clear_locked_raw_accum(ctx);
      }
    }

    ctx->x11_lib.XDefineCursor(ctx->display, win->handle, ctx->hidden_cursor);
    if (focused && ctx->locked_window == win) {
      _maru_x11_recenter_locked_pointer(ctx, win);
    }
    return true;
  }

  if (ctx->locked_window == win) {
    _maru_x11_release_pointer_lock(ctx, win);
  }

  if (win->base.pub.cursor_mode == MARU_CURSOR_HIDDEN) {
    if (!_maru_x11_ensure_hidden_cursor(ctx)) {
      return false;
    }
    ctx->x11_lib.XDefineCursor(ctx->display, win->handle, ctx->hidden_cursor);
    return true;
  }

  if (win->base.pub.current_cursor) {
    MARU_Cursor_X11 *cursor = (MARU_Cursor_X11 *)win->base.pub.current_cursor;
    if (cursor->base.ctx_base == &ctx->base && cursor->handle) {
      ctx->x11_lib.XDefineCursor(ctx->display, win->handle, cursor->handle);
      return true;
    }
  }

  ctx->x11_lib.XUndefineCursor(ctx->display, win->handle);
  return true;
}

static void _maru_x11_record_pump_duration_ns(MARU_Context_X11 *ctx,
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

static bool _maru_x11_reapply_cursor_to_windows(MARU_Context_X11 *ctx,
                                                const MARU_Cursor_X11 *cursor) {
  bool applied = false;
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)it;
    if (win->base.pub.current_cursor != (const MARU_Cursor *)cursor ||
        win->base.pub.cursor_mode != MARU_CURSOR_NORMAL || !win->handle ||
        !cursor->handle) {
      continue;
    }
    ctx->x11_lib.XDefineCursor(ctx->display, win->handle, cursor->handle);
    applied = true;
  }
  return applied;
}

static void _maru_x11_link_animated_cursor(MARU_Context_X11 *ctx,
                                           MARU_Cursor_X11 *cursor) {
  cursor->anim_prev = NULL;
  cursor->anim_next = ctx->animated_cursors_head;
  if (ctx->animated_cursors_head) {
    ctx->animated_cursors_head->anim_prev = cursor;
  }
  ctx->animated_cursors_head = cursor;
}

static void _maru_x11_unlink_animated_cursor(MARU_Context_X11 *ctx,
                                             MARU_Cursor_X11 *cursor) {
  if (cursor->anim_prev) {
    cursor->anim_prev->anim_next = cursor->anim_next;
  } else if (ctx->animated_cursors_head == cursor) {
    ctx->animated_cursors_head = cursor->anim_next;
  }
  if (cursor->anim_next) {
    cursor->anim_next->anim_prev = cursor->anim_prev;
  }
  cursor->anim_prev = NULL;
  cursor->anim_next = NULL;
}

static uint64_t _maru_x11_next_cursor_deadline_ms(const MARU_Context_X11 *ctx) {
  uint64_t next_deadline = 0;
  for (const MARU_Cursor_X11 *cur = ctx->animated_cursors_head; cur;
       cur = cur->anim_next) {
    if (!cur->is_animated || cur->frame_count <= 1 || cur->next_frame_deadline_ms == 0) {
      continue;
    }
    if (next_deadline == 0 || cur->next_frame_deadline_ms < next_deadline) {
      next_deadline = cur->next_frame_deadline_ms;
    }
  }
  return next_deadline;
}

static int _maru_x11_compute_poll_timeout_ms(MARU_Context_X11 *ctx,
                                             uint32_t timeout_ms) {
  int timeout = -1;
  if (timeout_ms != MARU_NEVER) {
    timeout = (timeout_ms > (uint32_t)INT_MAX) ? INT_MAX : (int)timeout_ms;
  }
  const uint64_t cursor_deadline_ms = _maru_x11_next_cursor_deadline_ms(ctx);
  if (cursor_deadline_ms == 0) {
    return timeout;
  }

  const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
  if (now_ms == 0) {
    return timeout;
  }
  if (cursor_deadline_ms <= now_ms) {
    return 0;
  }

  uint64_t wait_ms_u64 = cursor_deadline_ms - now_ms;
  if (wait_ms_u64 > (uint64_t)INT_MAX) {
    wait_ms_u64 = (uint64_t)INT_MAX;
  }
  const int cursor_timeout_ms = (int)wait_ms_u64;
  if (timeout < 0 || cursor_timeout_ms < timeout) {
    timeout = cursor_timeout_ms;
  }
  return timeout;
}

static bool _maru_x11_advance_animated_cursors(MARU_Context_X11 *ctx,
                                               uint64_t now_ms) {
  bool any_applied = false;
  const uint32_t max_advance_per_pump = 16u;
  for (MARU_Cursor_X11 *cur = ctx->animated_cursors_head; cur; cur = cur->anim_next) {
    if (!cur->is_animated || cur->frame_count <= 1 || !cur->frame_handles ||
        !cur->frame_delays_ms) {
      continue;
    }
    if (cur->next_frame_deadline_ms == 0) {
      cur->next_frame_deadline_ms = now_ms + (uint64_t)cur->frame_delays_ms[cur->current_frame];
    }
    if (now_ms < cur->next_frame_deadline_ms) {
      continue;
    }

    uint32_t advanced = 0;
    while (now_ms >= cur->next_frame_deadline_ms && advanced < max_advance_per_pump) {
      cur->current_frame = (cur->current_frame + 1u) % cur->frame_count;
      cur->handle = cur->frame_handles[cur->current_frame];
      cur->next_frame_deadline_ms +=
          (uint64_t)cur->frame_delays_ms[cur->current_frame];
      advanced++;
    }
    if (advanced == max_advance_per_pump && now_ms >= cur->next_frame_deadline_ms) {
      cur->next_frame_deadline_ms = now_ms + 1u;
    }
    if (advanced > 0 && _maru_x11_reapply_cursor_to_windows(ctx, cur)) {
      any_applied = true;
    }
  }
  return any_applied;
}

static void _maru_x11_process_event(MARU_Context_X11 *ctx, XEvent *ev) {
  if (ev->type == GenericEvent && ctx->xi2_raw_motion_enabled &&
      ev->xcookie.extension == ctx->xi2_opcode &&
      ctx->x11_lib.XGetEventData(ctx->display, &ev->xcookie)) {
    if (ev->xcookie.evtype == XI_RawMotion && ctx->locked_window &&
        (ctx->locked_window->base.pub.cursor_mode == MARU_CURSOR_LOCKED)) {
      const XIRawEvent *raw = (const XIRawEvent *)ev->xcookie.data;
      if (raw && raw->raw_values && raw->valuators.mask &&
          raw->valuators.mask_len > 0) {
        const double *values = raw->raw_values;
        const int bit_count = raw->valuators.mask_len * 8;
        MARU_Scalar raw_dx = (MARU_Scalar)0.0;
        MARU_Scalar raw_dy = (MARU_Scalar)0.0;
        for (int bit = 0; bit < bit_count; ++bit) {
          if (!XIMaskIsSet(raw->valuators.mask, bit)) {
            continue;
          }
          const MARU_Scalar value = (MARU_Scalar)(*values++);
          if (bit == 0) {
            raw_dx += value;
          } else if (bit == 1) {
            raw_dy += value;
          }
        }
        if (raw_dx != (MARU_Scalar)0.0 || raw_dy != (MARU_Scalar)0.0) {
          ctx->locked_raw_dx_accum += raw_dx;
          ctx->locked_raw_dy_accum += raw_dy;
          ctx->locked_raw_pending = true;
        }
      }
    }
    ctx->x11_lib.XFreeEventData(ctx->display, &ev->xcookie);
  }

  switch (ev->type) {
    case ClientMessage: {
      if (ev->xclient.message_type == ctx->wm_protocols &&
          (Atom)ev->xclient.data.l[0] == ctx->wm_delete_window) {
        MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
        if (win) {
          fprintf(stderr, "[X11 DEBUG] Close requested\n");
          MARU_Event mevt = {0};
          _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)win, &mevt);
        }
      }
      break;
    }
    case ConfigureNotify: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xconfigure.window);
      if (win) {
        const MARU_Scalar new_w = (MARU_Scalar)ev->xconfigure.width;
        const MARU_Scalar new_h = (MARU_Scalar)ev->xconfigure.height;
        const bool changed = (win->server_logical_size.x != new_w) ||
                             (win->server_logical_size.y != new_h);
        
        if (changed) {
          win->server_logical_size.x = new_w;
          win->server_logical_size.y = new_h;
          if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED &&
              ctx->locked_window == win) {
            win->lock_center_x = ev->xconfigure.width / 2;
            win->lock_center_y = ev->xconfigure.height / 2;
            _maru_x11_recenter_locked_pointer(ctx, win);
          }

          const bool viewport_active =
              (win->base.attrs_effective.viewport_size.x > (MARU_Scalar)0.0) &&
              (win->base.attrs_effective.viewport_size.y > (MARU_Scalar)0.0);
          if (!viewport_active) {
            win->base.attrs_effective.logical_size = win->server_logical_size;
          }

          MARU_Event mevt = {0};
          mevt.resized.geometry.logical_size = win->server_logical_size;
          mevt.resized.geometry.pixel_size.x = ev->xconfigure.width;
          mevt.resized.geometry.pixel_size.y = ev->xconfigure.height;
          mevt.resized.geometry.scale = (MARU_Scalar)1.0;
          mevt.resized.geometry.buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
          _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)win, &mevt);
        }
      }
      break;
    }
    case Expose: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xexpose.window);
      if (win && ev->xexpose.count == 0) {
        fprintf(stderr, "[X11 DEBUG] Expose event\n");
        MARU_Event mevt = {0};
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME, (MARU_Window *)win, &mevt);
      }
      break;
    }
    case MapNotify: {
      fprintf(stderr, "[X11 DEBUG] MapNotify\n");
      break;
    }
    case FocusIn: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xfocus.window);
      if (win) {
        fprintf(stderr, "[X11 DEBUG] FocusIn\n");
        win->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
        ctx->linux_common.xkb.focused_window = (MARU_Window *)win;
        (void)_maru_x11_apply_window_cursor_mode(ctx, win);
        MARU_Event mevt = {0};
        mevt.presentation.changed_fields = MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED;
        mevt.presentation.focused = true;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED, (MARU_Window *)win, &mevt);
      }
      break;
    }
    case FocusOut: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xfocus.window);
      if (win) {
        fprintf(stderr, "[X11 DEBUG] FocusOut\n");
        win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
        if (ctx->locked_window == win) {
          _maru_x11_release_pointer_lock(ctx, win);
        }
        memset(win->base.keyboard_state, 0, sizeof(win->base.keyboard_state));
        if (ctx->linux_common.xkb.focused_window == (MARU_Window *)win) {
          ctx->linux_common.xkb.focused_window = NULL;
        }
        MARU_Event mevt = {0};
        mevt.presentation.changed_fields = MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED;
        mevt.presentation.focused = false;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED, (MARU_Window *)win, &mevt);
      }
      break;
    }
    case MotionNotify: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xmotion.window);
      if (win) {
        if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED &&
            ctx->locked_window == win) {
          ctx->linux_common.pointer.focused_window = (MARU_Window *)win;
          if (win->suppress_lock_warp_event &&
              ev->xmotion.x == win->lock_center_x &&
              ev->xmotion.y == win->lock_center_y) {
            win->suppress_lock_warp_event = false;
            break;
          }

          const MARU_Scalar dx =
              (MARU_Scalar)(ev->xmotion.x - win->lock_center_x);
          const MARU_Scalar dy =
              (MARU_Scalar)(ev->xmotion.y - win->lock_center_y);
          if (dx != (MARU_Scalar)0.0 || dy != (MARU_Scalar)0.0) {
            MARU_Event mevt = {0};
            mevt.mouse_motion.position.x = (MARU_Scalar)ctx->linux_common.pointer.x;
            mevt.mouse_motion.position.y = (MARU_Scalar)ctx->linux_common.pointer.y;
            mevt.mouse_motion.delta.x = dx;
            mevt.mouse_motion.delta.y = dy;
            if (ctx->locked_raw_pending) {
              mevt.mouse_motion.raw_delta.x = ctx->locked_raw_dx_accum;
              mevt.mouse_motion.raw_delta.y = ctx->locked_raw_dy_accum;
            } else {
              mevt.mouse_motion.raw_delta.x = dx;
              mevt.mouse_motion.raw_delta.y = dy;
            }
            _maru_x11_clear_locked_raw_accum(ctx);
            mevt.mouse_motion.modifiers = _maru_x11_get_modifiers(ev->xmotion.state);
            _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED,
                                 (MARU_Window *)win, &mevt);
            _maru_x11_recenter_locked_pointer(ctx, win);
          }
          break;
        }

        _maru_x11_clear_locked_raw_accum(ctx);

        MARU_Event mevt = {0};
        const MARU_Scalar x = (MARU_Scalar)ev->xmotion.x;
        const MARU_Scalar y = (MARU_Scalar)ev->xmotion.y;
        mevt.mouse_motion.position.x = x;
        mevt.mouse_motion.position.y = y;
        if (ctx->linux_common.pointer.focused_window == (MARU_Window *)win) {
          mevt.mouse_motion.delta.x = x - (MARU_Scalar)ctx->linux_common.pointer.x;
          mevt.mouse_motion.delta.y = y - (MARU_Scalar)ctx->linux_common.pointer.y;
        }
        mevt.mouse_motion.raw_delta.x = (MARU_Scalar)0.0;
        mevt.mouse_motion.raw_delta.y = (MARU_Scalar)0.0;
        mevt.mouse_motion.modifiers = _maru_x11_get_modifiers(ev->xmotion.state);

        ctx->linux_common.pointer.focused_window = (MARU_Window *)win;
        ctx->linux_common.pointer.x = (double)x;
        ctx->linux_common.pointer.y = (double)y;

        _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED, (MARU_Window *)win, &mevt);
      }
      break;
    }
    case ButtonPress:
    case ButtonRelease: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xbutton.window);
      if (!win) break;

      const bool is_press = (ev->type == ButtonPress);
      const unsigned int btn = ev->xbutton.button;

      if (is_press && (btn >= 4 && btn <= 7)) {
        MARU_Event mevt = {0};
        mevt.mouse_scroll.modifiers = _maru_x11_get_modifiers(ev->xbutton.state);
        if (btn == 4) {
          mevt.mouse_scroll.delta.y = (MARU_Scalar)1.0;
          mevt.mouse_scroll.steps.y = 1;
        } else if (btn == 5) {
          mevt.mouse_scroll.delta.y = (MARU_Scalar)-1.0;
          mevt.mouse_scroll.steps.y = -1;
        } else if (btn == 6) {
          mevt.mouse_scroll.delta.x = (MARU_Scalar)-1.0;
          mevt.mouse_scroll.steps.x = -1;
        } else {
          mevt.mouse_scroll.delta.x = (MARU_Scalar)1.0;
          mevt.mouse_scroll.steps.x = 1;
        }
        _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_SCROLLED, (MARU_Window *)win, &mevt);
        break;
      }

      const uint32_t native_code = _maru_x11_button_to_native_code(btn);
      if (native_code == 0) break;

      const uint32_t button_id = _maru_x11_find_mouse_button_id(ctx, native_code);
      if (button_id == UINT32_MAX) break;

      const MARU_ButtonState state =
          is_press ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
      ctx->base.mouse_button_states[button_id] = (MARU_ButtonState8)state;

      MARU_Event mevt = {0};
      mevt.mouse_button.button_id = button_id;
      mevt.mouse_button.state = state;
      mevt.mouse_button.modifiers = _maru_x11_get_modifiers(ev->xbutton.state);
      _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)win, &mevt);
      break;
    }
    case KeyPress:
    case KeyRelease: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xkey.window);
      if (!win) break;

      const bool is_press = (ev->type == KeyPress);
      KeySym keysym = NoSymbol;
      char text_buf[32];
      const int text_len = ctx->x11_lib.XLookupString(&ev->xkey, text_buf, sizeof(text_buf), &keysym, NULL);
      const MARU_Key key = _maru_x11_map_keysym(keysym);
      const MARU_ButtonState state =
          is_press ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;

      bool is_repeat = false;
      if (key != MARU_KEY_UNKNOWN) {
        is_repeat = is_press && (win->base.keyboard_state[key] == (MARU_ButtonState8)MARU_BUTTON_STATE_PRESSED);
        win->base.keyboard_state[key] = (MARU_ButtonState8)state;
      }

      if (!is_repeat) {
        MARU_Event mevt = {0};
        mevt.key.raw_key = key;
        mevt.key.state = state;
        mevt.key.modifiers = _maru_x11_get_modifiers(ev->xkey.state);
        _maru_dispatch_event(&ctx->base, MARU_EVENT_KEY_STATE_CHANGED, (MARU_Window *)win, &mevt);
      }

      if (is_press && text_len > 0) {
        MARU_Event text_evt = {0};
        text_evt.text_edit_commit.committed_utf8 = text_buf;
        text_evt.text_edit_commit.committed_length = (uint32_t)text_len;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)win, &text_evt);
      }
      break;
    }
  }
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  const uint64_t pump_start_ns = _maru_x11_get_monotonic_time_ns();
  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  // 1. Drain hotplug events from worker thread
  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  _maru_drain_queued_events(&ctx->base);

  // 2. Process already pending X11 events
  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      if (ctx->x11_lib.XFilterEvent(&ev, None)) continue;
      _maru_x11_process_event(ctx, &ev);
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0 && _maru_x11_advance_animated_cursors(ctx, now_ms)) {
      ctx->x11_lib.XFlush(ctx->display);
    }
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

  int poll_timeout = _maru_x11_compute_poll_timeout_ms(ctx, timeout_ms);
  int ret = poll(pfds, nfds, poll_timeout);

  if (ret > 0) {
    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      if (read(ctx->linux_common.worker.event_fd, &val, sizeof(val)) < 0) {}
    }

    if (pfds[1].revents & POLLIN) {
        // X11 events available
    }

    if (ctrl_count > 0) {
      _maru_linux_common_process_pollfds(&ctx->linux_common, &pfds[ctrl_start_idx], ctrl_count);
    }
  }

  // Drain again to catch anything that came in during poll
  while (ctx->x11_lib.XPending(ctx->display)) {
      XEvent ev;
      ctx->x11_lib.XNextEvent(ctx->display, &ev);
      if (ctx->x11_lib.XFilterEvent(&ev, None)) continue;
      _maru_x11_process_event(ctx, &ev);
  }

  {
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0 && _maru_x11_advance_animated_cursors(ctx, now_ms)) {
      ctx->x11_lib.XFlush(ctx->display);
    }
  }

  _maru_linux_common_drain_internal_events(&ctx->linux_common);
  ctx->base.pump_ctx = NULL;

  const uint64_t pump_end_ns = _maru_x11_get_monotonic_time_ns();
  if (pump_start_ns != 0 && pump_end_ns != 0 && pump_end_ns >= pump_start_ns) {
    _maru_x11_record_pump_duration_ns(ctx, pump_end_ns - pump_start_ns);
  }

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

void maru_getWindowGeometry_X11(MARU_Window *window,
                               MARU_WindowGeometry *out_geometry) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  memset(out_geometry, 0, sizeof(*out_geometry));
  
  out_geometry->logical_size = win->base.attrs_effective.logical_size;
  out_geometry->pixel_size.x = (int32_t)win->base.attrs_effective.logical_size.x;
  out_geometry->pixel_size.y = (int32_t)win->base.attrs_effective.logical_size.y;
  out_geometry->scale = (MARU_Scalar)1.0;
  out_geometry->buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
  fprintf(stderr, "[X11 DEBUG] getWindowGeometry: %fx%f (%dx%d)\n", 
          out_geometry->logical_size.x, out_geometry->logical_size.y,
          out_geometry->pixel_size.x, out_geometry->pixel_size.y);
}

MARU_Status maru_createWindow_X11(MARU_Context *context,
                                 const MARU_WindowCreateInfo *create_info,
                                 MARU_Window **out_window) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  MARU_Window_X11 *win = (MARU_Window_X11 *)maru_context_alloc(&ctx->base, sizeof(MARU_Window_X11));
  if (!win) return MARU_FAILURE;

  memset(win, 0, sizeof(*win));
  win->base.ctx_base = &ctx->base;
  win->base.pub.context = context;
  win->base.pub.userdata = create_info->userdata;
  win->base.pub.metrics = &win->base.metrics;
  win->base.pub.keyboard_state = win->base.keyboard_state;
  win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  win->base.pub.mouse_button_state = ctx->base.pub.mouse_button_state;
  win->base.pub.mouse_button_channels = ctx->base.pub.mouse_button_channels;
  win->base.pub.mouse_button_count = ctx->base.pub.mouse_button_count;
  memcpy(win->base.pub.mouse_default_button_channels, ctx->base.pub.mouse_default_button_channels, sizeof(win->base.pub.mouse_default_button_channels));
  win->base.pub.event_mask = create_info->attributes.event_mask;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = create_info->attributes.cursor;
  win->base.pub.title = create_info->attributes.title;
  win->base.attrs_requested = create_info->attributes;
  win->base.attrs_effective = create_info->attributes;
  win->base.attrs_dirty_mask = 0;

#ifdef MARU_INDIRECT_BACKEND
  win->base.backend = ctx->base.backend;
#endif

  int32_t width = (int32_t)create_info->attributes.logical_size.x;
  int32_t height = (int32_t)create_info->attributes.logical_size.y;
  if (width <= 0) width = 640;
  if (height <= 0) height = 480;

  Visual *visual = DefaultVisual(ctx->display, ctx->screen);
  int depth = DefaultDepth(ctx->display, ctx->screen);

  XSetWindowAttributes swa;
  swa.colormap = ctx->x11_lib.XCreateColormap(ctx->display, ctx->root, visual, AllocNone);
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask;
  swa.border_pixel = 0;
  swa.background_pixel = 0;

  win->colormap = swa.colormap;
  win->handle = ctx->x11_lib.XCreateWindow(ctx->display, ctx->root, 
                                           0, 0, (uint32_t)width, (uint32_t)height, 
                                           0, depth, InputOutput, visual, 
                                           CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa);

  if (!win->handle) {
    ctx->x11_lib.XFreeColormap(ctx->display, win->colormap);
    maru_context_free(&ctx->base, win);
    return MARU_FAILURE;
  }

  ctx->x11_lib.XSetWMProtocols(ctx->display, win->handle, &ctx->wm_delete_window, 1);

  _maru_register_window(&ctx->base, (MARU_Window *)win);
  
  win->base.pub.flags = MARU_WINDOW_STATE_READY;
  if (create_info->attributes.visible) {
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
  }
  if (create_info->attributes.resizable) {
    win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }
  if (create_info->decorated) {
    win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  if (create_info->attributes.minimized) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
  }
  if (create_info->attributes.fullscreen) {
    win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  }
  if (create_info->attributes.maximized) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
  }
  if (create_info->attributes.mouse_passthrough) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
  }
  win->base.attrs_effective.logical_size.x = (MARU_Scalar)width;
  win->base.attrs_effective.logical_size.y = (MARU_Scalar)height;
  win->server_logical_size = win->base.attrs_effective.logical_size;

  ctx->x11_lib.XMapWindow(ctx->display, win->handle);
  (void)_maru_x11_apply_window_cursor_mode(ctx, win);
  ctx->x11_lib.XFlush(ctx->display);

  // Window creation happens outside maru_pumpEvents(), so dispatching directly
  // would drop these events when no pump callback is installed yet.
  MARU_Event mevt = {0};
  _maru_post_event_internal(&ctx->base, MARU_EVENT_WINDOW_READY, (MARU_Window *)win, &mevt);

  MARU_Event revt = {0};
  maru_getWindowGeometry_X11((MARU_Window *)win, &revt.resized.geometry);
  _maru_post_event_internal(&ctx->base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)win, &revt);

  *out_window = (MARU_Window *)win;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  if (ctx->locked_window == win) {
    _maru_x11_release_pointer_lock(ctx, win);
  }

  if (win->handle) {
    ctx->x11_lib.XUnmapWindow(ctx->display, win->handle);
    ctx->x11_lib.XDestroyWindow(ctx->display, win->handle);
    win->handle = 0;
  }

  if (win->colormap) {
    ctx->x11_lib.XFreeColormap(ctx->display, win->colormap);
    win->colormap = 0;
  }

  _maru_unregister_window(&ctx->base, (MARU_Window *)win);
  if (win->base.title_storage) {
    maru_context_free(&ctx->base, win->base.title_storage);
    win->base.title_storage = NULL;
  }
  if (win->base.surrounding_text_storage) {
    maru_context_free(&ctx->base, win->base.surrounding_text_storage);
    win->base.surrounding_text_storage = NULL;
  }
  maru_context_free(&ctx->base, win);
  return MARU_SUCCESS;
}

static bool _maru_x11_create_custom_cursor_frame(MARU_Context_X11 *ctx,
                                                 const MARU_CursorFrame *frame,
                                                 Cursor *out_handle) {
  const MARU_Image_Base *image = (const MARU_Image_Base *)frame->image;
  if (image->ctx_base != &ctx->base || !image->pixels || image->width == 0 ||
      image->height == 0) {
    return false;
  }

  XcursorImage *xc_img =
      ctx->xcursor_lib.XcursorImageCreate((int)image->width, (int)image->height);
  if (!xc_img || !xc_img->pixels) {
    if (xc_img) {
      ctx->xcursor_lib.XcursorImageDestroy(xc_img);
    }
    return false;
  }

  xc_img->xhot = (XcursorDim)((frame->hot_spot.x < 0) ? 0 : frame->hot_spot.x);
  xc_img->yhot = (XcursorDim)((frame->hot_spot.y < 0) ? 0 : frame->hot_spot.y);
  if (xc_img->xhot >= xc_img->width) {
    xc_img->xhot = xc_img->width ? (xc_img->width - 1u) : 0u;
  }
  if (xc_img->yhot >= xc_img->height) {
    xc_img->yhot = xc_img->height ? (xc_img->height - 1u) : 0u;
  }

  const uint32_t *src = (const uint32_t *)image->pixels;
  const size_t pixel_count = (size_t)image->width * (size_t)image->height;
  for (size_t i = 0; i < pixel_count; ++i) {
    // MARU_ImageCreateInfo defines pixels as ARGB8888.
    xc_img->pixels[i] = src[i];
  }

  *out_handle = ctx->xcursor_lib.XcursorImageLoadCursor(ctx->display, xc_img);
  ctx->xcursor_lib.XcursorImageDestroy(xc_img);
  return *out_handle != (Cursor)0;
}

MARU_Status maru_createCursor_X11(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  Cursor cursor_handle = (Cursor)0;
  bool is_system = false;
  Cursor *anim_frame_handles = NULL;
  uint32_t *anim_frame_delays_ms = NULL;
  uint32_t anim_frame_count = 0;

  if (create_info->source == MARU_CURSOR_SOURCE_SYSTEM) {
    unsigned int shape = XC_left_ptr;
    switch (create_info->system_shape) {
      case MARU_CURSOR_SHAPE_DEFAULT: shape = XC_left_ptr; break;
      case MARU_CURSOR_SHAPE_HELP: shape = XC_question_arrow; break;
      case MARU_CURSOR_SHAPE_HAND: shape = XC_hand2; break;
      case MARU_CURSOR_SHAPE_WAIT: shape = XC_watch; break;
      case MARU_CURSOR_SHAPE_CROSSHAIR: shape = XC_crosshair; break;
      case MARU_CURSOR_SHAPE_TEXT: shape = XC_xterm; break;
      case MARU_CURSOR_SHAPE_MOVE: shape = XC_fleur; break;
      case MARU_CURSOR_SHAPE_NOT_ALLOWED: shape = XC_X_cursor; break;
      case MARU_CURSOR_SHAPE_EW_RESIZE: shape = XC_sb_h_double_arrow; break;
      case MARU_CURSOR_SHAPE_NS_RESIZE: shape = XC_sb_v_double_arrow; break;
      case MARU_CURSOR_SHAPE_NESW_RESIZE: shape = XC_bottom_left_corner; break;
      case MARU_CURSOR_SHAPE_NWSE_RESIZE: shape = XC_bottom_right_corner; break;
      default: return MARU_FAILURE;
    }
    cursor_handle = ctx->x11_lib.XCreateFontCursor(ctx->display, shape);
    if (!cursor_handle) {
      return MARU_FAILURE;
    }
    is_system = true;
  } else {
    if (!create_info->frames || create_info->frame_count == 0 ||
        !create_info->frames[0].image) {
      return MARU_FAILURE;
    }
    if (!ctx->xcursor_lib.base.available) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Custom cursors require libXcursor");
      return MARU_FAILURE;
    }

    if (create_info->frame_count > 1) {
      anim_frame_count = create_info->frame_count;
      anim_frame_handles = (Cursor *)maru_context_alloc(
          &ctx->base, sizeof(Cursor) * (size_t)anim_frame_count);
      anim_frame_delays_ms = (uint32_t *)maru_context_alloc(
          &ctx->base, sizeof(uint32_t) * (size_t)anim_frame_count);
      if (!anim_frame_handles || !anim_frame_delays_ms) {
        if (anim_frame_handles) {
          maru_context_free(&ctx->base, anim_frame_handles);
        }
        if (anim_frame_delays_ms) {
          maru_context_free(&ctx->base, anim_frame_delays_ms);
        }
        return MARU_FAILURE;
      }
      memset(anim_frame_handles, 0, sizeof(Cursor) * (size_t)anim_frame_count);
      for (uint32_t i = 0; i < anim_frame_count; ++i) {
        const MARU_CursorFrame *frame = &create_info->frames[i];
        if (!frame->image ||
            !_maru_x11_create_custom_cursor_frame(ctx, frame, &anim_frame_handles[i])) {
          for (uint32_t j = 0; j < anim_frame_count; ++j) {
            if (anim_frame_handles[j]) {
              ctx->x11_lib.XFreeCursor(ctx->display, anim_frame_handles[j]);
            }
          }
          maru_context_free(&ctx->base, anim_frame_delays_ms);
          maru_context_free(&ctx->base, anim_frame_handles);
          return MARU_FAILURE;
        }
        anim_frame_delays_ms[i] = _maru_x11_cursor_frame_delay_ms(frame->delay_ms);
      }
      cursor_handle = anim_frame_handles[0];
    } else {
      if (!_maru_x11_create_custom_cursor_frame(ctx, &create_info->frames[0],
                                                &cursor_handle)) {
        return MARU_FAILURE;
      }
    }
  }

  MARU_Cursor_X11 *cursor =
      (MARU_Cursor_X11 *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_X11));
  if (!cursor) {
    if (anim_frame_handles && anim_frame_delays_ms) {
      for (uint32_t i = 0; i < anim_frame_count; ++i) {
        if (anim_frame_handles[i]) {
          ctx->x11_lib.XFreeCursor(ctx->display, anim_frame_handles[i]);
        }
      }
      maru_context_free(&ctx->base, anim_frame_delays_ms);
      maru_context_free(&ctx->base, anim_frame_handles);
    } else if (cursor_handle) {
      ctx->x11_lib.XFreeCursor(ctx->display, cursor_handle);
    }
    return MARU_FAILURE;
  }
  memset(cursor, 0, sizeof(MARU_Cursor_X11));

  cursor->base.ctx_base = &ctx->base;
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.userdata = create_info->userdata;
  cursor->base.pub.flags = is_system ? MARU_CURSOR_FLAG_SYSTEM : 0;
#ifdef MARU_INDIRECT_BACKEND
  cursor->base.backend = ctx->base.backend;
#endif
  cursor->handle = cursor_handle;
  cursor->is_system = is_system;
  if (anim_frame_handles && anim_frame_delays_ms && anim_frame_count > 1) {
    cursor->is_animated = true;
    cursor->frame_handles = anim_frame_handles;
    cursor->frame_delays_ms = anim_frame_delays_ms;
    cursor->frame_count = anim_frame_count;
    cursor->current_frame = 0;
    const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();
    if (now_ms != 0) {
      cursor->next_frame_deadline_ms =
          now_ms + (uint64_t)cursor->frame_delays_ms[cursor->current_frame];
    }
    _maru_x11_link_animated_cursor(ctx, cursor);
  }

  ctx->base.metrics.cursor_create_count_total++;
  if (is_system) ctx->base.metrics.cursor_create_count_system++;
  else ctx->base.metrics.cursor_create_count_custom++;
  ctx->base.metrics.cursor_alive_current++;
  if (ctx->base.metrics.cursor_alive_current > ctx->base.metrics.cursor_alive_peak) {
    ctx->base.metrics.cursor_alive_peak = ctx->base.metrics.cursor_alive_current;
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_X11(MARU_Cursor *cursor) {
  MARU_Cursor_X11 *cur = (MARU_Cursor_X11 *)cursor;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)cur->base.ctx_base;

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)it;
    if (win->base.pub.current_cursor != cursor) {
      continue;
    }
    win->base.pub.current_cursor = NULL;
    win->base.attrs_requested.cursor = NULL;
    win->base.attrs_effective.cursor = NULL;
    if (win->base.pub.cursor_mode == MARU_CURSOR_NORMAL) {
      ctx->x11_lib.XUndefineCursor(ctx->display, win->handle);
    }
  }

  if (cur->is_animated) {
    _maru_x11_unlink_animated_cursor(ctx, cur);
    if (cur->frame_handles) {
      for (uint32_t i = 0; i < cur->frame_count; ++i) {
        if (cur->frame_handles[i]) {
          ctx->x11_lib.XFreeCursor(ctx->display, cur->frame_handles[i]);
        }
      }
      maru_context_free(&ctx->base, cur->frame_handles);
      cur->frame_handles = NULL;
    }
    if (cur->frame_delays_ms) {
      maru_context_free(&ctx->base, cur->frame_delays_ms);
      cur->frame_delays_ms = NULL;
    }
    cur->frame_count = 0;
    cur->current_frame = 0;
    cur->next_frame_deadline_ms = 0;
    cur->handle = 0;
  } else if (cur->handle) {
    ctx->x11_lib.XFreeCursor(ctx->display, cur->handle);
    cur->handle = 0;
  }

  ctx->base.metrics.cursor_destroy_count_total++;
  if (ctx->base.metrics.cursor_alive_current > 0) {
    ctx->base.metrics.cursor_alive_current--;
  }

  maru_context_free(&ctx->base, cur);
  return MARU_SUCCESS;
}

MARU_Status maru_createImage_X11(MARU_Context *context,
                                        const MARU_ImageCreateInfo *create_info,
                                        MARU_Image **out_image) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  if (!create_info->pixels || create_info->size.x <= 0 || create_info->size.y <= 0) {
    return MARU_FAILURE;
  }

  const uint32_t width = (uint32_t)create_info->size.x;
  const uint32_t height = (uint32_t)create_info->size.y;
  const uint32_t min_stride = width * 4u;
  const uint32_t stride = (create_info->stride_bytes == 0) ? min_stride : create_info->stride_bytes;
  if (stride < min_stride) {
    return MARU_FAILURE;
  }

  MARU_Image_Base *image = (MARU_Image_Base *)maru_context_alloc(&ctx->base, sizeof(MARU_Image_Base));
  if (!image) {
    return MARU_FAILURE;
  }
  memset(image, 0, sizeof(MARU_Image_Base));

  const size_t packed_stride = (size_t)min_stride;
  const size_t dst_size = packed_stride * (size_t)height;
  image->pixels = (uint8_t *)maru_context_alloc(&ctx->base, dst_size);
  if (!image->pixels) {
    maru_context_free(&ctx->base, image);
    return MARU_FAILURE;
  }

  const uint8_t *src = (const uint8_t *)create_info->pixels;
  for (uint32_t y = 0; y < height; ++y) {
    memcpy(image->pixels + (size_t)y * packed_stride,
           src + (size_t)y * (size_t)stride,
           packed_stride);
  }

  image->ctx_base = &ctx->base;
  image->pub.userdata = NULL;
#ifdef MARU_INDIRECT_BACKEND
  image->backend = ctx->base.backend;
#endif
  image->width = width;
  image->height = height;
  image->stride_bytes = min_stride;

  *out_image = (MARU_Image *)image;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyImage_X11(MARU_Image *image) {
  MARU_Image_Base *img = (MARU_Image_Base *)image;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)img->ctx_base;
  if (img->pixels) {
    maru_context_free(&ctx->base, img->pixels);
    img->pixels = NULL;
  }
  maru_context_free(&ctx->base, img);
  return MARU_SUCCESS;
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
