// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include <stdbool.h>

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
