// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreVideo/CoreVideo.h>
#include <math.h>

static CVReturn _maru_cocoa_display_link_callback(CVDisplayLinkRef displayLink,
                                                  const CVTimeStamp *inNow,
                                                  const CVTimeStamp *inOutputTime,
                                                  CVOptionFlags flagsIn,
                                                  CVOptionFlags *flagsOut,
                                                  void *displayLinkContext) {
    (void)displayLink; (void)inNow; (void)inOutputTime; (void)flagsIn; (void)flagsOut;
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)displayLinkContext;
    atomic_store(&win->pending_frame_vblank, true);
    maru_wakeContext_Cocoa((MARU_Context *)win->base.pub.context);
    return kCVReturnSuccess;
}

@interface MARU_WindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) MARU_Window_Cocoa *window;
@end

@interface MARU_ContentView : NSView <NSTextInputClient, NSDraggingDestination, NSDraggingSource> {
    NSTrackingArea *trackingArea;
}
@property (nonatomic, assign) MARU_Window_Cocoa *maruWindow;
@end

@implementation MARU_ContentView
- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    (void)session;
    (void)context;
    return NSDragOperationCopy | NSDragOperationMove | NSDragOperationGeneric | NSDragOperationLink;
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation {
    (void)session;
    (void)screenPoint;
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return;

    MARU_Event evt = {0};
    
    switch (operation) {
        case NSDragOperationCopy: evt.drag_finished.action = MARU_DROP_ACTION_COPY; break;
        case NSDragOperationMove: evt.drag_finished.action = MARU_DROP_ACTION_MOVE; break;
        case NSDragOperationLink: evt.drag_finished.action = MARU_DROP_ACTION_LINK; break;
        default: evt.drag_finished.action = MARU_DROP_ACTION_NONE; break;
    }

    _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DRAG_FINISHED, (MARU_Window *)win, &evt);
}

- (BOOL)wantsUpdateLayer { return YES; }
+ (Class)layerClass { return [CAMetalLayer class]; }
- (CALayer *)makeBackingLayer { return [CAMetalLayer layer]; }
- (BOOL)acceptsFirstResponder { return YES; }

// --- NSDraggingDestination implementation ---

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return NSDragOperationNone;

    NSPasteboard *pb = [sender draggingPasteboard];
    NSArray *types = [pb types];
    
    for (uint32_t i = 0; i < win->drop_available_mime_type_count; ++i) {
        maru_context_free(win->base.ctx_base, win->drop_available_mime_types[i]);
    }
    maru_context_free(win->base.ctx_base, win->drop_available_mime_types);
    
    win->drop_available_mime_type_count = (uint32_t)[types count];
    win->drop_available_mime_types = maru_context_alloc(win->base.ctx_base, sizeof(char *) * win->drop_available_mime_type_count);
    for (uint32_t i = 0; i < win->drop_available_mime_type_count; ++i) {
        const char *mime = _maru_ns_type_to_mime([types objectAtIndex:i]);
        size_t len = strlen(mime);
        win->drop_available_mime_types[i] = maru_context_alloc(win->base.ctx_base, len + 1);
        memcpy(win->drop_available_mime_types[i], mime, len + 1);
    }

    MARU_DropSessionPrefix session_exposed = {
        .action = &win->current_drop_action,
        .session_userdata = &win->drop_session_userdata
    };

    MARU_Event evt = {0};
    evt.drop_entered.session = (MARU_DropSession *)&session_exposed;
    evt.drop_entered.available_actions = MARU_DROP_ACTION_COPY | MARU_DROP_ACTION_MOVE | MARU_DROP_ACTION_LINK;
    evt.drop_entered.available_types.count = win->drop_available_mime_type_count;
    evt.drop_entered.available_types.strings = (const char *const *)win->drop_available_mime_types;
    
    NSPoint pt = [self convertPoint:[sender draggingLocation] fromView:nil];
    evt.drop_entered.dip_position.x = (MARU_Scalar)pt.x;
    evt.drop_entered.dip_position.y = (MARU_Scalar)(self.bounds.size.height - pt.y);

    win->current_dragging_info = sender;
    win->current_drop_action = MARU_DROP_ACTION_NONE;
    _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DROP_ENTERED, (MARU_Window *)win, &evt);
    win->current_dragging_info = nil;
    
    switch (win->current_drop_action) {
        case MARU_DROP_ACTION_COPY: return NSDragOperationCopy;
        case MARU_DROP_ACTION_MOVE: return NSDragOperationMove;
        case MARU_DROP_ACTION_LINK: return NSDragOperationLink;
        default: return NSDragOperationNone;
    }
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return NSDragOperationNone;

    MARU_DropSessionPrefix session_exposed = {
        .action = &win->current_drop_action,
        .session_userdata = &win->drop_session_userdata
    };

    MARU_Event evt = {0};
    evt.drop_hovered.session = (MARU_DropSession *)&session_exposed;
    evt.drop_hovered.available_actions = MARU_DROP_ACTION_COPY | MARU_DROP_ACTION_MOVE | MARU_DROP_ACTION_LINK;
    evt.drop_hovered.available_types.count = win->drop_available_mime_type_count;
    evt.drop_hovered.available_types.strings = (const char *const *)win->drop_available_mime_types;
    NSPoint pt = [self convertPoint:[sender draggingLocation] fromView:nil];
    evt.drop_hovered.dip_position.x = (MARU_Scalar)pt.x;
    evt.drop_hovered.dip_position.y = (MARU_Scalar)(self.bounds.size.height - pt.y);

    win->current_dragging_info = sender;
    _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DROP_HOVERED, (MARU_Window *)win, &evt);
    win->current_dragging_info = nil;

    switch (win->current_drop_action) {
        case MARU_DROP_ACTION_COPY: return NSDragOperationCopy;
        case MARU_DROP_ACTION_MOVE: return NSDragOperationMove;
        case MARU_DROP_ACTION_LINK: return NSDragOperationLink;
        default: return NSDragOperationNone;
    }
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return;

    MARU_DropSessionPrefix session_exposed = {
        .action = &win->current_drop_action,
        .session_userdata = &win->drop_session_userdata
    };

    MARU_Event evt = {0};
    evt.drop_exited.session = (MARU_DropSession *)&session_exposed;
    win->current_dragging_info = sender;
    _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DROP_EXITED, (MARU_Window *)win, &evt);
    win->current_dragging_info = nil;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return NO;

    MARU_DropSessionPrefix session_exposed = {
        .action = &win->current_drop_action,
        .session_userdata = &win->drop_session_userdata
    };

    MARU_Event evt = {0};
    evt.drop_dropped.session = (MARU_DropSession *)&session_exposed;
    evt.drop_dropped.available_actions = MARU_DROP_ACTION_COPY | MARU_DROP_ACTION_MOVE | MARU_DROP_ACTION_LINK;
    evt.drop_dropped.available_types.count = win->drop_available_mime_type_count;
    evt.drop_dropped.available_types.strings = (const char *const *)win->drop_available_mime_types;
    
    NSPoint pt = [self convertPoint:[sender draggingLocation] fromView:nil];
    evt.drop_dropped.dip_position.x = (MARU_Scalar)pt.x;
    evt.drop_dropped.dip_position.y = (MARU_Scalar)(self.bounds.size.height - pt.y);

    win->current_dragging_info = sender;
    _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DROP_DROPPED, (MARU_Window *)win, &evt);
    win->current_dragging_info = nil;
    
    return win->current_drop_action != MARU_DROP_ACTION_NONE;
}

// --- End of NSDraggingDestination ---

// --- NSTextInputClient implementation ---

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return;
    
    if (win->ime_preedit_active) {
        win->ime_preedit_active = false;
        MARU_Event end_evt = {0};
        end_evt.text_edit_ended.session_id = win->text_input_session_id;
        end_evt.text_edit_ended.canceled = false;
        _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_ENDED, (MARU_Window *)win, &end_evt);
    }
    
    MARU_Event evt = {0};
    evt.text_edit_committed.session_id = win->text_input_session_id;
    evt.text_edit_committed.committed_utf8 = [text UTF8String];
    evt.text_edit_committed.committed_length_bytes = (uint32_t)strlen(evt.text_edit_committed.committed_utf8);
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_COMMITTED, (MARU_Window *)win, &evt);
}

- (void)doCommandBySelector:(SEL)selector {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win || win->base.attrs_effective.text_input_type == MARU_TEXT_INPUT_TYPE_NONE) return;

    MARU_Event evt = {0};
    evt.text_edit_navigation.session_id = win->text_input_session_id;
    bool handled = true;
    bool extend = false;

    if (selector == @selector(moveLeft:) || selector == @selector(moveBackward:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LEFT;
    } else if (selector == @selector(moveRight:) || selector == @selector(moveForward:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_RIGHT;
    } else if (selector == @selector(moveUp:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_UP;
    } else if (selector == @selector(moveDown:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOWN;
    } else if (selector == @selector(moveLeftAndModifySelection:) || selector == @selector(moveBackwardAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LEFT;
        extend = true;
    } else if (selector == @selector(moveRightAndModifySelection:) || selector == @selector(moveForwardAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_RIGHT;
        extend = true;
    } else if (selector == @selector(moveUpAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_UP;
        extend = true;
    } else if (selector == @selector(moveDownAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOWN;
        extend = true;
    } else if (selector == @selector(moveWordLeft:) || selector == @selector(moveWordBackward:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_WORD_LEFT;
    } else if (selector == @selector(moveWordRight:) || selector == @selector(moveWordForward:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_WORD_RIGHT;
    } else if (selector == @selector(moveWordLeftAndModifySelection:) || selector == @selector(moveWordBackwardAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_WORD_LEFT;
        extend = true;
    } else if (selector == @selector(moveWordRightAndModifySelection:) || selector == @selector(moveWordForwardAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_WORD_RIGHT;
        extend = true;
    } else if (selector == @selector(moveToBeginningOfLine:) || selector == @selector(moveToLeftEndOfLine:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LINE_START;
    } else if (selector == @selector(moveToEndOfLine:) || selector == @selector(moveToRightEndOfLine:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LINE_END;
    } else if (selector == @selector(moveToBeginningOfLineAndModifySelection:) || selector == @selector(moveToLeftEndOfLineAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LINE_START;
        extend = true;
    } else if (selector == @selector(moveToEndOfLineAndModifySelection:) || selector == @selector(moveToRightEndOfLineAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_LINE_END;
        extend = true;
    } else if (selector == @selector(moveToBeginningOfDocument:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_START;
    } else if (selector == @selector(moveToEndOfDocument:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_END;
    } else if (selector == @selector(moveToBeginningOfDocumentAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_START;
        extend = true;
    } else if (selector == @selector(moveToEndOfDocumentAndModifySelection:)) {
        evt.text_edit_navigation.command = MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_END;
        extend = true;
    } else {
        handled = false;
    }

    if (handled) {
        evt.text_edit_navigation.extend_selection = extend;
        NSEvent *currentEvent = [NSApp currentEvent];
        if (currentEvent && (currentEvent.type == NSEventTypeKeyDown)) {
            evt.text_edit_navigation.is_repeat = [currentEvent isARepeat];
            evt.text_edit_navigation.modifiers = _maru_cocoa_translate_modifiers(currentEvent.modifierFlags);
        }
        _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_NAVIGATION, (MARU_Window *)win, &evt);
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return;
    
    if (!win->ime_preedit_active) {
        win->ime_preedit_active = true;
        MARU_Event start_evt = {0};
        start_evt.text_edit_started.session_id = win->text_input_session_id;
        _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_STARTED, (MARU_Window *)win, &start_evt);
    }
    
    MARU_Event evt = {0};
    evt.text_edit_updated.session_id = win->text_input_session_id;
    evt.text_edit_updated.preedit_utf8 = [text UTF8String];
    evt.text_edit_updated.preedit_length_bytes = (uint32_t)strlen(evt.text_edit_updated.preedit_utf8);
    evt.text_edit_updated.caret.start_byte = (uint32_t)selectedRange.location;
    evt.text_edit_updated.caret.length_bytes = 0;
    evt.text_edit_updated.selection.start_byte = (uint32_t)selectedRange.location;
    evt.text_edit_updated.selection.length_bytes = (uint32_t)selectedRange.length;
    
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_UPDATED, (MARU_Window *)win, &evt);
}

- (void)unmarkText {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win || !win->ime_preedit_active) return;
    
    win->ime_preedit_active = false;
    MARU_Event evt = {0};
    evt.text_edit_ended.session_id = win->text_input_session_id;
    evt.text_edit_ended.canceled = false;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_ENDED, (MARU_Window *)win, &evt);
}

static NSUInteger _maru_cocoa_byte_offset_to_utf16_offset(const char *utf8, uint32_t byte_offset) {
    if (!utf8 || byte_offset == 0) return 0;
    NSString *s = [[NSString alloc] initWithBytes:utf8 length:byte_offset encoding:NSUTF8StringEncoding];
    NSUInteger res = [s length];
    [s release];
    return res;
}

- (NSRange)selectedRange {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win || !win->base.attrs_effective.surrounding_text) return NSMakeRange(NSNotFound, 0);

    NSUInteger cursor = _maru_cocoa_byte_offset_to_utf16_offset(win->base.attrs_effective.surrounding_text, win->base.attrs_effective.surrounding_cursor_byte);
    NSUInteger anchor = _maru_cocoa_byte_offset_to_utf16_offset(win->base.attrs_effective.surrounding_text, win->base.attrs_effective.surrounding_anchor_byte);

    if (cursor < anchor) return NSMakeRange(cursor, anchor - cursor);
    return NSMakeRange(anchor, cursor - anchor);
}

- (NSRange)markedRange { return NSMakeRange(NSNotFound, 0); }
- (BOOL)hasMarkedText { return self.maruWindow ? self.maruWindow->ime_preedit_active : NO; }

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win || !win->base.attrs_effective.surrounding_text) return nil;

    NSString *s = [NSString stringWithUTF8String:win->base.attrs_effective.surrounding_text];
    if (!s) return nil;

    if (range.location >= [s length]) return nil;
    if (range.location + range.length > [s length]) {
        range.length = [s length] - range.location;
    }
    
    if (actualRange) *actualRange = range;
    
    return [[[NSAttributedString alloc] initWithString:[s substringWithRange:range]] autorelease];
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    (void)actualRange;
    MARU_Window_Cocoa *win = self.maruWindow;
    if (!win) return NSZeroRect;
    
    MARU_RectDip rect = win->base.attrs_effective.dip_text_input_rect;
    NSRect contentRect = [win->ns_window contentRectForFrameRect:[win->ns_window frame]];
    
    // Convert DIP to Cocoa coordinates (flip Y)
    NSRect r = NSMakeRect(rect.position.x, contentRect.size.height - rect.position.y - rect.size.y, rect.size.x, rect.size.y);
    return [win->ns_window convertRectToScreen:[self convertRect:r toView:nil]];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return 0; }

// --- End of NSTextInputClient ---

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
- (void)keyDown:(NSEvent *)event {
    if (self.maruWindow && self.maruWindow->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE) {
        [self interpretKeyEvents:@[event]];
    }
}
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

static MARU_WindowGeometry NSRectToMARUGeometry(NSWindow *nsWindow, NSRect contentRect) {
    NSRect frame = [nsWindow frame];
    NSRect backing = [nsWindow convertRectToBacking:contentRect];
    MARU_WindowGeometry geo = {0};
    geo.px_size.x = (int32_t)backing.size.width;
    geo.px_size.y = (int32_t)backing.size.height;
    geo.dip_position.x = (MARU_Scalar)frame.origin.x;
    geo.dip_position.y = (MARU_Scalar)frame.origin.y;
    geo.dip_size.x = (MARU_Scalar)contentRect.size.width;
    geo.dip_size.y = (MARU_Scalar)contentRect.size.height;
    geo.dip_viewport_size.x = (MARU_Scalar)contentRect.size.width;
    geo.dip_viewport_size.y = (MARU_Scalar)contentRect.size.height;
    geo.scale = (MARU_Scalar)(backing.size.width / contentRect.size.width); 
    return geo;
}

static void _maru_cocoa_update_drop_registration(MARU_Window_Cocoa *win) {
    if (win->base.attrs_effective.accept_drop) {
        [win->ns_window registerForDraggedTypes:@[
            NSPasteboardTypeString,
            NSPasteboardTypeFileURL,
            NSPasteboardTypeHTML,
            NSPasteboardTypePNG,
            NSPasteboardTypeTIFF,
            (NSPasteboardType)@"public.data"
        ]];
    } else {
        [win->ns_window unregisterDraggedTypes];
    }
}

static void _maru_cocoa_refresh_window_geometry(MARU_Window_Cocoa *win,
                                                MARU_WindowGeometry *out_geometry) {
    NSWindow *nsWindow = win->ns_window;
    NSRect contentRect = [nsWindow contentRectForFrameRect:nsWindow.frame];
    MARU_WindowGeometry geometry = NSRectToMARUGeometry(nsWindow, contentRect);
    win->base.pub.geometry = geometry;
    [win->ns_layer setContentsScale:geometry.scale];
    if (out_geometry) {
        *out_geometry = geometry;
    }
}

@implementation MARU_WindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender {
    (void)sender;
    MARU_Event event = {0};
    _maru_post_event_internal(self.window->base.ctx_base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)self.window, &event);
    return NO;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    MARU_Window_Cocoa *win = self.window;
    if (!win) {
        return frameSize;
    }

    const MARU_Fraction aspect = win->base.attrs_effective.aspect_ratio;
    if (aspect.num <= 0 || aspect.denom <= 0) {
        return frameSize;
    }

    const NSRect currentFrame = [sender frame];
    const NSRect currentContent = [sender contentRectForFrameRect:currentFrame];
    const CGFloat frameDeltaW = currentFrame.size.width - currentContent.size.width;
    const CGFloat frameDeltaH = currentFrame.size.height - currentContent.size.height;
    CGFloat proposedContentW = frameSize.width - frameDeltaW;
    CGFloat proposedContentH = frameSize.height - frameDeltaH;
    if (proposedContentW < 0.0) proposedContentW = 0.0;
    if (proposedContentH < 0.0) proposedContentH = 0.0;

    const CGFloat currentContentW = currentContent.size.width;
    const CGFloat currentContentH = currentContent.size.height;
    const CGFloat deltaW = fabs(proposedContentW - currentContentW);
    const CGFloat deltaH = fabs(proposedContentH - currentContentH);
    const CGFloat targetRatio = (CGFloat)aspect.num / (CGFloat)aspect.denom;

    if (deltaW >= deltaH) {
        proposedContentH = proposedContentW / targetRatio;
    } else {
        proposedContentW = proposedContentH * targetRatio;
    }

    return NSMakeSize(proposedContentW + frameDeltaW,
                      proposedContentH + frameDeltaH);
}

- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(self.window, &geo);

    MARU_Event event = {0};
    event.window_resized.geometry = geo;

    _maru_post_event_internal(self.window->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)self.window, &event);
}

- (void)windowDidMove:(NSNotification *)notification {
    (void)notification;
    _maru_cocoa_refresh_window_geometry(self.window, NULL);
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    (void)notification;
    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(self.window, &geo);

    MARU_Event event = {0};
    event.window_resized.geometry = geo;
    _maru_post_event_internal(self.window->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)self.window, &event);
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    win->base.attrs_effective.presentation_state = MARU_WINDOW_PRESENTATION_FULLSCREEN;
    win->base.attrs_effective.visible = true;

    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(win, &geo);

    MARU_Event res_evt = {0};
    res_evt.window_resized.geometry = geo;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)win, &res_evt);

    MARU_Event state_evt = {0};
    state_evt.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE | MARU_WINDOW_STATE_CHANGED_VISIBLE;
    state_evt.window_state_changed.presentation_state = MARU_WINDOW_PRESENTATION_FULLSCREEN;
    state_evt.window_state_changed.visible = true;
    state_evt.window_state_changed.focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    state_evt.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &state_evt);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
    win->base.attrs_effective.presentation_state = [win->ns_window isZoomed] ? MARU_WINDOW_PRESENTATION_MAXIMIZED : MARU_WINDOW_PRESENTATION_NORMAL;
    if (win->base.attrs_effective.presentation_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
        win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    }

    MARU_WindowGeometry geo = {0};
    _maru_cocoa_refresh_window_geometry(win, &geo);

    MARU_Event res_evt = {0};
    res_evt.window_resized.geometry = geo;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)win, &res_evt);

    MARU_Event state_evt = {0};
    state_evt.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
    state_evt.window_state_changed.presentation_state = win->base.attrs_effective.presentation_state;
    state_evt.window_state_changed.visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    state_evt.window_state_changed.focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    state_evt.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &state_evt);
}

- (void)windowDidMiniaturize:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
    win->base.attrs_effective.presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;
    win->base.attrs_effective.visible = false;

    MARU_Event state_evt = {0};
    state_evt.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE | MARU_WINDOW_STATE_CHANGED_VISIBLE;
    state_evt.window_state_changed.presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;
    state_evt.window_state_changed.visible = false;
    state_evt.window_state_changed.focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    state_evt.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &state_evt);
}

- (void)windowDidDeminiaturize:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    win->base.attrs_effective.presentation_state = [win->ns_window isZoomed] ? MARU_WINDOW_PRESENTATION_MAXIMIZED : MARU_WINDOW_PRESENTATION_NORMAL;
    win->base.attrs_effective.visible = true;
    if (win->base.attrs_effective.presentation_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
        win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    } else {
        win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
    }

    MARU_Event state_evt = {0};
    state_evt.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE | MARU_WINDOW_STATE_CHANGED_VISIBLE;
    state_evt.window_state_changed.presentation_state = win->base.attrs_effective.presentation_state;
    state_evt.window_state_changed.visible = true;
    state_evt.window_state_changed.focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    state_evt.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &state_evt);
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
    MARU_Event event = {0};
    event.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_FOCUSED;
    event.window_state_changed.focused = true;
    event.window_state_changed.visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    event.window_state_changed.presentation_state = win->base.attrs_effective.presentation_state;
    event.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &event);
}

- (void)windowDidResignKey:(NSNotification *)notification {
    (void)notification;
    MARU_Window_Cocoa *win = self.window;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
    MARU_Event event = {0};
    event.window_state_changed.changed_fields = MARU_WINDOW_STATE_CHANGED_FOCUSED;
    event.window_state_changed.focused = false;
    event.window_state_changed.visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    event.window_state_changed.presentation_state = win->base.attrs_effective.presentation_state;
    event.window_state_changed.resizable = (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_WINDOW_STATE_CHANGED, (MARU_Window *)win, &event);
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

    if (win->base.attrs_effective.presentation_state == MARU_WINDOW_PRESENTATION_MAXIMIZED) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
    }
    if (win->base.attrs_effective.presentation_state == MARU_WINDOW_PRESENTATION_FULLSCREEN) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
    }
    if (win->base.attrs_effective.visible) {
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    }
    if (win->base.attrs_effective.presentation_state == MARU_WINDOW_PRESENTATION_MINIMIZED) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
    }
    if (win->base.attrs_effective.resizable) {
      win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
    }
    if (create_info->has_decorations) {
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

    if (create_info->app_id) {
        size_t len = strlen(create_info->app_id);
        win->app_id = maru_context_alloc(&ctx_cocoa->base, len + 1);
        if (win->app_id) {
            memcpy(win->app_id, create_info->app_id, len + 1);
        }
    }
    win->content_type = create_info->content_type;

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
    NSUInteger styleMask = 0;
    if (create_info->has_decorations) {
        styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        if (create_info->attributes.resizable) {
            styleMask |= NSWindowStyleMaskResizable;
        }
    } else {
        styleMask = NSWindowStyleMaskBorderless;
    }

    NSWindow *nsWindow = [[NSWindow alloc] initWithContentRect:contentRect
                                                     styleMask:styleMask
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
    if (!nsWindow) {
        _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_BACKEND_FAILURE, "Failed to create NSWindow");
        _maru_unregister_window(&ctx_cocoa->base, (MARU_Window *)win);
        maru_context_free(&ctx_cocoa->base, win->base.title_storage);
        maru_context_free(&ctx_cocoa->base, win);
        return MARU_FAILURE;
    }

    [nsWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [nsWindow setBackgroundColor:[NSColor blackColor]];
    [nsWindow setTitle:[NSString stringWithUTF8String:create_info->attributes.title]];
    if (win->app_id) {
        if (@available(macOS 10.12, *)) {
            [nsWindow setIdentifier:[NSString stringWithUTF8String:win->app_id]];
        }
    }
    [nsWindow setReleasedWhenClosed:NO];
    MARU_WindowDelegate *delegate = [[MARU_WindowDelegate alloc] init];
    delegate.window = win;
    [nsWindow setDelegate:delegate];

    MARU_ContentView *view = [[MARU_ContentView alloc] initWithFrame:contentRect];
    [view setWantsLayer:YES];
    CALayer *layer = [view layer];
    [layer setOpaque:YES];
    view.maruWindow = win;
    
    win->ns_window = nsWindow;
    win->ns_view = view;
    win->ns_layer = layer;
    win->dip_size = create_info->attributes.dip_size;
    win->text_input_session_id = 0;
    win->ime_preedit_active = false;
    win->pending_frame_request = false;
    atomic_init(&win->pending_frame_vblank, false);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVReturn cv_ret = CVDisplayLinkCreateWithActiveCGDisplays(&win->display_link);
    if (cv_ret != kCVReturnSuccess) {
        _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_BACKEND_FAILURE, "Failed to create CVDisplayLink");
    } else {
        CVDisplayLinkSetOutputCallback(win->display_link, _maru_cocoa_display_link_callback, win);
        CVDisplayLinkStart(win->display_link);
    }
#pragma clang diagnostic pop
    _maru_cocoa_associate_window(nsWindow, win);

    [nsWindow setContentView:view];
    [view release];
    
    [nsWindow makeKeyAndOrderFront:nil];
    [nsWindow center];

    _maru_cocoa_refresh_window_geometry(win, NULL);
    _maru_cocoa_update_drop_registration(win);

    maru_updateWindow_Cocoa((MARU_Window *)win, MARU_WINDOW_ATTR_ALL & ~(MARU_WINDOW_ATTR_VISIBLE | MARU_WINDOW_ATTR_PRESENTATION_STATE), &create_info->attributes);
    
    if (create_info->attributes.visible) {
        maru_updateWindow_Cocoa((MARU_Window *)win, MARU_WINDOW_ATTR_VISIBLE, &create_info->attributes);
    }
    
    if (create_info->attributes.presentation_state != MARU_WINDOW_PRESENTATION_NORMAL) {
        maru_updateWindow_Cocoa((MARU_Window *)win, MARU_WINDOW_ATTR_PRESENTATION_STATE, &create_info->attributes);
    }

    [nsWindow center];

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

    if (win->app_id) {
        maru_context_free(win->base.ctx_base, win->app_id);
    }

    for (uint32_t i = 0; i < win->drop_available_mime_type_count; ++i) {
        maru_context_free(win->base.ctx_base, win->drop_available_mime_types[i]);
    }
    maru_context_free(win->base.ctx_base, win->drop_available_mime_types);
    
    NSWindow *nsWindow = win->ns_window;
    _maru_cocoa_associate_window(nsWindow, NULL);

    if (win->display_link) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CVDisplayLinkStop(win->display_link);
        CVDisplayLinkRelease(win->display_link);
#pragma clang diagnostic pop
        win->display_link = NULL;
    }

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
        frame.size.width = effective->dip_size.x;
        frame.size.height = effective->dip_size.y;
        [nsWindow setFrame:[nsWindow frameRectForContentRect:frame] display:YES];
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_POSITION) {
        requested->dip_position = attributes->dip_position;
        effective->dip_position = attributes->dip_position;
        NSRect frame = [nsWindow frame];
        frame.origin.x = effective->dip_position.x;
        frame.origin.y = effective->dip_position.y;
        [nsWindow setFrame:frame display:YES];
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_MIN_SIZE) {
        requested->dip_min_size = attributes->dip_min_size;
        effective->dip_min_size = attributes->dip_min_size;
        [nsWindow setContentMinSize:NSMakeSize(effective->dip_min_size.x, effective->dip_min_size.y)];
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_MAX_SIZE) {
        requested->dip_max_size = attributes->dip_max_size;
        effective->dip_max_size = attributes->dip_max_size;
        NSSize max_size = NSMakeSize(
            effective->dip_max_size.x > 0 ? effective->dip_max_size.x : FLT_MAX,
            effective->dip_max_size.y > 0 ? effective->dip_max_size.y : FLT_MAX
        );
        [nsWindow setContentMaxSize:max_size];
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
        requested->dip_viewport_size = attributes->dip_viewport_size;
        effective->dip_viewport_size = attributes->dip_viewport_size;
    }

    if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
        requested->aspect_ratio = attributes->aspect_ratio;
        effective->aspect_ratio = attributes->aspect_ratio;
    }

    if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
        requested->resizable = attributes->resizable;
        effective->resizable = attributes->resizable;
        NSWindowStyleMask styleMask = [nsWindow styleMask];
        if (effective->resizable) {
            styleMask |= NSWindowStyleMaskResizable;
            win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
        } else {
            styleMask &= ~NSWindowStyleMaskResizable;
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
        }
        [nsWindow setStyleMask:styleMask];
    }

    if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
        requested->presentation_state = attributes->presentation_state;
        effective->presentation_state = attributes->presentation_state;
        const MARU_WindowPresentationState target_state = effective->presentation_state;
        const bool is_fullscreen = ([nsWindow styleMask] & NSWindowStyleMaskFullScreen) != 0;
        const bool is_zoomed = [nsWindow isZoomed];
        const bool is_minimized = [nsWindow isMiniaturized];

        switch (target_state) {
            case MARU_WINDOW_PRESENTATION_FULLSCREEN:
                win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
                if (requested->visible) {
                    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
                }
                if (!is_fullscreen) {
                    [nsWindow toggleFullScreen:nil];
                }
                break;

            case MARU_WINDOW_PRESENTATION_MAXIMIZED:
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
                win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
                if (requested->visible) {
                    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
                }
                if (is_fullscreen) {
                    [nsWindow toggleFullScreen:nil];
                }
                if (is_minimized) {
                    [nsWindow deminiaturize:nil];
                }
                if (!is_zoomed) {
                    [nsWindow zoom:nil];
                }
                break;

            case MARU_WINDOW_PRESENTATION_MINIMIZED:
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
                win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
                if (is_fullscreen) {
                    [nsWindow toggleFullScreen:nil];
                }
                if (!is_minimized) {
                    [nsWindow miniaturize:nil];
                }
                break;

            case MARU_WINDOW_PRESENTATION_NORMAL:
            default:
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
                win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
                if (requested->visible) {
                    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
                }
                if (is_fullscreen) {
                    [nsWindow toggleFullScreen:nil];
                }
                if (is_zoomed) {
                    [nsWindow zoom:nil];
                }
                if (is_minimized) {
                    [nsWindow deminiaturize:nil];
                }
                break;
        }

    }

    if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
        requested->visible = attributes->visible;
        if (requested->visible) {
            if (effective->presentation_state != MARU_WINDOW_PRESENTATION_MINIMIZED) {
                [nsWindow makeKeyAndOrderFront:nil];
                win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
            }
        } else {
            [nsWindow orderOut:nil];
            win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
        }
        effective->visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
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

    if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
        MARU_TextInputType old_type = effective->text_input_type;
        requested->text_input_type = attributes->text_input_type;
        effective->text_input_type = attributes->text_input_type;
        
        if (old_type == MARU_TEXT_INPUT_TYPE_NONE && effective->text_input_type != MARU_TEXT_INPUT_TYPE_NONE) {
            win->text_input_session_id++;
            win->ime_preedit_active = false;
            MARU_Event evt = {0};
            evt.text_edit_started.session_id = win->text_input_session_id;
            _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_STARTED, (MARU_Window *)win, &evt);
        } else if (old_type != MARU_TEXT_INPUT_TYPE_NONE && effective->text_input_type == MARU_TEXT_INPUT_TYPE_NONE) {
            if (win->ime_preedit_active) {
                win->ime_preedit_active = false;
                MARU_Event evt = {0};
                evt.text_edit_ended.session_id = win->text_input_session_id;
                evt.text_edit_ended.canceled = true;
                _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_ENDED, (MARU_Window *)win, &evt);
            }
            MARU_Event evt = {0};
            evt.text_edit_ended.session_id = win->text_input_session_id;
            evt.text_edit_ended.canceled = false;
            _maru_post_event_internal(win->base.ctx_base, MARU_EVENT_TEXT_EDIT_ENDED, (MARU_Window *)win, &evt);
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT) {
        requested->dip_text_input_rect = attributes->dip_text_input_rect;
        effective->dip_text_input_rect = attributes->dip_text_input_rect;
    }

    if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT) {
        if (win->base.surrounding_text_storage) {
            maru_context_free(win->base.ctx_base, win->base.surrounding_text_storage);
            win->base.surrounding_text_storage = NULL;
        }
        requested->surrounding_text = NULL;
        effective->surrounding_text = NULL;

        if (attributes->surrounding_text) {
            size_t len = strlen(attributes->surrounding_text);
            win->base.surrounding_text_storage = maru_context_alloc(win->base.ctx_base, len + 1);
            if (win->base.surrounding_text_storage) {
                memcpy(win->base.surrounding_text_storage, attributes->surrounding_text, len + 1);
                requested->surrounding_text = win->base.surrounding_text_storage;
                effective->surrounding_text = win->base.surrounding_text_storage;
            }
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE) {
        requested->surrounding_cursor_byte = attributes->surrounding_cursor_byte;
        effective->surrounding_cursor_byte = attributes->surrounding_cursor_byte;
    }

    if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE) {
        requested->surrounding_anchor_byte = attributes->surrounding_anchor_byte;
        effective->surrounding_anchor_byte = attributes->surrounding_anchor_byte;
    }

    if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
        requested->accept_drop = attributes->accept_drop;
        effective->accept_drop = attributes->accept_drop;
        _maru_cocoa_update_drop_registration(win);
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

    win->pending_frame_request = true;
    return MARU_SUCCESS;
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
