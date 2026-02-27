// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <string.h>
#include <linux/input-event-codes.h>
#include <locale.h>
#include <wchar.h>
#include <stdlib.h>

bool _maru_x11_init_context_mouse_channels(MARU_Context_X11 *ctx) {
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

bool _maru_x11_enable_xi2_raw_motion(MARU_Context_X11 *ctx) {
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

MARU_ModifierFlags _maru_x11_get_modifiers(unsigned int state) {
  MARU_ModifierFlags mods = 0;
  if (state & ShiftMask) mods |= MARU_MODIFIER_SHIFT;
  if (state & ControlMask) mods |= MARU_MODIFIER_CONTROL;
  if (state & Mod1Mask) mods |= MARU_MODIFIER_ALT;
  if (state & Mod4Mask) mods |= MARU_MODIFIER_META;
  if (state & LockMask) mods |= MARU_MODIFIER_CAPS_LOCK;
  if (state & Mod2Mask) mods |= MARU_MODIFIER_NUM_LOCK;
  return mods;
}

MARU_Key _maru_x11_map_keysym(KeySym keysym) {
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

uint32_t _maru_x11_button_to_native_code(unsigned int x_button) {
  switch (x_button) {
    case Button1: return BTN_LEFT;
    case Button2: return BTN_MIDDLE;
    case Button3: return BTN_RIGHT;
    case 8: return BTN_SIDE;
    case 9: return BTN_EXTRA;
    default: return 0;
  }
}

uint32_t _maru_x11_find_mouse_button_id(const MARU_Context_X11 *ctx,
                                               uint32_t native_code) {
  for (uint32_t i = 0; i < ctx->base.pub.mouse_button_count; ++i) {
    if (ctx->base.pub.mouse_button_channels[i].native_code == native_code) {
      return i;
    }
  }
  return UINT32_MAX;
}

bool _maru_x11_replace_utf8_storage(MARU_Context_X11 *ctx, char **storage,
                                           const char *text, size_t len) {
  char *copy = NULL;
  if (len > 0) {
    copy = (char *)maru_context_alloc(&ctx->base, len + 1u);
    if (!copy) {
      return false;
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
  }
  if (*storage) {
    maru_context_free(&ctx->base, *storage);
  }
  *storage = copy;
  return true;
}

static bool _maru_x11_copy_xim_text(MARU_Context_X11 *ctx, const XIMText *text,
                                    char **out_utf8, size_t *out_len) {
  *out_utf8 = NULL;
  *out_len = 0;
  if (!text || text->length == 0) {
    return true;
  }

  if (text->encoding_is_wchar) {
    const wchar_t *w = text->string.wide_char;
    if (!w) return true;
    const size_t needed = wcstombs(NULL, w, 0);
    if (needed == (size_t)-1) {
      return false;
    }
    char *buf = (char *)maru_context_alloc(&ctx->base, needed + 1u);
    if (!buf) {
      return false;
    }
    const size_t written = wcstombs(buf, w, needed + 1u);
    if (written == (size_t)-1) {
      maru_context_free(&ctx->base, buf);
      return false;
    }
    *out_utf8 = buf;
    *out_len = written;
    return true;
  }

  if (!text->string.multi_byte) {
    return true;
  }
  const size_t len = (size_t)text->length;
  char *buf = (char *)maru_context_alloc(&ctx->base, len + 1u);
  if (!buf) {
    return false;
  }
  memcpy(buf, text->string.multi_byte, len);
  buf[len] = '\0';
  *out_utf8 = buf;
  *out_len = len;
  return true;
}

static bool _maru_x11_update_ic_spot(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (!win->xic) {
    return true;
  }
  XPoint spot;
  spot.x = (short)win->base.attrs_effective.text_input_rect.origin.x;
  spot.y = (short)(win->base.attrs_effective.text_input_rect.origin.y +
                   win->base.attrs_effective.text_input_rect.size.y);

  XVaNestedList preedit_attrs =
      ctx->x11_lib.XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
  if (!preedit_attrs) {
    return false;
  }

  (void)ctx->x11_lib.XSetICValues(win->xic, XNPreeditAttributes, preedit_attrs,
                                  NULL);
  ctx->x11_lib.XFree(preedit_attrs);
  return true;
}

static void _maru_x11_dispatch_preedit_update(MARU_Context_X11 *ctx,
                                              MARU_Window_X11 *win,
                                              int32_t caret_pos_hint) {
  const char *preedit = win->ime_preedit_storage ? win->ime_preedit_storage : "";
  const uint32_t len = (uint32_t)strlen(preedit);
  uint32_t caret = 0;
  if (caret_pos_hint > 0) {
    caret = (uint32_t)caret_pos_hint;
    if (caret > len) {
      caret = len;
    }
  }

  MARU_Event evt = {0};
  evt.text_edit_update.session_id = win->text_input_session_id;
  evt.text_edit_update.preedit_utf8 = preedit;
  evt.text_edit_update.preedit_length = len;
  evt.text_edit_update.caret.start_byte = caret;
  evt.text_edit_update.caret.length_byte = 0;
  evt.text_edit_update.selection.start_byte = caret;
  evt.text_edit_update.selection.length_byte = 0;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_UPDATE, (MARU_Window *)win,
                       &evt);
}

static void _maru_x11_emit_reset_commit(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (!win->xic) {
    return;
  }
  char *committed = ctx->x11_lib.XmbResetIC(win->xic);
  if (!committed || committed[0] == '\0') {
    return;
  }
  MARU_Event evt = {0};
  evt.text_edit_commit.session_id = win->text_input_session_id;
  evt.text_edit_commit.committed_utf8 = committed;
  evt.text_edit_commit.committed_length = (uint32_t)strlen(committed);
  _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)win,
                       &evt);
  ctx->x11_lib.XFree(committed);
}

void _maru_x11_xim_preedit_draw(XIC xic, XPointer client_data,
                                       XPointer call_data) {
  (void)xic;
  MARU_Window_X11 *win = (MARU_Window_X11 *)client_data;
  if (!win || !win->base.ctx_base || !call_data) {
    return;
  }
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  XIMPreeditDrawCallbackStruct *draw = (XIMPreeditDrawCallbackStruct *)call_data;

  char *text_utf8 = NULL;
  size_t text_len = 0;
  if (!draw->text) {
    (void)_maru_x11_replace_utf8_storage(ctx, &win->ime_preedit_storage, "", 0);
  } else if (_maru_x11_copy_xim_text(ctx, draw->text, &text_utf8, &text_len)) {
    if (text_utf8) {
      (void)_maru_x11_replace_utf8_storage(ctx, &win->ime_preedit_storage, text_utf8,
                                           text_len);
      maru_context_free(&ctx->base, text_utf8);
    }
  }

  win->ime_preedit_active =
      (win->ime_preedit_storage && win->ime_preedit_storage[0] != '\0');
  _maru_x11_dispatch_preedit_update(ctx, win, draw->caret);
}

void _maru_x11_xim_preedit_done(XIC xic, XPointer client_data,
                                       XPointer call_data) {
  (void)xic;
  (void)call_data;
  MARU_Window_X11 *win = (MARU_Window_X11 *)client_data;
  if (!win || !win->base.ctx_base) {
    return;
  }
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  (void)_maru_x11_replace_utf8_storage(ctx, &win->ime_preedit_storage, "", 0);
  win->ime_preedit_active = false;
  _maru_x11_dispatch_preedit_update(ctx, win, 0);
}

static void _maru_x11_begin_text_session(MARU_Context_X11 *ctx,
                                         MARU_Window_X11 *win) {
  if (!win->xic || win->text_input_session_active ||
      win->base.attrs_effective.text_input_type == MARU_TEXT_INPUT_TYPE_NONE) {
    return;
  }
  ctx->x11_lib.XSetICFocus(win->xic);
  win->text_input_session_id++;
  win->text_input_session_active = true;
  win->ime_preedit_active = false;

  MARU_Event evt = {0};
  evt.text_edit_start.session_id = win->text_input_session_id;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_START, (MARU_Window *)win,
                       &evt);
}

void _maru_x11_end_text_session(MARU_Context_X11 *ctx, MARU_Window_X11 *win,
                                       bool canceled) {
  if (!win->text_input_session_active) {
    return;
  }
  _maru_x11_emit_reset_commit(ctx, win);
  if (win->xic) {
    ctx->x11_lib.XUnsetICFocus(win->xic);
  }
  win->text_input_session_active = false;
  win->ime_preedit_active = false;
  (void)_maru_x11_replace_utf8_storage(ctx, &win->ime_preedit_storage, "", 0);

  MARU_Event evt = {0};
  evt.text_edit_end.session_id = win->text_input_session_id;
  evt.text_edit_end.canceled = canceled;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_END, (MARU_Window *)win,
                       &evt);
}

void _maru_x11_refresh_text_input_state(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  const bool ime_enabled =
      (win->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE);
  const bool focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;

  if (!ime_enabled) {
    _maru_x11_end_text_session(ctx, win, true);
    if (win->xic) {
      ctx->x11_lib.XDestroyIC(win->xic);
      win->xic = NULL;
    }
    return;
  }

  if (!win->xic && ctx->xim) {
    XIMCallback draw_cb;
    memset(&draw_cb, 0, sizeof(draw_cb));
    draw_cb.client_data = (XPointer)win;
    draw_cb.callback = (XIMProc)_maru_x11_xim_preedit_draw;

    XIMCallback done_cb;
    memset(&done_cb, 0, sizeof(done_cb));
    done_cb.client_data = (XPointer)win;
    done_cb.callback = (XIMProc)_maru_x11_xim_preedit_done;

    XPoint spot;
    spot.x = (short)win->base.attrs_effective.text_input_rect.origin.x;
    spot.y = (short)(win->base.attrs_effective.text_input_rect.origin.y +
                     win->base.attrs_effective.text_input_rect.size.y);
    XVaNestedList preedit_attrs = ctx->x11_lib.XVaCreateNestedList(
        0, XNSpotLocation, &spot, XNPreeditDrawCallback, &draw_cb,
        XNPreeditDoneCallback, &done_cb, NULL);
    win->xic = ctx->x11_lib.XCreateIC(
        ctx->xim, XNInputStyle, (XIMPreeditCallbacks | XIMStatusNothing),
        XNClientWindow, win->handle, XNFocusWindow, win->handle,
        XNPreeditAttributes, preedit_attrs, NULL);
    if (preedit_attrs) {
      ctx->x11_lib.XFree(preedit_attrs);
    }
  } else {
    (void)_maru_x11_update_ic_spot(ctx, win);
  }

  if (focused) {
    _maru_x11_begin_text_session(ctx, win);
  } else if (win->xic) {
    ctx->x11_lib.XUnsetICFocus(win->xic);
  }
}

bool _maru_x11_process_input_event(MARU_Context_X11 *ctx, XEvent *ev) {
  switch (ev->type) {
    case MotionNotify: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xmotion.window);
      if (ctx->dnd_source.active) {
        _maru_x11_update_dnd_source_target(ctx, ev->xmotion.time, ev->xmotion.x_root,
                                           ev->xmotion.y_root, true);
      }
      if (win) {
        if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED &&
            ctx->locked_window == win) {
          ctx->linux_common.pointer.focused_window = (MARU_Window *)win;
          if (win->suppress_lock_warp_event &&
              ev->xmotion.x == win->lock_center_x &&
              ev->xmotion.y == win->lock_center_y) {
            win->suppress_lock_warp_event = false;
            return true;
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
          return true;
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
      return true;
    }
    case ButtonPress:
    case ButtonRelease: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xbutton.window);
      if (ev->type == ButtonRelease && ctx->dnd_source.active) {
        MARU_X11DnDSourceSession *src = &ctx->dnd_source;
        _maru_x11_update_dnd_source_target(ctx, ev->xbutton.time, ev->xbutton.x_root,
                                           ev->xbutton.y_root, true);
        if (src->current_target_window != None && src->current_target_accepts) {
          extern void _maru_x11_send_xdnd_drop_source(MARU_Context_X11 *ctx, Window target, Window source, Time time);
          _maru_x11_send_xdnd_drop_source(ctx, src->current_target_window,
                                          src->source_window, ev->xbutton.time);
          src->awaiting_finish = true;
        } else {
          if (src->current_target_window != None) {
            extern void _maru_x11_send_xdnd_leave_source(MARU_Context_X11 *ctx, Window target, Window source);
            _maru_x11_send_xdnd_leave_source(ctx, src->current_target_window,
                                             src->source_window);
          }
          _maru_x11_clear_dnd_source_session(ctx, true, MARU_DROP_ACTION_NONE);
        }
      }
      if (!win) return true;

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
        return true;
      }

      const uint32_t native_code = _maru_x11_button_to_native_code(btn);
      if (native_code == 0) return true;

      const uint32_t button_id = _maru_x11_find_mouse_button_id(ctx, native_code);
      if (button_id == UINT32_MAX) return true;

      const MARU_ButtonState state =
          is_press ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
      ctx->base.mouse_button_states[button_id] = (MARU_ButtonState8)state;

      MARU_Event mevt = {0};
      mevt.mouse_button.button_id = button_id;
      mevt.mouse_button.state = state;
      mevt.mouse_button.modifiers = _maru_x11_get_modifiers(ev->xbutton.state);
      _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)win, &mevt);
      return true;
    }
    case KeyPress:
    case KeyRelease: {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xkey.window);
      if (!win) return true;

      const bool is_press = (ev->type == KeyPress);
      KeySym keysym = NoSymbol;
      char text_buf[64];
      int text_len = 0;
      bool emit_text = false;
      if (is_press && win->xic &&
          win->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE) {
        Status lookup_status = 0;
        text_len = ctx->x11_lib.Xutf8LookupString(
            win->xic, &ev->xkey, text_buf, (int)(sizeof(text_buf) - 1u), &keysym,
            &lookup_status);
        if (lookup_status == XBufferOverflow && text_len > 0) {
          char *dyn_buf = (char *)maru_context_alloc(&ctx->base, (size_t)text_len + 1u);
          if (dyn_buf) {
            text_len = ctx->x11_lib.Xutf8LookupString(
                win->xic, &ev->xkey, dyn_buf, text_len, &keysym, &lookup_status);
            if (text_len > 0 &&
                (lookup_status == XLookupChars || lookup_status == XLookupBoth)) {
              MARU_Event text_evt = {0};
              text_evt.text_edit_commit.session_id = win->text_input_session_id;
              text_evt.text_edit_commit.committed_utf8 = dyn_buf;
              text_evt.text_edit_commit.committed_length = (uint32_t)text_len;
              _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT,
                                   (MARU_Window *)win, &text_evt);
            }
            maru_context_free(&ctx->base, dyn_buf);
          }
          text_len = 0;
        } else {
          emit_text = (text_len > 0) &&
                      (lookup_status == XLookupChars ||
                       lookup_status == XLookupBoth);
        }
      } else {
        text_len = ctx->x11_lib.XLookupString(&ev->xkey, text_buf, (int)sizeof(text_buf),
                                              &keysym, NULL);
        emit_text = is_press && (text_len > 0);
      }
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

      if (emit_text && text_len > 0) {
        text_buf[text_len] = '\0';
        MARU_Event text_evt = {0};
        text_evt.text_edit_commit.session_id = win->text_input_session_id;
        text_evt.text_edit_commit.committed_utf8 = text_buf;
        text_evt.text_edit_commit.committed_length = (uint32_t)text_len;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)win, &text_evt);
      }
      return true;
    }
    default: return false;
  }
}
