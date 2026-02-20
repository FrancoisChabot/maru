// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "macos_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static id _maru_cocoa_create_window(const MARU_WindowCreateInfo *create_info) {
  Class ns_window_class = objc_getClass("NSWindow");
  if (!ns_window_class) {
    return nil;
  }

  CGSize default_size = { .width = 800, .height = 600 };
  if (create_info->attributes.logical_size.x > 0 && create_info->attributes.logical_size.y > 0) {
    default_size.width = (CGFloat)create_info->attributes.logical_size.x;
    default_size.height = (CGFloat)create_info->attributes.logical_size.y;
  }

  NSRect frame = { .origin = { .x = 0, .y = 0 }, .size = default_size };
  
  NSWindowStyleMask style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  
  SEL alloc_sel = sel_getUid("alloc");
  id ns_window = ((id (*)(id, SEL))objc_msgSend)(ns_window_class, alloc_sel);
  
  if (!ns_window) {
    return nil;
  }
  
  SEL init_sel = sel_getUid("initWithContentRect:styleMask:backing:defer:");
  typedef id (*NSWindowInitType)(id, SEL, NSRect, NSWindowStyleMask, NSBackingStoreType, BOOL);
  ns_window = ((NSWindowInitType)objc_msgSend)(ns_window, init_sel, frame, style_mask, NSBackingStoreBuffered, NO);
  
  if (create_info->attributes.title) {
    NSString *title = [[NSString alloc] initWithUTF8String:create_info->attributes.title];
    if (title) {
      SEL set_title_sel = sel_getUid("setTitle:");
      ((void (*)(id, SEL, id))objc_msgSend)(ns_window, set_title_sel, title);
      [title release];
    }
  }
  
  return ns_window;
}

static id _maru_cocoa_create_view(void) {
  Class ns_view_class = objc_getClass("NSView");
  if (!ns_view_class) {
    return nil;
  }
  
  SEL alloc_sel = sel_getUid("alloc");
  id ns_view = ((id (*)(id, SEL))objc_msgSend)(ns_view_class, alloc_sel);
  
  if (!ns_view) {
    return nil;
  }
  
  NSRect frame = { .origin = { .x = 0, .y = 0 }, .size = { .width = 800, .height = 600 } };
  SEL init_sel = sel_getUid("initWithFrame:");
  typedef id (*NSViewInitType)(id, SEL, NSRect);
  ns_view = ((NSViewInitType)objc_msgSend)(ns_view, init_sel, frame);
  
  return ns_view;
}

MARU_Status maru_createWindow_Cocoa(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window) {
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;

  MARU_Window_Cocoa *window = (MARU_Window_Cocoa *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_Cocoa));
  if (!window) {
    return MARU_FAILURE;
  }
  memset(((uint8_t*)window) + sizeof(MARU_Window_Base), 0, sizeof(MARU_Window_Cocoa) - sizeof(MARU_Window_Base));

  window->base.ctx_base = &ctx->base;
  window->base.pub.context = (MARU_Context *)ctx;
  window->base.pub.userdata = create_info->userdata;
#ifdef MARU_INDIRECT_BACKEND
  window->base.backend = ctx->base.backend;
#endif

  window->size = create_info->attributes.logical_size;
  if (window->size.x <= 0) window->size.x = 800;
  if (window->size.y <= 0) window->size.y = 600;

  window->ns_window = _maru_cocoa_create_window(create_info);
  if (!window->ns_window) {
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  window->ns_view = _maru_cocoa_create_view();
  if (!window->ns_view) {
    SEL release_sel = sel_getUid("release");
    ((void (*)(id, SEL))objc_msgSend)(window->ns_window, release_sel);
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  SEL content_view_sel = sel_getUid("setContentView:");
  ((void (*)(id, SEL, id))objc_msgSend)(window->ns_window, content_view_sel, window->ns_view);

  SEL make_key_sel = sel_getUid("makeKeyAndOrderFront:");
  ((void (*)(id, SEL, id))objc_msgSend)(window->ns_window, make_key_sel, nil);

  window->base.pub.flags = MARU_WINDOW_STATE_READY;

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window_handle) {
  MARU_Window_Cocoa *window = (MARU_Window_Cocoa *)window_handle;
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)window->base.ctx_base;

  if (window->ns_view) {
    SEL release_sel = sel_getUid("release");
    ((void (*)(id, SEL))objc_msgSend)(window->ns_view, release_sel);
  }

  if (window->ns_window) {
    SEL release_sel = sel_getUid("release");
    ((void (*)(id, SEL))objc_msgSend)(window->ns_window, release_sel);
  }

  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

void maru_getWindowGeometry_Cocoa(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_Cocoa *window = (const MARU_Window_Cocoa *)window_handle;

  NSRect frame = { .origin = { .x = 0, .y = 0 }, .size = { .width = 0, .height = 0 } };
  
  if (window->ns_window) {
    SEL frame_sel = sel_getUid("frame");
    frame = ((NSRect (*)(id, SEL))objc_msgSend)(window->ns_window, frame_sel);
  }

  *out_geometry = (MARU_WindowGeometry){
      .origin = { (MARU_Scalar)frame.origin.x, (MARU_Scalar)frame.origin.y },
      .logical_size = { (MARU_Scalar)frame.size.width, (MARU_Scalar)frame.size.height },
      .pixel_size =
          {
              .x = (int32_t)frame.size.width,
              .y = (int32_t)frame.size.height,
          },
  };
}
