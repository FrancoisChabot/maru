// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>

@interface MARU_ClipboardDelegate : NSObject <NSPasteboardItemDataProvider, NSPasteboardTypeOwner>
@property (nonatomic, assign) MARU_Context_Cocoa *context;
@end

@implementation MARU_ClipboardDelegate

- (void)pasteboard:(NSPasteboard *)sender item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type {
    (void)item;
    [self pasteboard:sender provideDataForType:type];
}

- (void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSPasteboardType)type {
    MARU_DataRequest_Cocoa req = {0};
    req.base.ctx_base = &self.context->base;
    req.base.target = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
    req.ns_pasteboard = sender;
    req.ns_type = type;

    MARU_Event event = {0};
    event.data_requested.target = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
    event.data_requested.mime_type = _maru_ns_type_to_mime(type);
    event.data_requested.request = (MARU_DataRequest *)&req;

    _maru_dispatch_event(&self.context->base, MARU_EVENT_DATA_REQUESTED, NULL, &event);
}

- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)sender {
    (void)sender;
    // Ownership lost
}

@end

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList mime_types, MARU_DropActionMask allowed_actions) {
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
        MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)maru_getWindowContext(window);
        if (!ctx->clipboard_delegate) {
            ctx->clipboard_delegate = [[MARU_ClipboardDelegate alloc] init];
            ((MARU_ClipboardDelegate *)ctx->clipboard_delegate).context = ctx;
        }

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
    }
    // DnD not implemented yet
    return MARU_FAILURE;
}

MARU_Status maru_provideData_Cocoa(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags) {
    (void)flags;
    MARU_DataRequest_Cocoa *req = (MARU_DataRequest_Cocoa *)request;
    if (req->base.target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
        NSData *nsData = [NSData dataWithBytes:data length:size];
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
