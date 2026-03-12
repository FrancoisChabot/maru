// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

static HCURSOR _maru_create_custom_cursor_windows(const MARU_Image_Windows *img, int hot_x, int hot_y) {
  return (HCURSOR)_maru_windows_create_hicon_from_image((const MARU_Image *)img, FALSE, hot_x, hot_y);
}

static void _maru_windows_select_animated_cursor_frame(MARU_Cursor_Base *cursor_base, uint32_t frame_index) {
  MARU_Cursor_Windows *cursor = (MARU_Cursor_Windows *)cursor_base;
  if (!cursor->hcursors || frame_index >= cursor->anim_frame_count) {
    return;
  }
  cursor->hcursor = cursor->hcursors[frame_index];
}

static bool _maru_windows_reapply_animated_cursor(MARU_Cursor_Base *cursor_base) {
  MARU_Cursor_Windows *cursor = (MARU_Cursor_Windows *)cursor_base;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)cursor->base.ctx_base;
  bool applied = false;

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)it;
    if (win->base.pub.current_cursor != (const MARU_Cursor *)cursor ||
        win->base.pub.cursor_mode != MARU_CURSOR_NORMAL || !win->hwnd) {
      continue;
    }

    // Trigger WM_SETCURSOR
    PostMessageW(win->hwnd, WM_SETCURSOR, (WPARAM)win->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    applied = true;
  }
  return applied;
}

static const MARU_CursorAnimationCallbacks g_maru_windows_cursor_animation_callbacks = {
    .select_frame = _maru_windows_select_animated_cursor_frame,
    .reapply = _maru_windows_reapply_animated_cursor,
    .on_reapplied = NULL,
};

MARU_Status maru_createCursor_Windows(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  HCURSOR hcursor = NULL;
  HCURSOR *hcursors = NULL;
  uint32_t *delays = NULL;
  bool is_system = false;

  if (create_info->source == MARU_CURSOR_SOURCE_SYSTEM) {
    LPCWSTR idc_name = IDC_ARROW;
    switch (create_info->system_shape) {
      case MARU_CURSOR_SHAPE_DEFAULT: idc_name = IDC_ARROW; break;
      case MARU_CURSOR_SHAPE_HELP: idc_name = IDC_HELP; break;
      case MARU_CURSOR_SHAPE_HAND: idc_name = IDC_HAND; break;
      case MARU_CURSOR_SHAPE_WAIT: idc_name = IDC_WAIT; break;
      case MARU_CURSOR_SHAPE_CROSSHAIR: idc_name = IDC_CROSS; break;
      case MARU_CURSOR_SHAPE_TEXT: idc_name = IDC_IBEAM; break;
      case MARU_CURSOR_SHAPE_MOVE: idc_name = IDC_SIZEALL; break;
      case MARU_CURSOR_SHAPE_NOT_ALLOWED: idc_name = IDC_NO; break;
      case MARU_CURSOR_SHAPE_EW_RESIZE: idc_name = IDC_SIZEWE; break;
      case MARU_CURSOR_SHAPE_NS_RESIZE: idc_name = IDC_SIZENS; break;
      case MARU_CURSOR_SHAPE_NESW_RESIZE: idc_name = IDC_SIZENESW; break;
      case MARU_CURSOR_SHAPE_NWSE_RESIZE: idc_name = IDC_SIZENWSE; break;
      default: break;
    }
    hcursor = LoadCursorW(NULL, idc_name);
    is_system = true;
  } else if (create_info->source == MARU_CURSOR_SOURCE_CUSTOM) {
    if (create_info->frame_count == 0 || !create_info->frames) {
      return MARU_FAILURE;
    }

    if (create_info->frame_count > 1) {
      hcursors = (HCURSOR *)maru_context_alloc(&ctx->base, sizeof(HCURSOR) * create_info->frame_count);
      delays = (uint32_t *)maru_context_alloc(&ctx->base, sizeof(uint32_t) * create_info->frame_count);
      if (!hcursors || !delays) {
        if (hcursors) maru_context_free(&ctx->base, hcursors);
        if (delays) maru_context_free(&ctx->base, delays);
        return MARU_FAILURE;
      }

      for (uint32_t i = 0; i < create_info->frame_count; ++i) {
        const MARU_Image_Windows *img = (const MARU_Image_Windows *)create_info->frames[i].image;
        hcursors[i] = _maru_create_custom_cursor_windows(img, (int)create_info->frames[i].px_hot_spot.x, (int)create_info->frames[i].px_hot_spot.y);
        delays[i] = _maru_cursor_frame_delay_ms(create_info->frames[i].delay_ms);
        if (!hcursors[i]) {
          for (uint32_t j = 0; j < i; ++j) DestroyCursor(hcursors[j]);
          maru_context_free(&ctx->base, hcursors);
          maru_context_free(&ctx->base, delays);
          return MARU_FAILURE;
        }
      }
      hcursor = hcursors[0];
    } else {
      const MARU_Image_Windows *img = (const MARU_Image_Windows *)create_info->frames[0].image;
      hcursor = _maru_create_custom_cursor_windows(img, (int)create_info->frames[0].px_hot_spot.x, (int)create_info->frames[0].px_hot_spot.y);
    }
  }

  if (!hcursor) {
    return MARU_FAILURE;
  }

  MARU_Cursor_Windows *cur = (MARU_Cursor_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_Windows));
  if (!cur) {
    if (!is_system) {
      if (hcursors) {
        for (uint32_t i = 0; i < create_info->frame_count; ++i) DestroyCursor(hcursors[i]);
        maru_context_free(&ctx->base, hcursors);
        maru_context_free(&ctx->base, delays);
      } else {
        DestroyCursor(hcursor);
      }
    }
    return MARU_FAILURE;
  }
  memset(cur, 0, sizeof(MARU_Cursor_Windows));
  cur->base.ctx_base = &ctx->base;
  cur->hcursor = hcursor;
  cur->is_system = is_system;
  cur->base.pub.userdata = create_info->userdata;
  cur->base.pub.metrics = &cur->base.metrics;

  if (hcursors) {
    cur->hcursors = hcursors;
    cur->anim_frame_delays_ms = delays;
    cur->anim_frame_count = create_info->frame_count;
    
    uint64_t now = _maru_windows_get_time_ms();
    if (!_maru_register_animated_cursor(&cur->base, cur->anim_frame_count, cur->anim_frame_delays_ms, &g_maru_windows_cursor_animation_callbacks, now)) {
      for (uint32_t i = 0; i < create_info->frame_count; ++i) DestroyCursor(hcursors[i]);
      maru_context_free(&ctx->base, hcursors);
      maru_context_free(&ctx->base, delays);
      maru_context_free(&ctx->base, cur);
      return MARU_FAILURE;
    }
  }

  *out_cursor = (MARU_Cursor *)cur;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_Windows(MARU_Cursor *cursor) {
  MARU_Cursor_Windows *cur = (MARU_Cursor_Windows *)cursor;
  if (!cur) return MARU_SUCCESS;

  MARU_Context_Windows *ctx = (MARU_Context_Windows *)cur->base.ctx_base;
  
  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
    MARU_Window_Windows *win = (MARU_Window_Windows *)it;
    if (win->base.pub.current_cursor != cursor) {
      continue;
    }
    win->base.pub.current_cursor = NULL;
    win->base.attrs_requested.cursor = NULL;
    win->base.attrs_effective.cursor = NULL;
    
    if (win->base.pub.cursor_mode == MARU_CURSOR_NORMAL && win->is_cursor_in_client_area) {
        PostMessageW(win->hwnd, WM_SETCURSOR, (WPARAM)win->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }
  }

  if (cur->base.anim_enabled) {
    _maru_unregister_animated_cursor(&cur->base);
  }

  if (!cur->is_system) {
    if (cur->hcursors) {
      for (uint32_t i = 0; i < cur->anim_frame_count; ++i) {
        if (cur->hcursors[i]) DestroyCursor(cur->hcursors[i]);
      }
      maru_context_free(&ctx->base, cur->hcursors);
      maru_context_free(&ctx->base, cur->anim_frame_delays_ms);
    } else if (cur->hcursor) {
      DestroyCursor(cur->hcursor);
    }
  }
  maru_context_free(&ctx->base, cur);
  return MARU_SUCCESS;
}

MARU_Status maru_resetCursorMetrics_Windows(MARU_Cursor *cursor) {
  MARU_Cursor_Windows *cur = (MARU_Cursor_Windows *)cursor;
  if (!cur) return MARU_FAILURE;
  memset(&cur->base.metrics, 0, sizeof(cur->base.metrics));
  return MARU_SUCCESS;
}
