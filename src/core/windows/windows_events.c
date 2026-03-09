// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include <stdbool.h>

static MARU_Key _maru_key_table[256];

static void _maru_init_key_table(void) {
  memset(_maru_key_table, 0, sizeof(_maru_key_table));
  _maru_key_table[VK_SPACE] = MARU_KEY_SPACE;
  _maru_key_table[VK_OEM_7] = MARU_KEY_APOSTROPHE;
  _maru_key_table[VK_OEM_COMMA] = MARU_KEY_COMMA;
  _maru_key_table[VK_OEM_MINUS] = MARU_KEY_MINUS;
  _maru_key_table[VK_OEM_PERIOD] = MARU_KEY_PERIOD;
  _maru_key_table[VK_OEM_2] = MARU_KEY_SLASH;
  _maru_key_table['0'] = MARU_KEY_0;
  _maru_key_table['1'] = MARU_KEY_1;
  _maru_key_table['2'] = MARU_KEY_2;
  _maru_key_table['3'] = MARU_KEY_3;
  _maru_key_table['4'] = MARU_KEY_4;
  _maru_key_table['5'] = MARU_KEY_5;
  _maru_key_table['6'] = MARU_KEY_6;
  _maru_key_table['7'] = MARU_KEY_7;
  _maru_key_table['8'] = MARU_KEY_8;
  _maru_key_table['9'] = MARU_KEY_9;
  _maru_key_table[VK_OEM_1] = MARU_KEY_SEMICOLON;
  _maru_key_table[VK_OEM_PLUS] = MARU_KEY_EQUAL;
  _maru_key_table['A'] = MARU_KEY_A;
  _maru_key_table['B'] = MARU_KEY_B;
  _maru_key_table['C'] = MARU_KEY_C;
  _maru_key_table['D'] = MARU_KEY_D;
  _maru_key_table['E'] = MARU_KEY_E;
  _maru_key_table['F'] = MARU_KEY_F;
  _maru_key_table['G'] = MARU_KEY_G;
  _maru_key_table['H'] = MARU_KEY_H;
  _maru_key_table['I'] = MARU_KEY_I;
  _maru_key_table['J'] = MARU_KEY_J;
  _maru_key_table['K'] = MARU_KEY_K;
  _maru_key_table['L'] = MARU_KEY_L;
  _maru_key_table['M'] = MARU_KEY_M;
  _maru_key_table['N'] = MARU_KEY_N;
  _maru_key_table['O'] = MARU_KEY_O;
  _maru_key_table['P'] = MARU_KEY_P;
  _maru_key_table['Q'] = MARU_KEY_Q;
  _maru_key_table['R'] = MARU_KEY_R;
  _maru_key_table['S'] = MARU_KEY_S;
  _maru_key_table['T'] = MARU_KEY_T;
  _maru_key_table['U'] = MARU_KEY_U;
  _maru_key_table['V'] = MARU_KEY_V;
  _maru_key_table['W'] = MARU_KEY_W;
  _maru_key_table['X'] = MARU_KEY_X;
  _maru_key_table['Y'] = MARU_KEY_Y;
  _maru_key_table['Z'] = MARU_KEY_Z;
  _maru_key_table[VK_OEM_4] = MARU_KEY_LEFT_BRACKET;
  _maru_key_table[VK_OEM_5] = MARU_KEY_BACKSLASH;
  _maru_key_table[VK_OEM_6] = MARU_KEY_RIGHT_BRACKET;
  _maru_key_table[VK_OEM_3] = MARU_KEY_GRAVE_ACCENT;
  _maru_key_table[VK_ESCAPE] = MARU_KEY_ESCAPE;
  _maru_key_table[VK_RETURN] = MARU_KEY_ENTER;
  _maru_key_table[VK_TAB] = MARU_KEY_TAB;
  _maru_key_table[VK_BACK] = MARU_KEY_BACKSPACE;
  _maru_key_table[VK_INSERT] = MARU_KEY_INSERT;
  _maru_key_table[VK_DELETE] = MARU_KEY_DELETE;
  _maru_key_table[VK_RIGHT] = MARU_KEY_RIGHT;
  _maru_key_table[VK_LEFT] = MARU_KEY_LEFT;
  _maru_key_table[VK_DOWN] = MARU_KEY_DOWN;
  _maru_key_table[VK_UP] = MARU_KEY_UP;
  _maru_key_table[VK_PRIOR] = MARU_KEY_PAGE_UP;
  _maru_key_table[VK_NEXT] = MARU_KEY_PAGE_DOWN;
  _maru_key_table[VK_HOME] = MARU_KEY_HOME;
  _maru_key_table[VK_END] = MARU_KEY_END;
  _maru_key_table[VK_CAPITAL] = MARU_KEY_CAPS_LOCK;
  _maru_key_table[VK_SCROLL] = MARU_KEY_SCROLL_LOCK;
  _maru_key_table[VK_NUMLOCK] = MARU_KEY_NUM_LOCK;
  _maru_key_table[VK_SNAPSHOT] = MARU_KEY_PRINT_SCREEN;
  _maru_key_table[VK_PAUSE] = MARU_KEY_PAUSE;
  _maru_key_table[VK_F1] = MARU_KEY_F1;
  _maru_key_table[VK_F2] = MARU_KEY_F2;
  _maru_key_table[VK_F3] = MARU_KEY_F3;
  _maru_key_table[VK_F4] = MARU_KEY_F4;
  _maru_key_table[VK_F5] = MARU_KEY_F5;
  _maru_key_table[VK_F6] = MARU_KEY_F6;
  _maru_key_table[VK_F7] = MARU_KEY_F7;
  _maru_key_table[VK_F8] = MARU_KEY_F8;
  _maru_key_table[VK_F9] = MARU_KEY_F9;
  _maru_key_table[VK_F10] = MARU_KEY_F10;
  _maru_key_table[VK_F11] = MARU_KEY_F11;
  _maru_key_table[VK_F12] = MARU_KEY_F12;
  _maru_key_table[VK_NUMPAD0] = MARU_KEY_KP_0;
  _maru_key_table[VK_NUMPAD1] = MARU_KEY_KP_1;
  _maru_key_table[VK_NUMPAD2] = MARU_KEY_KP_2;
  _maru_key_table[VK_NUMPAD3] = MARU_KEY_KP_3;
  _maru_key_table[VK_NUMPAD4] = MARU_KEY_KP_4;
  _maru_key_table[VK_NUMPAD5] = MARU_KEY_KP_5;
  _maru_key_table[VK_NUMPAD6] = MARU_KEY_KP_6;
  _maru_key_table[VK_NUMPAD7] = MARU_KEY_KP_7;
  _maru_key_table[VK_NUMPAD8] = MARU_KEY_KP_8;
  _maru_key_table[VK_NUMPAD9] = MARU_KEY_KP_9;
  _maru_key_table[VK_DECIMAL] = MARU_KEY_KP_DECIMAL;
  _maru_key_table[VK_DIVIDE] = MARU_KEY_KP_DIVIDE;
  _maru_key_table[VK_MULTIPLY] = MARU_KEY_KP_MULTIPLY;
  _maru_key_table[VK_SUBTRACT] = MARU_KEY_KP_SUBTRACT;
  _maru_key_table[VK_ADD] = MARU_KEY_KP_ADD;
  // VK_RETURN + Extended bit logic needed for KP_ENTER
  _maru_key_table[VK_LSHIFT] = MARU_KEY_LEFT_SHIFT;
  _maru_key_table[VK_LCONTROL] = MARU_KEY_LEFT_CONTROL;
  _maru_key_table[VK_LMENU] = MARU_KEY_LEFT_ALT;
  _maru_key_table[VK_LWIN] = MARU_KEY_LEFT_META;
  _maru_key_table[VK_RSHIFT] = MARU_KEY_RIGHT_SHIFT;
  _maru_key_table[VK_RCONTROL] = MARU_KEY_RIGHT_CONTROL;
  _maru_key_table[VK_RMENU] = MARU_KEY_RIGHT_ALT;
  _maru_key_table[VK_RWIN] = MARU_KEY_RIGHT_META;
  _maru_key_table[VK_APPS] = MARU_KEY_MENU;
}

MARU_Key _maru_translate_key_windows(WPARAM wParam, LPARAM lParam) {
  static bool table_initialized = false;
  if (!table_initialized) {
    _maru_init_key_table();
    table_initialized = true;
  }

  if (wParam == VK_RETURN) {
    if ((lParam >> 24) & 1) return MARU_KEY_KP_ENTER;
    return MARU_KEY_ENTER;
  }

  if (wParam == VK_SHIFT) {
    // We can use MapVirtualKey to get the specific side
    UINT scancode = (lParam & 0x00ff0000) >> 16;
    return _maru_translate_key_windows(MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX), lParam);
  }

  if (wParam == VK_CONTROL) {
      if ((lParam >> 24) & 1) return MARU_KEY_RIGHT_CONTROL;
      return MARU_KEY_LEFT_CONTROL;
  }

  if (wParam == VK_MENU) {
      if ((lParam >> 24) & 1) return MARU_KEY_RIGHT_ALT;
      return MARU_KEY_LEFT_ALT;
  }

  if (wParam < 256) {
    return _maru_key_table[wParam];
  }

  return MARU_KEY_UNKNOWN;
}

MARU_ModifierFlags _maru_get_modifiers_windows(void) {
  MARU_ModifierFlags modifiers = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MARU_MODIFIER_SHIFT;
  if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MARU_MODIFIER_CONTROL;
  if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MARU_MODIFIER_ALT;
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) modifiers |= MARU_MODIFIER_META;
  if (GetKeyState(VK_CAPITAL) & 1) modifiers |= MARU_MODIFIER_CAPS_LOCK;
  if (GetKeyState(VK_NUMLOCK) & 1) modifiers |= MARU_MODIFIER_NUM_LOCK;
  return modifiers;
}

static uint64_t _maru_windows_get_time_ns(void) {
  static LARGE_INTEGER frequency;
  if (frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (uint64_t)((counter.QuadPart * 1000000000) / frequency.QuadPart);
}

static uint64_t _maru_windows_get_time_ms(void) {
  return _maru_windows_get_time_ns() / 1000000;
}

MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms,
                                    MARU_EventCallback callback,
                                    void *userdata) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  const uint64_t pump_start_ns = _maru_windows_get_time_ns();

  MARU_PumpContext pump_ctx = {.callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  _maru_drain_queued_events(&ctx->base);

  {
    const uint64_t now_ms = _maru_windows_get_time_ms();
    _maru_advance_animated_cursors(&ctx->base, now_ms);
  }

  // Initial pump of all available messages
  MSG msg;
  while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  // Handle first-time show for windows to avoid white flash
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)it;
    if (win->show_on_first_pump) {
      ShowWindow(win->hwnd, SW_SHOW);
      UpdateWindow(win->hwnd);
      win->show_on_first_pump = false;
    }
  }

  // If we have a timeout and no messages were processed, or we just want to wait
  if (timeout_ms != 0) {
    uint64_t now_ms = _maru_windows_get_time_ms();
    uint32_t remaining_timeout = _maru_adjust_timeout_for_cursor_animation(&ctx->base, timeout_ms, now_ms);

    if (remaining_timeout > 0 || timeout_ms == MARU_NEVER) {
      DWORD dw_timeout = (remaining_timeout == MARU_NEVER) ? INFINITE : (DWORD)remaining_timeout;
      
      // Wait for either a message, or the wake event
      MsgWaitForMultipleObjectsEx(1, &ctx->wake_event, dw_timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

      // Pump any new messages
      while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
  }

  {
    const uint64_t now_ms = _maru_windows_get_time_ms();
    _maru_advance_animated_cursors(&ctx->base, now_ms);
  }

  _maru_drain_queued_events(&ctx->base);
  ctx->base.pump_ctx = NULL;

  const uint64_t pump_end_ns = _maru_windows_get_time_ns();
  if (pump_end_ns >= pump_start_ns) {
    uint64_t duration = pump_end_ns - pump_start_ns;
    ctx->base.metrics.pump_call_count_total++;
    if (duration > ctx->base.metrics.pump_duration_peak_ns) {
      ctx->base.metrics.pump_duration_peak_ns = duration;
    }
    // Simple moving average or similar could go here, but for now let's just update peak
  }

  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  if (ctx->wake_event) {
    SetEvent(ctx->wake_event);
  }
  return MARU_SUCCESS;
}
