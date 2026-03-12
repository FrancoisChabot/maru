// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

@interface MARU_WindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) MARU_Window_Cocoa *window;
@end

@interface MARU_ContentView : NSView {
    NSTrackingArea *trackingArea;
}
@property (nonatomic, assign) MARU_Window_Cocoa *maruWindow;
@end

@implementation MARU_ContentView
- (BOOL)wantsUpdateLayer { return YES; }
+ (Class)layerClass { return [CAMetalLayer class]; }
- (CALayer *)makeBackingLayer { return [CAMetalLayer layer]; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)updateTrackingAreas {
    if (trackingArea) {
        [self removeTrackingArea:trackingArea];
        [trackingArea release];
    }
    NSTrackingAreaOptions options = (NSTrackingActiveInKeyWindow | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingInVisibleRect);
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:options owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}

- (void)dealloc {
    if (trackingArea) {
        [self removeTrackingArea:trackingArea];
        [trackingArea release];
    }
    [super dealloc];
}

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
- (void)mouseEntered:(NSEvent *)event { (void)event; }
- (void)mouseExited:(NSEvent *)event { (void)event; }

- (void)resetCursorRects {
    [super resetCursorRects];
    if (self.maruWindow) {
        MARU_CursorMode mode = self.maruWindow->base.pub.cursor_mode;
        if (mode == MARU_CURSOR_HIDDEN || mode == MARU_CURSOR_LOCKED) {
            NSImage *transparentImage = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
            NSCursor *invisibleCursor = [[NSCursor alloc] initWithImage:transparentImage hotSpot:NSMakePoint(0, 0)];
            [self addCursorRect:[self bounds] cursor:invisibleCursor];
            [transparentImage release];
            [invisibleCursor release];
        } else {
            MARU_Cursor_Cocoa *cur = (MARU_Cursor_Cocoa *)self.maruWindow->base.pub.current_cursor;
            if (cur && cur->ns_cursor) {
                [self addCursorRect:[self bounds] cursor:cur->ns_cursor];
            } else {
                [self addCursorRect:[self bounds] cursor:[NSCursor arrowCursor]];
            }
        }
    }
}
@end

static MARU_WindowGeometry NSRectToMARUGeometry(NSWindow *nsWindow, NSRect frame) {
    NSRect backing = [nsWindow convertRectToBacking:frame];
    MARU_WindowGeometry geo = {0};
    geo.px_size.x = (int32_t)backing.dip_size.width;
    geo.px_size.y = (int32_t)backing.dip_size.height;
    geo.dip_position.x = (MARU_Scalar)frame.dip_position.x;
    geo.dip_position.y = (MARU_Scalar)frame.dip_position.y;
    geo.dip_size.x = (MARU_Scalar)frame.dip_size.width;
    geo.dip_size.y = (MARU_Scalar)frame.dip_size.height;
    geo.scale = (MARU_Scalar)(backing.dip_size.width / frame.dip_size.width); 
    return geo;
}

static void _maru_cocoa_refresh_window_geometry(MARU_Window_Cocoa *win,
                                                MARU_WindowGeometry *out_geometry) {
    NSWindow *nsWindow = win->ns_window;
    NSRect frame = [nsWindow contentRectForFrameRect:nsWindow.frame];
    MARU_WindowGeometry geometry = NSRectToMARUGeometry(nsWindow, frame);
    win->base.pub.geometry = geometry;
    if (out_geometry) {
        *out_geometry = geometry;
    }
}

@implementation MARU_WindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    MARU_Event event = {0};
    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)self.window, &event);
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(self.window, &geo);
    [self.window->ns_layer setContentsScale:geo.scale];

    MARU_Event event = {0};
    event.window_resized.geometry = geo;

    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)self.window, &event);
}

- (void)windowDidMove:(NSNotification *)notification {
    (void)notification;
    _maru_cocoa_refresh_window_geometry(self.window, NULL);
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    (void)notification;
    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(self.window, &geo);
    [self.window->ns_layer setContentsScale:geo.scale];

    MARU_Event event = {0};
    event.window_resized.geometry = geo;
    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)self.window, &event);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    self.window->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
    MARU_Event event = {0};
    event.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_FOCUSED;
    event.window_state_changed.focused = true;
    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)self.window, &event);
}

- (void)windowDidResignKey:(NSNotification *)notification {
    self.window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
    MARU_Event event = {0};
    event.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_FOCUSED;
    event.window_state_changed.focused = false;
    _maru_dispatch_event(self.window->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)self.window, &event);
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
    win->base.attrs_requested = create_info->attributes;
    win->base.attrs_effective = create_info->attributes;
    win->base.attrs_dirty_mask = 0;
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

    if (create_info->attributes.icon) {
        MARU_Image_Cocoa *img_cocoa = (MARU_Image_Cocoa *)create_info->attributes.icon;
        if (img_cocoa->ns_image) {
            [NSApp setApplicationIconImage:img_cocoa->ns_image];
            win->base.attrs_requested.icon = create_info->attributes.icon;
            win->base.attrs_effective.icon = create_info->attributes.icon;
        }
    }

#ifdef MARU_INDIRECT_BACKEND
    win->base.backend = &maru_backend_Cocoa;
#endif

    _maru_register_window(&ctx_cocoa->base, (MARU_Window *)win);

    NSRect contentRect = NSMakeRect(0, 0, create_info->attributes.dip_size.x, create_info->attributes.dip_size.y);
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
    view.maruWindow = win;
    
    win->ns_window = nsWindow;
    win->ns_view = view;
    win->ns_layer = [view layer];
    win->dip_size = create_info->attributes.dip_size;
    _maru_cocoa_associate_window(nsWindow, win);

    [nsWindow setContentView:view];
    [view release];
    [nsWindow makeKeyAndOrderFront:nil];
    [nsWindow center];
    _maru_cocoa_refresh_window_geometry(win, NULL);

    win->base.pending_ready_event = true;

    *out_window = (MARU_Window *)win;
    return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    _maru_unregister_window(win->base.ctx_base, window);

    if (win->base.title_storage) {
        maru_context_free(win->base.ctx_base, win->base.title_storage);
        win->base.title_storage = NULL;
        win->base.attrs_requested.title = NULL;
        win->base.attrs_effective.title = NULL;
        win->base.pub.title = NULL;
    }
    
    NSWindow *nsWindow = win->ns_window;
    _maru_cocoa_associate_window(nsWindow, NULL);
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
    _maru_cocoa_refresh_window_geometry(win, out_geometry);
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

    if (field_mask & MARU_WINDOW_ATTR_DIP_SIZE) {
        requested->dip_size = attributes->dip_size;
        effective->dip_size = attributes->dip_size;
        NSRect frame = [nsWindow contentRectForFrameRect:[nsWindow frame]];
        frame.dip_size.width = effective->dip_size.x;
        frame.dip_size.height = effective->dip_size.y;
        [nsWindow setFrame:[nsWindow frameRectForContentRect:frame] display:YES];
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_POSITION) {
        requested->dip_position = attributes->dip_position;
        effective->dip_position = attributes->dip_position;
        NSRect frame = [nsWindow frame];
        frame.dip_position.x = effective->dip_position.x;
        frame.dip_position.y = effective->dip_position.y;
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

    if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
        requested->cursor = attributes->cursor;
        effective->cursor = attributes->cursor;
        win->base.pub.current_cursor = effective->cursor;
        [nsWindow invalidateCursorRectsForView:win->ns_view];
    }

    if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
        requested->cursor_mode = attributes->cursor_mode;
        effective->cursor_mode = attributes->cursor_mode;
        win->base.pub.cursor_mode = effective->cursor_mode;
        [nsWindow invalidateCursorRectsForView:win->ns_view];
    }

    if (field_mask & MARU_WINDOW_ATTR_ICON) {
        requested->icon = attributes->icon;
        effective->icon = attributes->icon;
        if (effective->icon) {
            MARU_Image_Cocoa *img_cocoa = (MARU_Image_Cocoa *)effective->icon;
            if (img_cocoa->ns_image) {
                [NSApp setApplicationIconImage:img_cocoa->ns_image];
            }
        } else {
            [NSApp setApplicationIconImage:nil];
        }
    }

    _maru_cocoa_refresh_window_geometry(win, NULL);
    win->base.attrs_dirty_mask &= ~field_mask;
    return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    [win->ns_window makeKeyAndOrderFront:nil];
    return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    if (!win || !win->base.ctx_base) {
        return MARU_FAILURE;
    }

    MARU_Event event = {0};
    const NSTimeInterval uptime = [NSProcessInfo processInfo].systemUptime;
    if (uptime > 0.0) {
        event.frame.timestamp_ms = (uint32_t)(uptime * 1000.0);
    }

    if (win->base.ctx_base->pump_ctx) {
        _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_WINDOW_FRAME, window, &event);
        return MARU_SUCCESS;
    }

    return _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_FRAME, window, &event);
}

MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window) {
    (void)window;
    [NSApp requestUserAttention:NSInformationalRequest];
    return MARU_SUCCESS;
}

void *_maru_getWindowNativeHandle_Cocoa(MARU_Window *window) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
    return win->ns_window;
}
