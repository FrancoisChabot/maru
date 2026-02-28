// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

@interface MARU_WindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) MARU_Window_Cocoa *window;
@end

@interface MARU_ContentView : NSView
@end

@implementation MARU_ContentView
- (BOOL)wantsUpdateLayer { return YES; }
+ (Class)layerClass { return [CAMetalLayer class]; }
- (CALayer *)makeBackingLayer { return [CAMetalLayer layer]; }
- (BOOL)acceptsFirstResponder { return YES; }

// We let the context handle the events by overriding these and doing nothing, 
// which prevents the "bleep" sound from the OS.
- (void)keyDown:(NSEvent *)event { (void)event; }
- (void)keyUp:(NSEvent *)event { (void)event; }
- (void)flagsChanged:(NSEvent *)event { (void)event; }
- (void)mouseDown:(NSEvent *)event { (void)event; }
- (void)mouseUp:(NSEvent *)event { (void)event; }
- (void)rightMouseDown:(NSEvent *)event { (void)event; }
- (void)rightMouseUp:(NSEvent *)event { (void)event; }
- (void)otherMouseDown:(NSEvent *)event { (void)event; }
- (void)otherMouseUp:(NSEvent *)event { (void)event; }
- (void)scrollWheel:(NSEvent *)event { (void)event; }
@end

static MARU_WindowGeometry NSRectToMARUGeometry(NSWindow *nsWindow, NSRect frame) {
    NSRect backing = [nsWindow convertRectToBacking:frame];
    MARU_WindowGeometry geo = {0};
    geo.pixel_size.x = (int32_t)backing.size.width;
    geo.pixel_size.y = (int32_t)backing.size.height;
    geo.origin.x = (MARU_Scalar)frame.origin.x;
    geo.origin.y = (MARU_Scalar)frame.origin.y;
    geo.logical_size.x = (MARU_Scalar)frame.size.width;
    geo.logical_size.y = (MARU_Scalar)frame.size.height;
    geo.scale = (MARU_Scalar)(backing.size.width / frame.size.width); 
    return geo;
}

@implementation MARU_WindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    MARU_Event event = {0};
    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)self.window, &event);
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
    NSWindow *nsWindow = notification.object;
    NSRect frame = [nsWindow contentRectForFrameRect:nsWindow.frame];
    
    MARU_WindowGeometry geo = NSRectToMARUGeometry(nsWindow, frame);
    [self.window->ns_layer setContentsScale:geo.scale];

    MARU_Event event = {0};
    event.resized.geometry = geo;

    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)self.window, &event);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    self.window->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
}

- (void)windowDidResignKey:(NSNotification *)notification {
    self.window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
}

@end

MARU_Status maru_createWindow_Cocoa(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window) {
    MARU_Context_Cocoa *ctx_cocoa = (MARU_Context_Cocoa *)context;
    MARU_Window_Cocoa *win = maru_context_alloc(&ctx_cocoa->base, sizeof(MARU_Window_Cocoa));
    if (!win) return MARU_FAILURE;

    memset(win, 0, sizeof(MARU_Window_Cocoa));
    win->base.ctx_base = &ctx_cocoa->base;
    win->base.pub.userdata = create_info->userdata;
    win->base.pub.context = context;
    win->base.pub.metrics = &win->base.metrics;
    win->base.pub.keyboard_state = win->base.keyboard_state;
    win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
    win->base.pub.mouse_button_state = NULL;
    win->base.pub.mouse_button_channels = NULL;
    win->base.pub.mouse_button_count = 0;
    for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
      win->base.pub.mouse_default_button_channels[i] = -1;
    }
    win->base.attrs_requested = create_info->attributes;
    win->base.attrs_effective = create_info->attributes;
    win->base.attrs_dirty_mask = 0;
    win->base.pub.event_mask = win->base.attrs_effective.event_mask;
    win->base.pub.cursor_mode = win->base.attrs_effective.cursor_mode;
    win->base.pub.current_cursor = win->base.attrs_effective.cursor;

    win->base.pub.flags = 0;

    if (win->base.attrs_effective.maximized) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    }
    if (win->base.attrs_effective.visible) {
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    }
    if (win->base.attrs_effective.minimized || !win->base.attrs_effective.visible) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
    }
    if (win->base.attrs_effective.fullscreen) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
    }
    if (win->base.attrs_effective.resizable) {
      win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
    }
    if (create_info->decorated) {
      win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
    }
    if (win->base.attrs_effective.mouse_passthrough) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
    }

    if (create_info->attributes.title) {
      size_t len = strlen(create_info->attributes.title);
      win->base.title_storage = maru_context_alloc(&ctx_cocoa->base, len + 1);
      if (win->base.title_storage) {
        memcpy(win->base.title_storage, create_info->attributes.title, len + 1);
        win->base.attrs_requested.title = win->base.title_storage;
        win->base.attrs_effective.title = win->base.title_storage;
        win->base.pub.title = win->base.title_storage;
      }
    }

#ifdef MARU_INDIRECT_BACKEND
    win->base.backend = &maru_backend_Cocoa;
#endif

    _maru_register_window(&ctx_cocoa->base, (MARU_Window *)win);

    NSRect contentRect = NSMakeRect(0, 0, create_info->attributes.logical_size.x, create_info->attributes.logical_size.y);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

    NSWindow *nsWindow = [[NSWindow alloc] initWithContentRect:contentRect
                                                     styleMask:styleMask
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
    [nsWindow setTitle:[NSString stringWithUTF8String:create_info->attributes.title]];
    [nsWindow setReleasedWhenClosed:NO];

    MARU_WindowDelegate *delegate = [[MARU_WindowDelegate alloc] init];
    delegate.window = win;
    [nsWindow setDelegate:delegate];

    MARU_ContentView *view = [[MARU_ContentView alloc] initWithFrame:contentRect];
    [view setWantsLayer:YES];
    
    win->ns_window = nsWindow;
    win->ns_view = view;
    win->ns_layer = [view layer];
    win->size = create_info->attributes.logical_size;

    [nsWindow setContentView:view];
    [nsWindow makeKeyAndOrderFront:nil];
    [nsWindow center];

    win->base.pending_ready_event = true;

    *out_window = (MARU_Window *)win;
    return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    _maru_unregister_window(win->base.ctx_base, window);
    
    NSWindow *nsWindow = win->ns_window;
    id delegate = [nsWindow delegate];
    [nsWindow setDelegate:nil];
    [delegate release];

    [nsWindow close];
    [nsWindow release];

    maru_context_free(win->base.ctx_base, win);
    return MARU_SUCCESS;
}

MARU_Status maru_getWindowGeometry_Cocoa(MARU_Window *window, MARU_WindowGeometry *out_geometry) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    NSWindow *nsWindow = win->ns_window;
    NSRect frame = [nsWindow contentRectForFrameRect:nsWindow.frame];

    *out_geometry = NSRectToMARUGeometry(nsWindow, frame);
    
    return MARU_SUCCESS;
}

MARU_Status maru_updateWindow_Cocoa(MARU_Window *window, uint64_t field_mask,
                                      const MARU_WindowAttributes *attributes) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    NSWindow *nsWindow = win->ns_window;
    MARU_WindowAttributes *requested = &win->base.attrs_requested;
    MARU_WindowAttributes *effective = &win->base.attrs_effective;

    win->base.attrs_dirty_mask |= field_mask;

    if (field_mask & MARU_WINDOW_ATTR_TITLE) {
        if (win->base.title_storage) {
            maru_context_free(win->base.ctx_base, win->base.title_storage);
            win->base.title_storage = NULL;
        }
        requested->title = NULL;
        effective->title = NULL;
        win->base.pub.title = NULL;

        if (attributes->title) {
            size_t len = strlen(attributes->title);
            win->base.title_storage = maru_context_alloc(win->base.ctx_base, len + 1);
            if (win->base.title_storage) {
                memcpy(win->base.title_storage, attributes->title, len + 1);
                requested->title = win->base.title_storage;
                effective->title = win->base.title_storage;
                win->base.pub.title = win->base.title_storage;
            }
        }
        [nsWindow setTitle:[NSString stringWithUTF8String:effective->title ? effective->title : ""]];
    }

    if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
        requested->logical_size = attributes->logical_size;
        effective->logical_size = attributes->logical_size;
        NSRect frame = [nsWindow contentRectForFrameRect:[nsWindow frame]];
        frame.size.width = effective->logical_size.x;
        frame.size.height = effective->logical_size.y;
        [nsWindow setFrame:[nsWindow frameRectForContentRect:frame] display:YES];
    }

    if (field_mask & MARU_WINDOW_ATTR_POSITION) {
        requested->position = attributes->position;
        effective->position = attributes->position;
        NSRect frame = [nsWindow frame];
        frame.origin.x = effective->position.x;
        frame.origin.y = effective->position.y;
        [nsWindow setFrame:frame display:YES];
    }

    if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
        requested->fullscreen = attributes->fullscreen;
        effective->fullscreen = attributes->fullscreen;
        if (effective->fullscreen) {
            win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
            if (!([nsWindow styleMask] & NSWindowStyleMaskFullScreen)) {
                [nsWindow toggleFullScreen:nil];
            }
        } else {
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
            if ([nsWindow styleMask] & NSWindowStyleMaskFullScreen) {
                [nsWindow toggleFullScreen:nil];
            }
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
        requested->maximized = attributes->maximized;
        if (requested->maximized) {
            if (![nsWindow isZoomed]) [nsWindow zoom:nil];
            win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
        } else {
            if ([nsWindow isZoomed]) [nsWindow zoom:nil];
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
        }
        effective->maximized = (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
    }

    if (field_mask & MARU_WINDOW_ATTR_MINIMIZED) {
        requested->minimized = attributes->minimized;
        if (requested->minimized) {
            [nsWindow miniaturize:nil];
            win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
        } else {
            [nsWindow deminiaturize:nil];
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
            win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
        }
        effective->minimized = (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
        effective->visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    }

    if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
        requested->visible = attributes->visible;
        if (requested->visible) {
            [nsWindow makeKeyAndOrderFront:nil];
            win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
        } else {
            [nsWindow orderOut:nil];
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
        }
        effective->visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
        effective->minimized = (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
    }

    if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
        requested->event_mask = attributes->event_mask;
        effective->event_mask = attributes->event_mask;
        win->base.pub.event_mask = effective->event_mask;
    }

    if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
        requested->cursor = attributes->cursor;
        effective->cursor = attributes->cursor;
        win->base.pub.current_cursor = effective->cursor;
    }

    if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
        requested->cursor_mode = attributes->cursor_mode;
        effective->cursor_mode = attributes->cursor_mode;
        win->base.pub.cursor_mode = effective->cursor_mode;
    }

    win->base.attrs_dirty_mask &= ~field_mask;
    return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    [win->ns_window makeKeyAndOrderFront:nil];
    return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_Cocoa(MARU_Window *window) {
    return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window) {
    [NSApp requestUserAttention:NSInformationalRequest];
    return MARU_SUCCESS;
}

void *_maru_getWindowNativeHandle_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    return win->ns_window;
}
