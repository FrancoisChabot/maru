// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#define _GNU_SOURCE
#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru_cursor_assets.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

const char *_maru_cursor_shape_to_name(MARU_CursorShape shape) {
    switch (shape) {
        case MARU_CURSOR_SHAPE_DEFAULT: return "left_ptr";
        case MARU_CURSOR_SHAPE_HELP: return "question_arrow";
        case MARU_CURSOR_SHAPE_HAND: return "hand1";
        case MARU_CURSOR_SHAPE_WAIT: return "watch";
        case MARU_CURSOR_SHAPE_CROSSHAIR: return "crosshair";
        case MARU_CURSOR_SHAPE_TEXT: return "xterm";
        case MARU_CURSOR_SHAPE_MOVE: return "move";
        case MARU_CURSOR_SHAPE_NOT_ALLOWED: return "forbidden";
        case MARU_CURSOR_SHAPE_EW_RESIZE: return "sb_h_double_arrow";
        case MARU_CURSOR_SHAPE_NS_RESIZE: return "sb_v_double_arrow";
        case MARU_CURSOR_SHAPE_NESW_RESIZE: return "fd_double_arrow";
        case MARU_CURSOR_SHAPE_NWSE_RESIZE: return "bd_double_arrow";
        default: return "left_ptr";
    }
}

bool _maru_wayland_ensure_cursor_theme(MARU_Context_WL *ctx) {
    if (ctx->wl.cursor_theme) {
        return true;
    }
    if (!ctx->protocols.wl_shm) {
        return false;
    }
    const char *size_env = getenv("XCURSOR_SIZE");
    int size = 24;
    if (size_env) size = atoi(size_env);
    if (size <= 0) size = 24;

    const char *theme_env = getenv("XCURSOR_THEME");
    ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, theme_env, size, ctx->protocols.wl_shm);
    return ctx->wl.cursor_theme != NULL;
}

void _maru_wayland_clear_cursor_animation(MARU_Context_WL *ctx) {
    memset(&ctx->cursor_animation, 0, sizeof(ctx->cursor_animation));
}

static bool _maru_wayland_create_owned_cursor_frame_from_pixels(
    MARU_Context_WL *ctx, const void *pixels, int32_t width, int32_t height,
    int32_t hotspot_x, int32_t hotspot_y, uint32_t delay_ms,
    MARU_WaylandCursorFrame *out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->shm_fd = -1;

  if (width <= 0 || height <= 0 || !pixels || !ctx->protocols.wl_shm) {
    return false;
  }

  const int32_t stride = width * 4;
  const size_t data_size = (size_t)stride * (size_t)height;
  int fd = memfd_create("maru-cursor", MFD_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  if (ftruncate(fd, (off_t)data_size) != 0) {
    close(fd);
    return false;
  }

  void *data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    close(fd);
    return false;
  }
  memcpy(data, pixels, data_size);

  struct wl_shm_pool *pool =
      maru_wl_shm_create_pool(ctx, ctx->protocols.wl_shm, fd, (int32_t)data_size);
  if (!pool) {
    munmap(data, data_size);
    close(fd);
    return false;
  }

  struct wl_buffer *buffer = maru_wl_shm_pool_create_buffer(
      ctx, pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
  maru_wl_shm_pool_destroy(ctx, pool);
  if (!buffer) {
    munmap(data, data_size);
    close(fd);
    return false;
  }

  out_frame->buffer = buffer;
  out_frame->shm_data = data;
  out_frame->shm_size = data_size;
  out_frame->shm_fd = fd;
  out_frame->hotspot_x = hotspot_x;
  out_frame->hotspot_y = hotspot_y;
  out_frame->width = (uint32_t)width;
  out_frame->height = (uint32_t)height;
  out_frame->delay_ms = (delay_ms == 0) ? 1u : delay_ms;
  out_frame->owns_buffer = true;
  return true;
}

static bool _maru_wayland_create_owned_cursor_frame(MARU_Context_WL *ctx,
                                                    const MARU_Image_Base *image,
                                                    MARU_Vec2Px hot_spot,
                                                    uint32_t delay_ms,
                                                    MARU_WaylandCursorFrame *out_frame) {
  return _maru_wayland_create_owned_cursor_frame_from_pixels(
      ctx, image->pixels, (int32_t)image->width, (int32_t)image->height,
      hot_spot.x, hot_spot.y, delay_ms, out_frame);
}

static void _maru_wayland_destroy_cursor_frames(MARU_Context_WL *ctx, MARU_Cursor_WL *cursor) {
  if (!cursor || !cursor->frames) {
    return;
  }
  for (uint32_t i = 0; i < cursor->frame_count; ++i) {
    MARU_WaylandCursorFrame *frame = &cursor->frames[i];
    if (frame->owns_buffer && frame->buffer) {
      maru_wl_buffer_destroy(ctx, frame->buffer);
      frame->buffer = NULL;
    }
    if (frame->shm_data && frame->shm_size > 0) {
      munmap(frame->shm_data, frame->shm_size);
      frame->shm_data = NULL;
      frame->shm_size = 0;
    }
    if (frame->shm_fd >= 0) {
      close(frame->shm_fd);
      frame->shm_fd = -1;
    }
  }
  maru_context_free(&ctx->base, cursor->frames);
  cursor->frames = NULL;
  cursor->frame_count = 0;
}

static bool _maru_wayland_resolve_system_cursor(MARU_Context_WL *ctx,
                                                MARU_Cursor_WL *cursor,
                                                MARU_CursorShape shape) {
  const MARU_CursorPolicy policy = ctx->base.tuning.cursor_policy;

  if (policy != MARU_CURSOR_POLICY_MARU_ONLY) {
    if (_maru_wayland_ensure_cursor_theme(ctx)) {
      cursor->wl_cursor = maru_wl_cursor_theme_get_cursor(
          ctx, ctx->wl.cursor_theme, _maru_cursor_shape_to_name(shape));
      if (cursor->wl_cursor) {
        return true;
      }
    }
    if (ctx->protocols.opt.wp_cursor_shape_manager_v1 != NULL) {
      // If we have the protocol, we don't strictly need a wl_cursor here,
      // wayland_input.c will handle it.
      return true;
    }
  }

  if (policy != MARU_CURSOR_POLICY_SYSTEM_ONLY) {
    const MARU_CursorBitmapAsset *bitmap = &g_maru_cursor_bitmap_assets[shape];
    if (bitmap->pixels_argb) {
      cursor->frames = (MARU_WaylandCursorFrame *)maru_context_alloc(
          &ctx->base, sizeof(MARU_WaylandCursorFrame));
      if (cursor->frames) {
        if (_maru_wayland_create_owned_cursor_frame_from_pixels(
                ctx, bitmap->pixels_argb, (int32_t)bitmap->width, (int32_t)bitmap->height,
                bitmap->hot_x, bitmap->hot_y, 0, &cursor->frames[0])) {
          cursor->frame_count = 1;
          return true;
        }
        maru_context_free(&ctx->base, cursor->frames);
        cursor->frames = NULL;
      }
    }

    if (shape != MARU_CURSOR_SHAPE_DEFAULT) {
      return _maru_wayland_resolve_system_cursor(ctx, cursor, MARU_CURSOR_SHAPE_DEFAULT);
    }
  }

  return false;
}

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  const bool system_cursor = (create_info->source == MARU_CURSOR_SOURCE_SYSTEM);

  MARU_Cursor_WL *cursor = maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) return MARU_FAILURE;
  memset(cursor, 0, sizeof(MARU_Cursor_WL));

  cursor->base.ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  cursor->base.backend = &maru_backend_WL;
#endif
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.userdata = create_info->userdata;

  if (system_cursor) {
    if (create_info->system_shape > MARU_CURSOR_SHAPE_NWSE_RESIZE) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    if (!_maru_wayland_resolve_system_cursor(ctx, cursor, create_info->system_shape)) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    cursor->base.pub.flags = MARU_CURSOR_FLAG_SYSTEM;
    cursor->cursor_shape = create_info->system_shape;
  } else {
    cursor->frames = (MARU_WaylandCursorFrame *)maru_context_alloc(
        &ctx->base, sizeof(MARU_WaylandCursorFrame) * create_info->frame_count);
    if (!cursor->frames) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    memset(cursor->frames, 0, sizeof(MARU_WaylandCursorFrame) * create_info->frame_count);
    for (uint32_t i = 0; i < create_info->frame_count; ++i) {
      const MARU_CursorFrame *frame = &create_info->frames[i];
      const MARU_Image_Base *image = (const MARU_Image_Base *)frame->image;
      if (image->ctx_base != &ctx->base) {
        _maru_wayland_destroy_cursor_frames(ctx, cursor);
        maru_context_free(&ctx->base, cursor);
        return MARU_FAILURE;
      }
      if (!_maru_wayland_create_owned_cursor_frame(ctx, image, frame->hot_spot,
                                                   frame->delay_ms, &cursor->frames[i])) {
        _maru_wayland_destroy_cursor_frames(ctx, cursor);
        maru_context_free(&ctx->base, cursor);
        return MARU_FAILURE;
      }
    }
    cursor->frame_count = create_info->frame_count;
  }

  ctx->base.metrics.cursor_create_count_total++;
  if (system_cursor) ctx->base.metrics.cursor_create_count_system++;
  else ctx->base.metrics.cursor_create_count_custom++;
  ctx->base.metrics.cursor_alive_current++;
  if (ctx->base.metrics.cursor_alive_current > ctx->base.metrics.cursor_alive_peak) {
    ctx->base.metrics.cursor_alive_peak = ctx->base.metrics.cursor_alive_current;
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor) {
  MARU_Cursor_WL *cursor_wl = (MARU_Cursor_WL *)cursor;
  MARU_Context_WL *ctx = (MARU_Context_WL *)cursor_wl->base.ctx_base;

  if (ctx->cursor_animation.active && ctx->cursor_animation.cursor == cursor_wl) {
    _maru_wayland_clear_cursor_animation(ctx);
  }

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
      MARU_Window_WL *window = (MARU_Window_WL *)it;
      if (window->base.pub.current_cursor != cursor) {
          continue;
      }

      window->base.pub.current_cursor = NULL;
      window->base.attrs_requested.cursor = NULL;
      window->base.attrs_effective.cursor = NULL;

      if (ctx->linux_common.pointer.focused_window == (MARU_Window *)window) {
          _maru_wayland_update_cursor(ctx, window, ctx->linux_common.pointer.enter_serial);
      }
  }

  _maru_wayland_destroy_cursor_frames(ctx, cursor_wl);

  ctx->base.metrics.cursor_destroy_count_total++;
  if (ctx->base.metrics.cursor_alive_current > 0) {
    ctx->base.metrics.cursor_alive_current--;
  }

  maru_context_free(&ctx->base, cursor);
  return MARU_SUCCESS;
}
