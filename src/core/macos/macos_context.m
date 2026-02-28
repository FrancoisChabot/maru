// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

static MARU_Key _maru_cocoa_translate_key(unsigned short scancode) {
    static const MARU_Key table[128] = {
        [0x00] = MARU_KEY_A, [0x01] = MARU_KEY_S, [0x02] = MARU_KEY_D, [0x03] = MARU_KEY_F,
        [0x04] = MARU_KEY_H, [0x05] = MARU_KEY_G, [0x06] = MARU_KEY_Z, [0x07] = MARU_KEY_X,
        [0x08] = MARU_KEY_C, [0x09] = MARU_KEY_V, [0x0B] = MARU_KEY_B, [0x0C] = MARU_KEY_Q,
        [0x0D] = MARU_KEY_W, [0x0E] = MARU_KEY_E, [0x0F] = MARU_KEY_R, [0x10] = MARU_KEY_Y,
        [0x11] = MARU_KEY_T, [0x12] = MARU_KEY_1, [0x13] = MARU_KEY_2, [0x14] = MARU_KEY_3,
        [0x15] = MARU_KEY_4, [0x16] = MARU_KEY_6, [0x17] = MARU_KEY_5, [0x18] = MARU_KEY_EQUAL,
        [0x19] = MARU_KEY_9, [0x1A] = MARU_KEY_7, [0x1B] = MARU_KEY_MINUS, [0x1C] = MARU_KEY_8,
        [0x1D] = MARU_KEY_0, [0x1E] = MARU_KEY_RIGHT_BRACKET, [0x1F] = MARU_KEY_O, [0x20] = MARU_KEY_U,
        [0x21] = MARU_KEY_LEFT_BRACKET, [0x22] = MARU_KEY_I, [0x23] = MARU_KEY_P, [0x24] = MARU_KEY_ENTER,
        [0x25] = MARU_KEY_L, [0x26] = MARU_KEY_J, [0x27] = MARU_KEY_APOSTROPHE, [0x28] = MARU_KEY_K,
        [0x29] = MARU_KEY_SEMICOLON, [0x2A] = MARU_KEY_BACKSLASH, [0x2B] = MARU_KEY_COMMA, [0x2C] = MARU_KEY_SLASH,
        [0x2D] = MARU_KEY_N, [0x2E] = MARU_KEY_M, [0x2F] = MARU_KEY_PERIOD, [0x30] = MARU_KEY_TAB,
        [0x31] = MARU_KEY_SPACE, [0x32] = MARU_KEY_GRAVE_ACCENT, [0x33] = MARU_KEY_BACKSPACE, [0x35] = MARU_KEY_ESCAPE,
        [0x37] = MARU_KEY_LEFT_META, [0x38] = MARU_KEY_LEFT_SHIFT, [0x39] = MARU_KEY_CAPS_LOCK, [0x3A] = MARU_KEY_LEFT_ALT,
        [0x3B] = MARU_KEY_LEFT_CONTROL, [0x3C] = MARU_KEY_RIGHT_SHIFT, [0x3D] = MARU_KEY_RIGHT_ALT, [0x3E] = MARU_KEY_RIGHT_CONTROL,
        [0x3F] = MARU_KEY_RIGHT_META, [0x41] = MARU_KEY_KP_DECIMAL, [0x43] = MARU_KEY_KP_MULTIPLY, [0x45] = MARU_KEY_KP_ADD,
        [0x47] = MARU_KEY_NUM_LOCK, [0x4B] = MARU_KEY_KP_DIVIDE, [0x4C] = MARU_KEY_KP_ENTER, [0x4E] = MARU_KEY_KP_SUBTRACT,
        [0x51] = MARU_KEY_KP_EQUAL, [0x52] = MARU_KEY_KP_0, [0x53] = MARU_KEY_KP_1, [0x54] = MARU_KEY_KP_2,
        [0x55] = MARU_KEY_KP_3, [0x56] = MARU_KEY_KP_4, [0x57] = MARU_KEY_KP_5, [0x58] = MARU_KEY_KP_6,
        [0x59] = MARU_KEY_KP_7, [0x5B] = MARU_KEY_KP_8, [0x5C] = MARU_KEY_KP_9, [0x60] = MARU_KEY_F5,
        [0x61] = MARU_KEY_F6, [0x62] = MARU_KEY_F7, [0x63] = MARU_KEY_F3, [0x64] = MARU_KEY_F8,
        [0x65] = MARU_KEY_F9, [0x67] = MARU_KEY_F11, [0x6D] = MARU_KEY_F10, [0x6F] = MARU_KEY_F12,
        [0x72] = MARU_KEY_INSERT,
        [0x73] = MARU_KEY_HOME, [0x74] = MARU_KEY_PAGE_UP, [0x75] = MARU_KEY_DELETE, [0x76] = MARU_KEY_F4,
        [0x77] = MARU_KEY_END, [0x78] = MARU_KEY_F2, [0x79] = MARU_KEY_PAGE_DOWN, [0x7A] = MARU_KEY_F1,
        [0x7B] = MARU_KEY_LEFT, [0x7C] = MARU_KEY_RIGHT, [0x7D] = MARU_KEY_DOWN, [0x7E] = MARU_KEY_UP,
    };

    return (scancode < 128) ? table[scancode] : MARU_KEY_UNKNOWN;
}

static MARU_ModifierFlags _maru_cocoa_translate_modifiers(NSEventModifierFlags flags) {
    MARU_ModifierFlags mods = 0;
    if (flags & NSEventModifierFlagShift) mods |= MARU_MODIFIER_SHIFT;
    if (flags & NSEventModifierFlagControl) mods |= MARU_MODIFIER_CONTROL;
    if (flags & NSEventModifierFlagOption) mods |= MARU_MODIFIER_ALT;
    if (flags & NSEventModifierFlagCommand) mods |= MARU_MODIFIER_META;
    if (flags & NSEventModifierFlagCapsLock) mods |= MARU_MODIFIER_CAPS_LOCK;
    if (flags & NSEventModifierFlagNumericPad) mods |= MARU_MODIFIER_NUM_LOCK;
    return mods;
}

MARU_Cocoa_Global g_maru_cocoa = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, false};

static MARU_Window_Cocoa *_maru_cocoa_resolve_window_globally(NSWindow *nsWindow, MARU_Context_Cocoa **out_ctx) {
    if (!nsWindow) return NULL;
    
    pthread_mutex_lock(&g_maru_cocoa.mutex);
    for (MARU_Context_Cocoa *ctx = g_maru_cocoa.contexts_head; ctx; ctx = ctx->next) {
        for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
            MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)it;
            if (win->ns_window == nsWindow) {
                if (out_ctx) *out_ctx = ctx;
                pthread_mutex_unlock(&g_maru_cocoa.mutex);
                return win;
            }
        }
    }
    pthread_mutex_unlock(&g_maru_cocoa.mutex);
    return NULL;
}

static void _maru_cocoa_process_event(MARU_Context_Cocoa *pumping_ctx, NSEvent *nsEvent) {
    NSEventType type = [nsEvent type];
    MARU_Context_Cocoa *target_ctx = NULL;
    MARU_Window_Cocoa *window = _maru_cocoa_resolve_window_globally([nsEvent window], &target_ctx);

    if (!target_ctx) {
        // Application-wide events go to the primary context if it exists
        target_ctx = g_maru_cocoa.primary_context;
    }

    if (!target_ctx) return;

    // Helper to dispatch or route an event
    #define DISPATCH_OR_ROUTE(TYPE, EVT) \
        if (target_ctx == pumping_ctx) { \
            _maru_dispatch_event(&target_ctx->base, TYPE, (MARU_Window *)window, &EVT); \
        } else { \
            if (_maru_event_queue_push(&target_ctx->route_queue, TYPE, (MARU_Window *)window, EVT)) { \
                pthread_mutex_lock(&target_ctx->route_mutex); \
                pthread_cond_signal(&target_ctx->route_cond); \
                pthread_mutex_unlock(&target_ctx->route_mutex); \
            } \
        }

    switch (type) {
        case NSEventTypeKeyDown:
        case NSEventTypeKeyUp: {
            if (!window) break;
            MARU_Key key = _maru_cocoa_translate_key([nsEvent keyCode]);
            MARU_ButtonState state = (type == NSEventTypeKeyDown) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
            
            window->base.keyboard_state[key] = (MARU_ButtonState8)state;

            MARU_Event event = {0};
            event.key.raw_key = key;
            event.key.state = state;
            event.key.modifiers = _maru_cocoa_translate_modifiers([nsEvent modifierFlags]);
            
            DISPATCH_OR_ROUTE(MARU_EVENT_KEY_STATE_CHANGED, event);
            break;
        }
        case NSEventTypeFlagsChanged: {
            if (!window) break;
            NSEventModifierFlags new_mods = [nsEvent modifierFlags];
            unsigned short scancode = [nsEvent keyCode];
            MARU_Key key = _maru_cocoa_translate_key(scancode);
            if (key == MARU_KEY_UNKNOWN) break;

            bool pressed = false;
            if (scancode == 0x38 || scancode == 0x3C) pressed = (new_mods & NSEventModifierFlagShift) != 0;
            else if (scancode == 0x3B || scancode == 0x3E) pressed = (new_mods & NSEventModifierFlagControl) != 0;
            else if (scancode == 0x3A || scancode == 0x3D) pressed = (new_mods & NSEventModifierFlagOption) != 0;
            else if (scancode == 0x37 || scancode == 0x3F) pressed = (new_mods & NSEventModifierFlagCommand) != 0;
            else if (scancode == 0x39) pressed = (new_mods & NSEventModifierFlagCapsLock) != 0;

            window->base.keyboard_state[key] = pressed ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;

            MARU_Event event = {0};
            event.key.raw_key = key;
            event.key.state = pressed ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
            event.key.modifiers = _maru_cocoa_translate_modifiers(new_mods);
            
            DISPATCH_OR_ROUTE(MARU_EVENT_KEY_STATE_CHANGED, event);
            break;
        }
        case NSEventTypeLeftMouseDown:
        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseDown:
        case NSEventTypeRightMouseUp:
        case NSEventTypeOtherMouseDown:
        case NSEventTypeOtherMouseUp: {
            if (!window) break;
            uint32_t channel = (uint32_t)[nsEvent buttonNumber];
            MARU_ButtonState state = (type == NSEventTypeLeftMouseDown || type == NSEventTypeRightMouseDown || type == NSEventTypeOtherMouseDown) 
                                     ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;

            if (window->base.mouse_button_states && channel < window->base.pub.mouse_button_count) {
                window->base.mouse_button_states[channel] = (MARU_ButtonState8)state;
            }

            MARU_Event event = {0};
            event.mouse_button.button_id = channel;
            event.mouse_button.state = state;
            event.mouse_button.modifiers = _maru_cocoa_translate_modifiers([nsEvent modifierFlags]);
            
            DISPATCH_OR_ROUTE(MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED, event);
            break;
        }
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged: {
            if (!window) break;
            NSPoint p = [nsEvent locationInWindow];
            NSRect contentRect = [window->ns_window contentRectForFrameRect:[window->ns_window frame]];
            
            MARU_Event event = {0};
            event.mouse_motion.position.x = (MARU_Scalar)p.x;
            event.mouse_motion.position.y = (MARU_Scalar)(contentRect.size.height - p.y);
            event.mouse_motion.delta.x = (MARU_Scalar)[nsEvent deltaX];
            event.mouse_motion.delta.y = (MARU_Scalar)[nsEvent deltaY];
            event.mouse_motion.raw_delta.x = (MARU_Scalar)[nsEvent deltaX];
            event.mouse_motion.raw_delta.y = (MARU_Scalar)[nsEvent deltaY];
            event.mouse_motion.modifiers = _maru_cocoa_translate_modifiers([nsEvent modifierFlags]);
            
            DISPATCH_OR_ROUTE(MARU_EVENT_MOUSE_MOVED, event);
            break;
        }
        case NSEventTypeScrollWheel: {
            if (!window) break;
            MARU_Event event = {0};
            event.mouse_scroll.delta.x = (MARU_Scalar)[nsEvent scrollingDeltaX];
            event.mouse_scroll.delta.y = (MARU_Scalar)[nsEvent scrollingDeltaY];
            
            if (![nsEvent hasPreciseScrollingDeltas]) {
                event.mouse_scroll.delta.x *= (MARU_Scalar)10.0;
                event.mouse_scroll.delta.y *= (MARU_Scalar)10.0;
            }
            event.mouse_scroll.modifiers = _maru_cocoa_translate_modifiers([nsEvent modifierFlags]);
            
            DISPATCH_OR_ROUTE(MARU_EVENT_MOUSE_SCROLLED, event);
            break;
        }
        default:
            break;
    }
}

MARU_Status maru_createContext_Cocoa(const MARU_ContextCreateInfo *create_info,
                                      MARU_Context **out_context) {
    pthread_mutex_lock(&g_maru_cocoa.mutex);
    
    bool is_first = (g_maru_cocoa.contexts_head == NULL);
    if (is_first) {
        if (![NSThread isMainThread]) {
            pthread_mutex_unlock(&g_maru_cocoa.mutex);
            fprintf(stderr, "Error: The first Maru context on macOS must be created on the main thread.\n");
            return MARU_FAILURE;
        }
    }

    MARU_Context_Cocoa *ctx = maru_context_alloc_bootstrap(create_info, sizeof(MARU_Context_Cocoa));
    if (!ctx) {
        pthread_mutex_unlock(&g_maru_cocoa.mutex);
        return MARU_FAILURE;
    }

    if (create_info->allocator.alloc_cb) {
        ctx->base.allocator = create_info->allocator;
    } else {
        ctx->base.allocator.alloc_cb = _maru_default_alloc;
        ctx->base.allocator.realloc_cb = _maru_default_realloc;
        ctx->base.allocator.free_cb = _maru_default_free;
        ctx->base.allocator.userdata = NULL;
    }

    _maru_init_context_base(&ctx->base);
    ctx->base.pub.backend_type = MARU_BACKEND_COCOA;
#ifdef MARU_INDIRECT_BACKEND
    ctx->base.backend = &maru_backend_Cocoa;
#endif

#ifdef MARU_VALIDATE_API_CALLS
    extern MARU_ThreadId _maru_getCurrentThreadId(void);
    ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

    _maru_update_context_base(&ctx->base, MARU_CONTEXT_ATTR_ALL, &create_info->attributes);

    if (!_maru_event_queue_init(&ctx->route_queue, &ctx->base, MARU_COCOA_ROUTE_QUEUE_CAPACITY)) {
        maru_context_free(&ctx->base, ctx);
        pthread_mutex_unlock(&g_maru_cocoa.mutex);
        return MARU_FAILURE;
    }
    pthread_mutex_init(&ctx->route_mutex, NULL);
    pthread_cond_init(&ctx->route_cond, NULL);

    if (is_first) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        g_maru_cocoa.primary_context = ctx;
    }

    ctx->ns_app = NSApp;
    ctx->last_modifiers = [NSEvent modifierFlags];
    
    // Add to global context registry
    ctx->next = g_maru_cocoa.contexts_head;
    if (g_maru_cocoa.contexts_head) g_maru_cocoa.contexts_head->prev = ctx;
    g_maru_cocoa.contexts_head = ctx;

    pthread_mutex_unlock(&g_maru_cocoa.mutex);

    *out_context = (MARU_Context *)ctx;
    return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_Cocoa(MARU_Context *context) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
    
    pthread_mutex_lock(&g_maru_cocoa.mutex);
    
    if (ctx->prev) ctx->prev->next = ctx->next;
    if (ctx->next) ctx->next->prev = ctx->prev;
    if (g_maru_cocoa.contexts_head == ctx) g_maru_cocoa.contexts_head = ctx->next;
    if (g_maru_cocoa.primary_context == ctx) g_maru_cocoa.primary_context = g_maru_cocoa.contexts_head;
    
    pthread_mutex_unlock(&g_maru_cocoa.mutex);

    _maru_cleanup_context_base(&ctx->base);
    _maru_event_queue_cleanup(&ctx->route_queue, &ctx->base);
    pthread_mutex_destroy(&ctx->route_mutex);
    pthread_cond_destroy(&ctx->route_cond);
    maru_context_free(&ctx->base, ctx);
    return MARU_SUCCESS;
}

MARU_Status maru_updateContext_Cocoa(MARU_Context *context, uint64_t field_mask,
                                       const MARU_ContextAttributes *attributes) {
    _maru_update_context_base((MARU_Context_Base *)context, field_mask, attributes);
    return MARU_SUCCESS;
}

static void _maru_cocoa_drain_route_queue(MARU_Context_Cocoa *ctx) {
    MARU_EventId type;
    MARU_Window *window;
    MARU_Event evt;
    while (_maru_event_queue_pop(&ctx->route_queue, &type, &window, &evt)) {
        _maru_dispatch_event(&ctx->base, type, window, &evt);
    }
}

MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
    
    MARU_PumpContext pump_ctx = {callback, userdata};
    ctx->base.pump_ctx = &pump_ctx;

    @autoreleasepool {
        _maru_drain_queued_events(&ctx->base);
        _maru_cocoa_drain_route_queue(ctx);

        // Only the main thread is allowed to pump NSApp.
        // Worker threads just drain their internal queues.
        if ([NSThread isMainThread]) {
            NSDate *until = nil;
            if (timeout_ms == 0) {
                until = [NSDate distantPast];
            } else if (timeout_ms == 0xFFFFFFFF) {
                until = [NSDate distantFuture];
            } else {
                until = [NSDate dateWithTimeIntervalSinceNow:timeout_ms / 1000.0];
            }

            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:until
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (event) {
                _maru_cocoa_process_event(ctx, event);
                [NSApp sendEvent:event];

                while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                     untilDate:[NSDate distantPast]
                                                        inMode:NSDefaultRunLoopMode
                                                       dequeue:YES])) {
                    _maru_cocoa_process_event(ctx, event);
                    [NSApp sendEvent:event];
                }
            }
        } else if (timeout_ms > 0) {
            // Worker thread: wait for events to be routed if timeout > 0
            struct timespec ts;
            if (timeout_ms != 0xFFFFFFFF) {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
                ts.tv_nsec = ((long)tv.tv_usec + (long)(timeout_ms % 1000) * 1000L) * 1000L;
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }
            }

            pthread_mutex_lock(&ctx->route_mutex);
            // Only wait if the queue is empty
            MARU_EventId t; MARU_Window *w; MARU_Event e;
            if (!_maru_event_queue_pop(&ctx->route_queue, &t, &w, &e)) {
                if (timeout_ms == 0xFFFFFFFF) {
                    pthread_cond_wait(&ctx->route_cond, &ctx->route_mutex);
                } else {
                    pthread_cond_timedwait(&ctx->route_cond, &ctx->route_mutex, &ts);
                }
            } else {
                // We popped an event while trying to wait, put it back or dispatch it.
                // Simplest is to just dispatch it here and continue draining below.
                _maru_dispatch_event(&ctx->base, t, w, &e);
            }
            pthread_mutex_unlock(&ctx->route_mutex);
        }

        _maru_cocoa_drain_route_queue(ctx);
        _maru_drain_queued_events(&ctx->base);
    }

    ctx->base.pump_ctx = NULL;
    return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_Cocoa(MARU_Context *context) {
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:YES];
    return MARU_SUCCESS;
}

void *_maru_getContextNativeHandle_Cocoa(MARU_Context *context) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
    return ctx->ns_app;
}
