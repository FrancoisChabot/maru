// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include "maru_cursor_assets.h"

static uint64_t _maru_cocoa_now_ms(void) {
    const NSTimeInterval uptime = [NSProcessInfo processInfo].systemUptime;
    if (uptime <= 0.0) {
        return 0u;
    }
    return (uint64_t)(uptime * 1000.0);
}

static void _maru_cocoa_release_animated_frames(MARU_Context_Base *ctx_base,
                                                MARU_Cursor_Cocoa *cursor) {
    if (!cursor) {
        return;
    }

    if (cursor->ns_frame_cursors) {
        for (uint32_t i = 0; i < cursor->anim_frame_count; ++i) {
            if (cursor->ns_frame_cursors[i]) {
                [cursor->ns_frame_cursors[i] release];
            }
        }
        maru_context_free(ctx_base, cursor->ns_frame_cursors);
        cursor->ns_frame_cursors = NULL;
    }
    if (cursor->ns_frame_images) {
        for (uint32_t i = 0; i < cursor->anim_frame_count; ++i) {
            if (cursor->ns_frame_images[i]) {
                [cursor->ns_frame_images[i] release];
            }
        }
        maru_context_free(ctx_base, cursor->ns_frame_images);
        cursor->ns_frame_images = NULL;
    }
    if (cursor->anim_frame_delays_ms) {
        maru_context_free(ctx_base, cursor->anim_frame_delays_ms);
        cursor->anim_frame_delays_ms = NULL;
    }
    cursor->anim_frame_count = 0u;
    cursor->ns_cursor = nil;
}

static void _maru_cocoa_select_animated_cursor_frame(MARU_Cursor_Base *cursor_base,
                                                     uint32_t frame_index) {
    MARU_Cursor_Cocoa *cursor = (MARU_Cursor_Cocoa *)cursor_base;
    if (!cursor->ns_frame_cursors || frame_index >= cursor->anim_frame_count) {
        return;
    }
    cursor->ns_cursor = cursor->ns_frame_cursors[frame_index];
}

static bool _maru_cocoa_reapply_animated_cursor(MARU_Cursor_Base *cursor_base) {
    MARU_Cursor_Cocoa *cursor = (MARU_Cursor_Cocoa *)cursor_base;
    MARU_Context_Base *ctx_base = cursor_base->ctx_base;
    bool applied = false;
    for (MARU_Window_Base *it = ctx_base->window_list_head; it; it = it->ctx_next) {
        MARU_Window_Cocoa *window = (MARU_Window_Cocoa *)it;
        if (window->base.pub.current_cursor != (MARU_Cursor *)cursor ||
            window->base.pub.cursor_mode != MARU_CURSOR_NORMAL || !window->ns_window ||
            !window->ns_view) {
            continue;
        }
        NSWindow *ns_window = (NSWindow *)window->ns_window;
        NSView *ns_view = (NSView *)window->ns_view;
        [ns_window invalidateCursorRectsForView:ns_view];
        if ([ns_window isKeyWindow] && cursor->ns_cursor) {
            [(NSCursor *)cursor->ns_cursor set];
        }
        applied = true;
    }
    return applied;
}

static const MARU_CursorAnimationCallbacks g_maru_cocoa_cursor_animation_callbacks = {
    .select_frame = _maru_cocoa_select_animated_cursor_frame,
    .reapply = _maru_cocoa_reapply_animated_cursor,
    .on_reapplied = NULL,
};

static NSCursor *_maru_cocoa_get_system_cursor(MARU_CursorShape shape) {
    switch (shape) {
        case MARU_CURSOR_SHAPE_DEFAULT: return [NSCursor arrowCursor];
        case MARU_CURSOR_SHAPE_TEXT: return [NSCursor IBeamCursor];
        case MARU_CURSOR_SHAPE_CROSSHAIR: return [NSCursor crosshairCursor];
        case MARU_CURSOR_SHAPE_HAND: return [NSCursor pointingHandCursor];
        case MARU_CURSOR_SHAPE_MOVE: return [NSCursor openHandCursor];
        case MARU_CURSOR_SHAPE_NOT_ALLOWED: return [NSCursor operationNotAllowedCursor];
        case MARU_CURSOR_SHAPE_EW_RESIZE: return [NSCursor resizeLeftRightCursor];
        case MARU_CURSOR_SHAPE_NS_RESIZE: return [NSCursor resizeUpDownCursor];
        case MARU_CURSOR_SHAPE_HELP:
        case MARU_CURSOR_SHAPE_WAIT:
        case MARU_CURSOR_SHAPE_NESW_RESIZE:
        case MARU_CURSOR_SHAPE_NWSE_RESIZE:
        default:
            return nil;
    }
}

static bool _maru_cocoa_create_builtin_cursor(MARU_CursorShape shape,
                                              NSCursor **out_cursor,
                                              NSImage **out_image) {
    if (!out_cursor || !out_image) {
        return false;
    }
    *out_cursor = nil;
    *out_image = nil;

    if (shape > MARU_CURSOR_SHAPE_NWSE_RESIZE) {
        return false;
    }
    const MARU_CursorBitmapAsset *bitmap = &g_maru_cursor_bitmap_assets[shape];
    if (!bitmap->pixels_argb || bitmap->width == 0u || bitmap->height == 0u ||
        bitmap->stride_bytes == 0u) {
        return false;
    }

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    if (!color_space) {
        return false;
    }

    const size_t image_size =
        (size_t)bitmap->stride_bytes * (size_t)bitmap->height;
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, bitmap->pixels_argb, image_size, NULL);
    if (!provider) {
        CGColorSpaceRelease(color_space);
        return false;
    }

    CGImageRef cg_image = CGImageCreate(
        (size_t)bitmap->width, (size_t)bitmap->height,
        8, 32, (size_t)bitmap->stride_bytes,
        color_space,
        kCGBitmapByteOrder32Little | kCGImageAlphaFirst,
        provider, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
    if (!cg_image) {
        return false;
    }

    NSImage *image = [[NSImage alloc] initWithCGImage:cg_image
                                                 size:NSMakeSize((CGFloat)bitmap->width,
                                                                 (CGFloat)bitmap->height)];
    CGImageRelease(cg_image);
    if (!image) {
        return false;
    }

    NSCursor *cursor =
        [[NSCursor alloc] initWithImage:image
                                hotSpot:NSMakePoint((CGFloat)bitmap->hot_x,
                                                    (CGFloat)bitmap->hot_y)];
    if (!cursor) {
        [image release];
        return false;
    }

    *out_cursor = cursor;
    *out_image = image;
    return true;
}

static bool _maru_cocoa_resolve_system_cursor(MARU_Cursor_Cocoa *cursor,
                                              MARU_CursorShape shape) {
    if (!cursor || !cursor->base.ctx_base) {
        return false;
    }

    const MARU_CursorPolicy policy = cursor->base.ctx_base->tuning.cursor_policy;
    if (policy != MARU_CURSOR_POLICY_MARU_ONLY) {
        NSCursor *system_cursor = _maru_cocoa_get_system_cursor(shape);
        if (system_cursor) {
            cursor->ns_cursor = [system_cursor retain];
            return true;
        }
    }

    if (policy != MARU_CURSOR_POLICY_SYSTEM_ONLY) {
        if (_maru_cocoa_create_builtin_cursor(shape, (NSCursor **)&cursor->ns_cursor,
                                              (NSImage **)&cursor->ns_image)) {
            return true;
        }
        if (shape != MARU_CURSOR_SHAPE_DEFAULT &&
            _maru_cocoa_create_builtin_cursor(MARU_CURSOR_SHAPE_DEFAULT,
                                              (NSCursor **)&cursor->ns_cursor,
                                              (NSImage **)&cursor->ns_image)) {
            return true;
        }
    }

    cursor->ns_cursor = [[NSCursor arrowCursor] retain];
    return cursor->ns_cursor != nil;
}

MARU_Status maru_createCursor_Cocoa(MARU_Context *context,
                                      const MARU_CursorCreateInfo *create_info,
                                      MARU_Cursor **out_cursor) {
    if (!context || !create_info || !out_cursor) return MARU_FAILURE;

    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_Cursor_Cocoa *cursor = maru_context_alloc(ctx_base, sizeof(MARU_Cursor_Cocoa));
    if (!cursor) return MARU_FAILURE;

    memset(cursor, 0, sizeof(MARU_Cursor_Cocoa));
    cursor->base.ctx_base = ctx_base;
    cursor->base.pub.metrics = &cursor->base.metrics;
    cursor->base.pub.userdata = create_info->userdata;

#ifdef MARU_INDIRECT_BACKEND
    cursor->base.backend = &maru_backend_Cocoa;
#endif

    if (create_info->source == MARU_CURSOR_SOURCE_SYSTEM) {
        if (!_maru_cocoa_resolve_system_cursor(cursor, create_info->system_shape)) {
            maru_context_free(ctx_base, cursor);
            return MARU_FAILURE;
        }
        cursor->base.pub.flags |= MARU_CURSOR_FLAG_SYSTEM;
    } else {
        if (!create_info->frames || create_info->frame_count == 0) {
            maru_context_free(ctx_base, cursor);
            return MARU_FAILURE;
        }
        if (create_info->frame_count > 1) {
            const uint32_t frame_count = create_info->frame_count;
            cursor->ns_frame_cursors = maru_context_alloc(ctx_base, sizeof(id) * (size_t)frame_count);
            cursor->ns_frame_images = maru_context_alloc(ctx_base, sizeof(id) * (size_t)frame_count);
            cursor->anim_frame_delays_ms = maru_context_alloc(
                ctx_base, sizeof(uint32_t) * (size_t)frame_count);
            if (!cursor->ns_frame_cursors || !cursor->ns_frame_images ||
                !cursor->anim_frame_delays_ms) {
                _maru_cocoa_release_animated_frames(ctx_base, cursor);
                maru_context_free(ctx_base, cursor);
                return MARU_FAILURE;
            }
            memset(cursor->ns_frame_cursors, 0, sizeof(id) * (size_t)frame_count);
            memset(cursor->ns_frame_images, 0, sizeof(id) * (size_t)frame_count);

            cursor->anim_frame_count = frame_count;
            for (uint32_t i = 0; i < frame_count; ++i) {
                const MARU_CursorFrame *frame = &create_info->frames[i];
                MARU_Image_Cocoa *img = (MARU_Image_Cocoa *)frame->image;
                if (!img || !img->ns_image || img->base.ctx_base != ctx_base) {
                    _maru_cocoa_release_animated_frames(ctx_base, cursor);
                    maru_context_free(ctx_base, cursor);
                    return MARU_FAILURE;
                }
                cursor->ns_frame_images[i] = [img->ns_image retain];
                NSPoint hotspot = NSMakePoint(frame->hot_spot.x, frame->hot_spot.y);
                cursor->ns_frame_cursors[i] =
                    [[NSCursor alloc] initWithImage:img->ns_image hotSpot:hotspot];
                if (!cursor->ns_frame_cursors[i]) {
                    _maru_cocoa_release_animated_frames(ctx_base, cursor);
                    maru_context_free(ctx_base, cursor);
                    return MARU_FAILURE;
                }
                cursor->anim_frame_delays_ms[i] =
                    _maru_cursor_frame_delay_ms(frame->delay_ms);
            }

            cursor->ns_cursor = cursor->ns_frame_cursors[0];
            const uint64_t now_ms = _maru_cocoa_now_ms();
            if (!_maru_register_animated_cursor(&cursor->base, frame_count,
                                                cursor->anim_frame_delays_ms,
                                                &g_maru_cocoa_cursor_animation_callbacks,
                                                now_ms)) {
                _maru_cocoa_release_animated_frames(ctx_base, cursor);
                maru_context_free(ctx_base, cursor);
                return MARU_FAILURE;
            }
        } else {
            MARU_Image_Cocoa *img = (MARU_Image_Cocoa *)create_info->frames[0].image;
            if (!img || !img->ns_image || img->base.ctx_base != ctx_base) {
                maru_context_free(ctx_base, cursor);
                return MARU_FAILURE;
            }
            NSPoint hotspot =
                NSMakePoint(create_info->frames[0].hot_spot.x, create_info->frames[0].hot_spot.y);
            cursor->ns_image = [img->ns_image retain];
            cursor->ns_cursor = [[NSCursor alloc] initWithImage:img->ns_image hotSpot:hotspot];
        }
    }

    if (!cursor->ns_cursor) {
        _maru_cocoa_release_animated_frames(ctx_base, cursor);
        if (cursor->ns_image) {
            [cursor->ns_image release];
        }
        maru_context_free(ctx_base, cursor);
        return MARU_FAILURE;
    }

    *out_cursor = (MARU_Cursor *)cursor;
    return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_Cocoa(MARU_Cursor *cursor) {
    if (!cursor) return MARU_FAILURE;
    MARU_Cursor_Cocoa *cur = (MARU_Cursor_Cocoa *)cursor;
    if (cur->base.anim_enabled) {
        _maru_unregister_animated_cursor(&cur->base);
    }

    if (cur->anim_frame_count > 1u) {
        _maru_cocoa_release_animated_frames(cur->base.ctx_base, cur);
    } else if (cur->ns_cursor) {
        [cur->ns_cursor release];
    }
    if (cur->ns_image) {
        [cur->ns_image release];
    }
    maru_context_free(cur->base.ctx_base, cur);
    return MARU_SUCCESS;
}

MARU_Status maru_resetCursorMetrics_Cocoa(MARU_Cursor *cursor) {
    if (!cursor) return MARU_FAILURE;
    MARU_Cursor_Cocoa *cur = (MARU_Cursor_Cocoa *)cursor;
    memset(&cur->base.metrics, 0, sizeof(MARU_CursorMetrics));
    return MARU_SUCCESS;
}
