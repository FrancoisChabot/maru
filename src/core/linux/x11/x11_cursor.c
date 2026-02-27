// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <string.h>
#include <X11/cursorfont.h>

void _maru_x11_clear_locked_raw_accum(MARU_Context_X11 *ctx) {
  ctx->locked_raw_dx_accum = (MARU_Scalar)0.0;
  ctx->locked_raw_dy_accum = (MARU_Scalar)0.0;
  ctx->locked_raw_pending = false;
}

bool _maru_x11_ensure_hidden_cursor(MARU_Context_X11 *ctx) {
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

void _maru_x11_recenter_locked_pointer(MARU_Context_X11 *ctx,
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

bool _maru_x11_advance_animated_cursors(MARU_Context_X11 *ctx,
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

static uint32_t _maru_x11_cursor_frame_delay_ms(uint32_t delay_ms) {
  return (delay_ms == 0) ? 1u : delay_ms;
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

MARU_Status maru_resetCursorMetrics_X11(MARU_Cursor *cursor) {
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
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
