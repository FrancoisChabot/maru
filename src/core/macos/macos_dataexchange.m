// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <AppKit/NSDragging.h>

@interface MARU_ZeroCopyEntry : NSObject
@property (nonatomic, assign) const void *data;
@property (nonatomic, assign) size_t size;
@end

@implementation MARU_ZeroCopyEntry
@end

@interface MARU_ClipboardDelegate : NSObject <NSPasteboardItemDataProvider, NSPasteboardTypeOwner> {
    NSMutableDictionary *_zeroCopyEntries;
}
@property (nonatomic, assign) MARU_Context_Cocoa *context;
@property (nonatomic, assign) MARU_DataExchangeTarget target;

- (void)addZeroCopyEntry:(const void *)data size:(size_t)size forType:(NSPasteboardType)type;
@end

@implementation MARU_ClipboardDelegate

- (id)init {
    self = [super init];
    if (self) {
        _zeroCopyEntries = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_zeroCopyEntries release];
    [super dealloc];
}

- (void)addZeroCopyEntry:(const void *)data size:(size_t)size forType:(NSPasteboardType)type {
    MARU_ZeroCopyEntry *entry = [[MARU_ZeroCopyEntry alloc] init];
    entry.data = data;
    entry.size = size;
    [_zeroCopyEntries setObject:entry forKey:type];
    [entry release];
}

- (void)pasteboard:(NSPasteboard *)sender item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type {
    (void)item;
    [self pasteboard:sender provideDataForType:type];
}

- (void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSPasteboardType)type {
    MARU_DataRequest_Cocoa req = {0};
    req.base.ctx_base = &self.context->base;
    req.base.target = self.target;
    req.ns_pasteboard = sender;
    req.ns_type = type;
    req.ns_delegate = self;

    MARU_Event event = {0};
    event.data_requested.target = self.target;
    event.data_requested.mime_type = _maru_ns_type_to_mime(type);
    event.data_requested.request = (MARU_DataRequest *)&req;

    _maru_dispatch_event(&self.context->base, MARU_EVENT_DATA_REQUESTED, NULL, &event);
}

- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)sender {
    (void)sender;
    
    for (NSPasteboardType type in _zeroCopyEntries) {
        MARU_ZeroCopyEntry *entry = [_zeroCopyEntries objectForKey:type];
        MARU_Event event = {0};
        event.data_released.target = self.target;
        event.data_released.mime_type = _maru_ns_type_to_mime(type);
        event.data_released.data = entry.data;
        event.data_released.size = entry.size;
        _maru_post_event_internal(&self.context->base, MARU_EVENT_DATA_RELEASED, NULL, &event);
    }
    [_zeroCopyEntries removeAllObjects];
}

@end

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList mime_types, MARU_DropActionMask allowed_actions) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)maru_getWindowContext(window);
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
        if (!ctx->clipboard_delegate) {
            ctx->clipboard_delegate = [[MARU_ClipboardDelegate alloc] init];
            ((MARU_ClipboardDelegate *)ctx->clipboard_delegate).context = ctx;
        }
        ((MARU_ClipboardDelegate *)ctx->clipboard_delegate).target = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;

        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];

        NSMutableArray *types = [NSMutableArray arrayWithCapacity:mime_types.count];
        for (uint32_t i = 0; i < mime_types.count; ++i) {
            NSPasteboardType type = _maru_mime_to_ns_type(mime_types.strings[i]);
            [types addObject:type];
        }

        NSPasteboardItem *item = [[NSPasteboardItem alloc] init];
        [item setDataProvider:ctx->clipboard_delegate forTypes:types];
        [pb writeObjects:@[item]];
        [item release];

        return MARU_SUCCESS;
    } else if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
        MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
        
        MARU_ClipboardDelegate *dnd_delegate = [[MARU_ClipboardDelegate alloc] init];
        dnd_delegate.context = ctx;
        dnd_delegate.target = MARU_DATA_EXCHANGE_TARGET_DRAG_DROP;

        NSPasteboardItem *item = [[NSPasteboardItem alloc] init];
        NSMutableArray *types = [NSMutableArray arrayWithCapacity:mime_types.count];
        for (uint32_t i = 0; i < mime_types.count; ++i) {
            NSPasteboardType type = _maru_mime_to_ns_type(mime_types.strings[i]);
            [types addObject:type];
        }
        [item setDataProvider:dnd_delegate forTypes:types];

        NSDraggingItem *dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:item];
        NSPoint mousePos = [NSEvent mouseLocation];
        NSRect windowFrame = [win->ns_window frame];
        NSPoint localPos = NSMakePoint(mousePos.x - windowFrame.origin.x, mousePos.y - windowFrame.origin.y);
        [dragItem setDraggingFrame:NSMakeRect(localPos.x - 16, localPos.y - 16, 32, 32) contents:nil];

        NSEvent *currentEvent = [NSApp currentEvent];
        [(NSView *)win->ns_view beginDraggingSessionWithItems:@[dragItem] event:currentEvent source:(id<NSDraggingSource>)win->ns_view];
        
        [dragItem release];
        [item release];
        [dnd_delegate autorelease];

        return MARU_SUCCESS;
    }
    return MARU_FAILURE;
}

MARU_Status maru_provideData_Cocoa(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags) {
    MARU_DataRequest_Cocoa *req = (MARU_DataRequest_Cocoa *)request;
    if (req->base.target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD || req->base.target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
        NSData *nsData;
        if (flags & MARU_DATA_PROVIDE_FLAG_ZERO_COPY) {
            nsData = [NSData dataWithBytesNoCopy:(void *)data length:size freeWhenDone:NO];
            [(MARU_ClipboardDelegate *)req->ns_delegate addZeroCopyEntry:data size:size forType:req->ns_type];
        } else {
            nsData = [NSData dataWithBytes:data length:size];
        }
        [req->ns_pasteboard setData:nsData forType:req->ns_type];
        return MARU_SUCCESS;
    }
    return MARU_FAILURE;
}

MARU_Status maru_requestData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *userdata) {
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSPasteboardType type = _maru_mime_to_ns_type(mime_type);
        NSData *nsData = [pb dataForType:type];

        MARU_Event event = {0};
        event.data_received.userdata = userdata;
        event.data_received.target = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
        event.data_received.mime_type = mime_type;

        if (nsData) {
            event.data_received.status = MARU_SUCCESS;
            event.data_received.data = [nsData bytes];
            event.data_received.size = [nsData length];
        } else {
            event.data_received.status = MARU_FAILURE;
        }

        _maru_post_event_internal((MARU_Context_Base *)maru_getWindowContext(window), MARU_EVENT_DATA_RECEIVED, NULL, &event);
        return MARU_SUCCESS;
    } else if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
        MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
        if (!win->current_dragging_info) return MARU_FAILURE;

        NSPasteboard *pb = [win->current_dragging_info draggingPasteboard];
        NSPasteboardType type = _maru_mime_to_ns_type(mime_type);
        NSData *nsData = [pb dataForType:type];

        MARU_Event event = {0};
        event.data_received.userdata = userdata;
        event.data_received.target = MARU_DATA_EXCHANGE_TARGET_DRAG_DROP;
        event.data_received.mime_type = mime_type;

        if (nsData) {
            event.data_received.status = MARU_SUCCESS;
            event.data_received.data = [nsData bytes];
            event.data_received.size = [nsData length];
        } else {
            event.data_received.status = MARU_FAILURE;
        }

        _maru_dispatch_event(win->base.ctx_base, MARU_EVENT_DATA_RECEIVED, (MARU_Window *)win, &event);
        return MARU_SUCCESS;
    }
    return MARU_FAILURE;
}

MARU_Status maru_getAvailableMIMETypes_Cocoa(const MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList *out_list) {
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
        MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)maru_getWindowContext(window);
        
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSArray *types = [pb types];
        
        ctx->clipboard_available_mime_type_count = (uint32_t)[types count];
        ctx->clipboard_available_mime_types = maru_context_alloc(&ctx->base, sizeof(char *) * ctx->clipboard_available_mime_type_count);
        
        for (uint32_t i = 0; i < ctx->clipboard_available_mime_type_count; ++i) {
            const char *mime = _maru_ns_type_to_mime([types objectAtIndex:i]);
            size_t len = strlen(mime);
            ctx->clipboard_available_mime_types[i] = maru_context_alloc(&ctx->base, len + 1);
            memcpy(ctx->clipboard_available_mime_types[i], mime, len + 1);
        }

        out_list->count = ctx->clipboard_available_mime_type_count;
        out_list->strings = (const char *const *)ctx->clipboard_available_mime_types;
        return MARU_SUCCESS;
    } else if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
        MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
        if (!win->current_dragging_info) {
            out_list->count = win->drop_available_mime_type_count;
            out_list->strings = (const char *const *)win->drop_available_mime_types;
            return MARU_SUCCESS;
        }

        NSPasteboard *pb = [win->current_dragging_info draggingPasteboard];
        NSArray *types = [pb types];

        // For simplicity, we refresh the cached types in the window
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

        out_list->count = win->drop_available_mime_type_count;
        out_list->strings = (const char *const *)win->drop_available_mime_types;
        return MARU_SUCCESS;
    }
    return MARU_FAILURE;
}
