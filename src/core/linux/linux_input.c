// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "linux_internal.h"
#include "maru_mem_internal.h"
#include <linux/input-event-codes.h>
#include <time.h>

uint64_t _maru_linux_get_monotonic_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

bool _maru_linux_map_native_mouse_button(uint32_t native_code,
                                         uint32_t *out_channel) {
  switch (native_code) {
    case BTN_LEFT: *out_channel = 0; return true;
    case BTN_RIGHT: *out_channel = 1; return true;
    case BTN_MIDDLE: *out_channel = 2; return true;
    case BTN_BACK: *out_channel = 3; return true;
    case BTN_FORWARD: *out_channel = 4; return true;
    case BTN_SIDE: *out_channel = 5; return true;
    case BTN_EXTRA: *out_channel = 6; return true;
    case BTN_TASK: *out_channel = 7; return true;
    default: return false;
  }
}

MARU_Key _maru_linux_scancode_to_maru_key(uint32_t scancode) {
  switch (scancode) {
    case KEY_ESC: return MARU_KEY_ESCAPE;
    case KEY_1: return MARU_KEY_1;
    case KEY_2: return MARU_KEY_2;
    case KEY_3: return MARU_KEY_3;
    case KEY_4: return MARU_KEY_4;
    case KEY_5: return MARU_KEY_5;
    case KEY_6: return MARU_KEY_6;
    case KEY_7: return MARU_KEY_7;
    case KEY_8: return MARU_KEY_8;
    case KEY_9: return MARU_KEY_9;
    case KEY_0: return MARU_KEY_0;
    case KEY_MINUS: return MARU_KEY_MINUS;
    case KEY_EQUAL: return MARU_KEY_EQUAL;
    case KEY_BACKSPACE: return MARU_KEY_BACKSPACE;
    case KEY_TAB: return MARU_KEY_TAB;
    case KEY_Q: return MARU_KEY_Q;
    case KEY_W: return MARU_KEY_W;
    case KEY_E: return MARU_KEY_E;
    case KEY_R: return MARU_KEY_R;
    case KEY_T: return MARU_KEY_T;
    case KEY_Y: return MARU_KEY_Y;
    case KEY_U: return MARU_KEY_U;
    case KEY_I: return MARU_KEY_I;
    case KEY_O: return MARU_KEY_O;
    case KEY_P: return MARU_KEY_P;
    case KEY_LEFTBRACE: return MARU_KEY_LEFT_BRACKET;
    case KEY_RIGHTBRACE: return MARU_KEY_RIGHT_BRACKET;
    case KEY_ENTER: return MARU_KEY_ENTER;
    case KEY_LEFTCTRL: return MARU_KEY_LEFT_CONTROL;
    case KEY_A: return MARU_KEY_A;
    case KEY_S: return MARU_KEY_S;
    case KEY_D: return MARU_KEY_D;
    case KEY_F: return MARU_KEY_F;
    case KEY_G: return MARU_KEY_G;
    case KEY_H: return MARU_KEY_H;
    case KEY_J: return MARU_KEY_J;
    case KEY_K: return MARU_KEY_K;
    case KEY_L: return MARU_KEY_L;
    case KEY_SEMICOLON: return MARU_KEY_SEMICOLON;
    case KEY_APOSTROPHE: return MARU_KEY_APOSTROPHE;
    case KEY_GRAVE: return MARU_KEY_GRAVE_ACCENT;
    case KEY_LEFTSHIFT: return MARU_KEY_LEFT_SHIFT;
    case KEY_BACKSLASH: return MARU_KEY_BACKSLASH;
    case KEY_Z: return MARU_KEY_Z;
    case KEY_X: return MARU_KEY_X;
    case KEY_C: return MARU_KEY_C;
    case KEY_V: return MARU_KEY_V;
    case KEY_B: return MARU_KEY_B;
    case KEY_N: return MARU_KEY_N;
    case KEY_M: return MARU_KEY_M;
    case KEY_COMMA: return MARU_KEY_COMMA;
    case KEY_DOT: return MARU_KEY_PERIOD;
    case KEY_SLASH: return MARU_KEY_SLASH;
    case KEY_RIGHTSHIFT: return MARU_KEY_RIGHT_SHIFT;
    case KEY_KPASTERISK: return MARU_KEY_KP_MULTIPLY;
    case KEY_LEFTALT: return MARU_KEY_LEFT_ALT;
    case KEY_SPACE: return MARU_KEY_SPACE;
    case KEY_CAPSLOCK: return MARU_KEY_CAPS_LOCK;
    case KEY_F1: return MARU_KEY_F1;
    case KEY_F2: return MARU_KEY_F2;
    case KEY_F3: return MARU_KEY_F3;
    case KEY_F4: return MARU_KEY_F4;
    case KEY_F5: return MARU_KEY_F5;
    case KEY_F6: return MARU_KEY_F6;
    case KEY_F7: return MARU_KEY_F7;
    case KEY_F8: return MARU_KEY_F8;
    case KEY_F9: return MARU_KEY_F9;
    case KEY_F10: return MARU_KEY_F10;
    case KEY_NUMLOCK: return MARU_KEY_NUM_LOCK;
    case KEY_SCROLLLOCK: return MARU_KEY_SCROLL_LOCK;
    case KEY_KP7: return MARU_KEY_KP_7;
    case KEY_KP8: return MARU_KEY_KP_8;
    case KEY_KP9: return MARU_KEY_KP_9;
    case KEY_KPMINUS: return MARU_KEY_KP_SUBTRACT;
    case KEY_KP4: return MARU_KEY_KP_4;
    case KEY_KP5: return MARU_KEY_KP_5;
    case KEY_KP6: return MARU_KEY_KP_6;
    case KEY_KPPLUS: return MARU_KEY_KP_ADD;
    case KEY_KP1: return MARU_KEY_KP_1;
    case KEY_KP2: return MARU_KEY_KP_2;
    case KEY_KP3: return MARU_KEY_KP_3;
    case KEY_KP0: return MARU_KEY_KP_0;
    case KEY_KPDOT: return MARU_KEY_KP_DECIMAL;
    case KEY_F11: return MARU_KEY_F11;
    case KEY_F12: return MARU_KEY_F12;
    case KEY_KPENTER: return MARU_KEY_KP_ENTER;
    case KEY_RIGHTCTRL: return MARU_KEY_RIGHT_CONTROL;
    case KEY_KPSLASH: return MARU_KEY_KP_DIVIDE;
    case KEY_SYSRQ: return MARU_KEY_PRINT_SCREEN;
    case KEY_RIGHTALT: return MARU_KEY_RIGHT_ALT;
    case KEY_HOME: return MARU_KEY_HOME;
    case KEY_UP: return MARU_KEY_UP;
    case KEY_PAGEUP: return MARU_KEY_PAGE_UP;
    case KEY_LEFT: return MARU_KEY_LEFT;
    case KEY_RIGHT: return MARU_KEY_RIGHT;
    case KEY_END: return MARU_KEY_END;
    case KEY_DOWN: return MARU_KEY_DOWN;
    case KEY_PAGEDOWN: return MARU_KEY_PAGE_DOWN;
    case KEY_INSERT: return MARU_KEY_INSERT;
    case KEY_DELETE: return MARU_KEY_DELETE;
    case KEY_MUTE: return MARU_KEY_UNKNOWN; // No MARU equivalent
    case KEY_KPEQUAL: return MARU_KEY_KP_EQUAL;
    case KEY_PAUSE: return MARU_KEY_PAUSE;
    case KEY_LEFTMETA: return MARU_KEY_LEFT_META;
    case KEY_RIGHTMETA: return MARU_KEY_RIGHT_META;
    case KEY_COMPOSE: return MARU_KEY_MENU;
    default: return MARU_KEY_UNKNOWN;
  }
}

MARU_Status _maru_linux_common_get_controllers(MARU_Context_Linux_Common *common,
                                               MARU_ControllerList *out_list) {
  if (common->controller_snapshot_dirty) {
    if (common->controller_count > common->controller_list_capacity) {
      uint32_t new_capacity = common->controller_count;
      if (new_capacity < 8) new_capacity = 8;
      MARU_Controller **new_list = (MARU_Controller **)maru_context_realloc(
          common->ctx_base, common->controller_list_storage,
          common->controller_list_capacity * sizeof(MARU_Controller *),
          new_capacity * sizeof(MARU_Controller *));
      if (!new_list) return MARU_FAILURE;
      common->controller_list_storage = new_list;
      common->controller_list_capacity = new_capacity;
    }

    uint32_t i = 0;
    for (MARU_LinuxController *it = common->controllers; it; it = it->next) {
      common->controller_list_storage[i++] = (MARU_Controller *)it;
    }
    common->controller_snapshot_count = common->controller_count;
    common->controller_snapshot_dirty = false;
  }

  out_list->controllers = common->controller_list_storage;
  out_list->count = common->controller_snapshot_count;
  return MARU_SUCCESS;
}

void _maru_linux_common_retain_controller(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
}

void _maru_linux_common_release_controller(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u) {
        MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->base.context;
        _maru_linux_controller_destroy(ctx_base, ctrl);
      }
      break;
    }
  }
}
