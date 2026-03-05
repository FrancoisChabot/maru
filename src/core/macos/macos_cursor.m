// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>

MARU_Status maru_createCursor_Cocoa(MARU_Context *context,
                                      const MARU_CursorCreateInfo *create_info,
                                      MARU_Cursor **out_cursor) {
    if (!context || !create_info || !out_cursor) return MARU_FAILURE;

    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_Cursor_Cocoa *cursor = maru_context_alloc(ctx_base, sizeof(MARU_Cursor_Cocoa));
    if (!cursor) return MARU_FAILURE;

    memset(cursor, 0, sizeof(MARU_Cursor_Cocoa));
    cursor->base.ctx_base = ctx_base;

#ifdef MARU_INDIRECT_BACKEND
    cursor->base.backend = &maru_backend_Cocoa;
#endif

    if (create_info->source == MARU_CURSOR_SOURCE_SYSTEM) {
        NSCursor *nsCursor = nil;
        switch (create_info->system_shape) {
            case MARU_CURSOR_SHAPE_DEFAULT: nsCursor = [NSCursor arrowCursor]; break;
            case MARU_CURSOR_SHAPE_TEXT: nsCursor = [NSCursor IBeamCursor]; break;
            case MARU_CURSOR_SHAPE_CROSSHAIR: nsCursor = [NSCursor crosshairCursor]; break;
            case MARU_CURSOR_SHAPE_HAND: nsCursor = [NSCursor pointingHandCursor]; break;
            case MARU_CURSOR_SHAPE_EW_RESIZE: nsCursor = [NSCursor resizeLeftRightCursor]; break;
            case MARU_CURSOR_SHAPE_NS_RESIZE: nsCursor = [NSCursor resizeUpDownCursor]; break;
            // macOS doesn't have built-in diagonal resize or move cursors, we fallback or use private ones (if safe).
            // For now, mapping to safe public ones or arrow.
            case MARU_CURSOR_SHAPE_NESW_RESIZE: nsCursor = [NSCursor crosshairCursor]; break;
            case MARU_CURSOR_SHAPE_NWSE_RESIZE: nsCursor = [NSCursor crosshairCursor]; break;
            case MARU_CURSOR_SHAPE_MOVE: nsCursor = [NSCursor arrowCursor]; break; // fallback
            case MARU_CURSOR_SHAPE_NOT_ALLOWED: nsCursor = [NSCursor operationNotAllowedCursor]; break;
            default: nsCursor = [NSCursor arrowCursor]; break;
        }
        cursor->ns_cursor = [nsCursor retain];
        cursor->base.pub.flags |= MARU_CURSOR_FLAG_SYSTEM;
    } else {
        if (!create_info->frames || create_info->frame_count == 0) {
            maru_context_free(ctx_base, cursor);
            return MARU_FAILURE;
        }
        MARU_Image_Cocoa *img = (MARU_Image_Cocoa *)create_info->frames[0].image;
        if (!img || !img->ns_image) {
            maru_context_free(ctx_base, cursor);
            return MARU_FAILURE;
        }
        NSPoint hotspot = NSMakePoint(create_info->frames[0].hot_spot.x, create_info->frames[0].hot_spot.y);
        cursor->ns_image = [img->ns_image retain];
        cursor->ns_cursor = [[NSCursor alloc] initWithImage:img->ns_image hotSpot:hotspot];
    }

    if (!cursor->ns_cursor) {
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
    if (cur->ns_cursor) {
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
