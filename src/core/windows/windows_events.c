// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <stdbool.h>

static const MARU_Key _maru_key_table[256] = {
    [VK_SPACE] = MARU_KEY_SPACE,
    [VK_OEM_7] = MARU_KEY_APOSTROPHE,
    [VK_OEM_COMMA] = MARU_KEY_COMMA,
    [VK_OEM_MINUS] = MARU_KEY_MINUS,
    [VK_OEM_PERIOD] = MARU_KEY_PERIOD,
    [VK_OEM_2] = MARU_KEY_SLASH,
    ['0'] = MARU_KEY_0,
    ['1'] = MARU_KEY_1,
    ['2'] = MARU_KEY_2,
    ['3'] = MARU_KEY_3,
    ['4'] = MARU_KEY_4,
    ['5'] = MARU_KEY_5,
    ['6'] = MARU_KEY_6,
    ['7'] = MARU_KEY_7,
    ['8'] = MARU_KEY_8,
    ['9'] = MARU_KEY_9,
    [VK_OEM_1] = MARU_KEY_SEMICOLON,
    [VK_OEM_PLUS] = MARU_KEY_EQUAL,
    ['A'] = MARU_KEY_A,
    ['B'] = MARU_KEY_B,
    ['C'] = MARU_KEY_C,
    ['D'] = MARU_KEY_D,
    ['E'] = MARU_KEY_E,
    ['F'] = MARU_KEY_F,
    ['G'] = MARU_KEY_G,
    ['H'] = MARU_KEY_H,
    ['I'] = MARU_KEY_I,
    ['J'] = MARU_KEY_J,
    ['K'] = MARU_KEY_K,
    ['L'] = MARU_KEY_L,
    ['M'] = MARU_KEY_M,
    ['N'] = MARU_KEY_N,
    ['O'] = MARU_KEY_O,
    ['P'] = MARU_KEY_P,
    ['Q'] = MARU_KEY_Q,
    ['R'] = MARU_KEY_R,
    ['S'] = MARU_KEY_S,
    ['T'] = MARU_KEY_T,
    ['U'] = MARU_KEY_U,
    ['V'] = MARU_KEY_V,
    ['W'] = MARU_KEY_W,
    ['X'] = MARU_KEY_X,
    ['Y'] = MARU_KEY_Y,
    ['Z'] = MARU_KEY_Z,
    [VK_OEM_4] = MARU_KEY_LEFT_BRACKET,
    [VK_OEM_5] = MARU_KEY_BACKSLASH,
    [VK_OEM_6] = MARU_KEY_RIGHT_BRACKET,
    [VK_OEM_3] = MARU_KEY_GRAVE_ACCENT,
    [VK_ESCAPE] = MARU_KEY_ESCAPE,
    [VK_RETURN] = MARU_KEY_ENTER,
    [VK_TAB] = MARU_KEY_TAB,
    [VK_BACK] = MARU_KEY_BACKSPACE,
    [VK_INSERT] = MARU_KEY_INSERT,
    [VK_DELETE] = MARU_KEY_DELETE,
    [VK_RIGHT] = MARU_KEY_RIGHT,
    [VK_LEFT] = MARU_KEY_LEFT,
    [VK_DOWN] = MARU_KEY_DOWN,
    [VK_UP] = MARU_KEY_UP,
    [VK_PRIOR] = MARU_KEY_PAGE_UP,
    [VK_NEXT] = MARU_KEY_PAGE_DOWN,
    [VK_HOME] = MARU_KEY_HOME,
    [VK_END] = MARU_KEY_END,
    [VK_CAPITAL] = MARU_KEY_CAPS_LOCK,
    [VK_SCROLL] = MARU_KEY_SCROLL_LOCK,
    [VK_NUMLOCK] = MARU_KEY_NUM_LOCK,
    [VK_SNAPSHOT] = MARU_KEY_PRINT_SCREEN,
    [VK_PAUSE] = MARU_KEY_PAUSE,
    [VK_F1] = MARU_KEY_F1,
    [VK_F2] = MARU_KEY_F2,
    [VK_F3] = MARU_KEY_F3,
    [VK_F4] = MARU_KEY_F4,
    [VK_F5] = MARU_KEY_F5,
    [VK_F6] = MARU_KEY_F6,
    [VK_F7] = MARU_KEY_F7,
    [VK_F8] = MARU_KEY_F8,
    [VK_F9] = MARU_KEY_F9,
    [VK_F10] = MARU_KEY_F10,
    [VK_F11] = MARU_KEY_F11,
    [VK_F12] = MARU_KEY_F12,
    [VK_NUMPAD0] = MARU_KEY_KP_0,
    [VK_NUMPAD1] = MARU_KEY_KP_1,
    [VK_NUMPAD2] = MARU_KEY_KP_2,
    [VK_NUMPAD3] = MARU_KEY_KP_3,
    [VK_NUMPAD4] = MARU_KEY_KP_4,
    [VK_NUMPAD5] = MARU_KEY_KP_5,
    [VK_NUMPAD6] = MARU_KEY_KP_6,
    [VK_NUMPAD7] = MARU_KEY_KP_7,
    [VK_NUMPAD8] = MARU_KEY_KP_8,
    [VK_NUMPAD9] = MARU_KEY_KP_9,
    [VK_DECIMAL] = MARU_KEY_KP_DECIMAL,
    [VK_DIVIDE] = MARU_KEY_KP_DIVIDE,
    [VK_MULTIPLY] = MARU_KEY_KP_MULTIPLY,
    [VK_SUBTRACT] = MARU_KEY_KP_SUBTRACT,
    [VK_ADD] = MARU_KEY_KP_ADD,
    [VK_LSHIFT] = MARU_KEY_LEFT_SHIFT,
    [VK_LCONTROL] = MARU_KEY_LEFT_CONTROL,
    [VK_LMENU] = MARU_KEY_LEFT_ALT,
    [VK_LWIN] = MARU_KEY_LEFT_META,
    [VK_RSHIFT] = MARU_KEY_RIGHT_SHIFT,
    [VK_RCONTROL] = MARU_KEY_RIGHT_CONTROL,
    [VK_RMENU] = MARU_KEY_RIGHT_ALT,
    [VK_RWIN] = MARU_KEY_RIGHT_META,
    [VK_APPS] = MARU_KEY_MENU,
};

MARU_Key _maru_translate_key_windows(WPARAM wParam, LPARAM lParam) {
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

uint64_t _maru_windows_get_time_ns(void) {
  static LARGE_INTEGER frequency;
  if (frequency.QuadPart == 0) {
    QueryPerformanceFrequency(&frequency);
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (uint64_t)((counter.QuadPart * 1000000000) / frequency.QuadPart);
}

uint64_t _maru_windows_get_time_ms(void) {
  return _maru_windows_get_time_ns() / 1000000;
}

static void _maru_windows_dispatch_pending_frames(MARU_Context_Windows *ctx) {
  MARU_Window_Base *base = ctx->base.window_list_head;
  const uint64_t now_ms = _maru_windows_get_time_ms();

  while (base) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)base;
    if (win->pending_frame_request) {
      win->pending_frame_request = false;

      MARU_Event evt = {0};
      evt.frame.timestamp_ms = (uint32_t)now_ms;
      _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME,
                           (MARU_Window *)win, &evt);
    }
    base = base->ctx_next;
  }
}

static bool _maru_windows_has_pending_frames(MARU_Context_Windows *ctx) {
  MARU_Window_Base *base = ctx->base.window_list_head;
  while (base) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)base;
    if (win->pending_frame_request) {
      return true;
    }
    base = base->ctx_next;
  }
  return false;
}

MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms,
                                    MARU_EventMask mask,
                                    MARU_EventCallback callback,
                                    void *userdata) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  const uint64_t pump_start_ns = _maru_windows_get_time_ns();

  if (ctx->clipboard_mime_query_storage) {
    maru_context_free(&ctx->base, ctx->clipboard_mime_query_storage);
    ctx->clipboard_mime_query_storage = NULL;
  }
  if (ctx->clipboard_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard_mime_query_ptr);
    ctx->clipboard_mime_query_ptr = NULL;
    ctx->clipboard_mime_query_count = 0;
  }

  MARU_PumpContext pump_ctx = {.mask = mask, .callback = callback, .userdata = userdata};
  ctx->base.pump_ctx = &pump_ctx;

  _maru_drain_queued_events(&ctx->base);
  _maru_windows_drain_deferred_events(ctx);

  void _maru_windows_update_controllers(MARU_Context_Windows *ctx);
  _maru_windows_update_controllers(ctx);

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

  const bool had_pending_frames = _maru_windows_has_pending_frames(ctx);

  // If we have a timeout and no frame request is already pending, wait for work.
  if (timeout_ms != 0 && !had_pending_frames) {
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

  _maru_windows_dispatch_pending_frames(ctx);

  {
    const uint64_t now_ms = _maru_windows_get_time_ms();
    _maru_advance_animated_cursors(&ctx->base, now_ms);
  }

  _maru_drain_queued_events(&ctx->base);
  _maru_windows_drain_deferred_events(ctx);
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
