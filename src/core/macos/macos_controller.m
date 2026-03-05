// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <GameController/GameController.h>
#import <stdatomic.h>

typedef struct MARU_Controller_Cocoa {
  MARU_Controller_Base base;
  GCController *gc_controller;
  
  MARU_AnalogChannelInfo *analog_channel_infos;
  MARU_AnalogInputState *analog_states;
  MARU_ButtonChannelInfo *button_channel_infos;
  MARU_ButtonState8 *button_states;
} MARU_Controller_Cocoa;

@interface MARU_ControllerObserver : NSObject
@end

@implementation MARU_ControllerObserver

- (void)controllerDidConnect:(NSNotification *)notification {
    MARU_Context_Cocoa *ctx = g_maru_cocoa.primary_context;
    if (ctx) {
        atomic_store(&ctx->controllers_dirty, true);
        maru_wakeContext_Cocoa((MARU_Context *)ctx);
    }
}

- (void)controllerDidDisconnect:(NSNotification *)notification {
    MARU_Context_Cocoa *ctx = g_maru_cocoa.primary_context;
    if (ctx) {
        atomic_store(&ctx->controllers_dirty, true);
        maru_wakeContext_Cocoa((MARU_Context *)ctx);
    }
}

@end

void _maru_controller_free(MARU_Controller_Base *controller) {
    MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)controller;
    MARU_Context_Base *ctx = controller->ctx_base;
    
    c->gc_controller.extendedGamepad.valueChangedHandler = nil;
    
    maru_context_free(ctx, c->analog_channel_infos);
    maru_context_free(ctx, c->analog_states);
    maru_context_free(ctx, c->button_channel_infos);
    maru_context_free(ctx, c->button_states);
    
    maru_context_free(ctx, c);
}

static void _maru_cocoa_dispatch_button_event(MARU_Controller_Cocoa *c, uint32_t button_id, bool pressed) {
    MARU_ButtonState state = pressed ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (c->button_states[button_id] == (MARU_ButtonState8)state) return;
    
    c->button_states[button_id] = (MARU_ButtonState8)state;
    
    MARU_Event event = {0};
    event.controller_button_state_changed.controller = (MARU_Controller *)c;
    event.controller_button_state_changed.button_id = button_id;
    event.controller_button_state_changed.state = state;
    _maru_post_event_internal(c->base.ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &event);
}

static MARU_Controller_Cocoa *_maru_cocoa_create_controller(MARU_Context_Cocoa *ctx, GCController *gc) {
    MARU_Controller_Cocoa *c = maru_context_alloc(&ctx->base, sizeof(MARU_Controller_Cocoa));
    if (!c) return NULL;
    
    c->base.ctx_base = &ctx->base;
    c->base.pub.context = (MARU_Context *)ctx;
    c->base.is_active = true;
    c->base.ref_count = 1;
    c->gc_controller = gc;
    
    c->base.pub.analog_count = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;
    c->base.pub.button_count = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;
    
    c->analog_channel_infos = maru_context_alloc(&ctx->base, sizeof(MARU_AnalogChannelInfo) * c->base.pub.analog_count);
    c->analog_states = maru_context_alloc(&ctx->base, sizeof(MARU_AnalogInputState) * c->base.pub.analog_count);
    c->button_channel_infos = maru_context_alloc(&ctx->base, sizeof(MARU_ButtonChannelInfo) * c->base.pub.button_count);
    c->button_states = maru_context_alloc(&ctx->base, sizeof(MARU_ButtonState8) * c->base.pub.button_count);
    
    c->base.pub.analog_channels = c->analog_channel_infos;
    c->base.pub.analogs = c->analog_states;
    c->base.pub.button_channels = c->button_channel_infos;
    c->base.pub.buttons = c->button_states;
    c->base.pub.metrics = &c->base.metrics;

    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_X].name = "Left X";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_Y].name = "Left Y";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_X].name = "Right X";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_Y].name = "Right Y";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].name = "Left Trigger";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].name = "Right Trigger";

    c->button_channel_infos[MARU_CONTROLLER_BUTTON_SOUTH].name = "South";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_EAST].name = "East";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_WEST].name = "West";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_NORTH].name = "North";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_LB].name = "Left Bumper";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_RB].name = "Right Bumper";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_BACK].name = "Back";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_START].name = "Start";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_GUIDE].name = "Guide";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_L_THUMB].name = "Left Thumb";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_R_THUMB].name = "Right Thumb";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_DPAD_UP].name = "D-Pad Up";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_DPAD_RIGHT].name = "D-Pad Right";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_DPAD_DOWN].name = "D-Pad Down";
    c->button_channel_infos[MARU_CONTROLLER_BUTTON_DPAD_LEFT].name = "D-Pad Left";

    __block MARU_Controller_Cocoa *block_c = c;
    gc.extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad * _Nonnull gamepad, GCControllerElement * _Nonnull element) {
        if ([element isKindOfClass:[GCControllerButtonInput class]]) {
            GCControllerButtonInput *btn = (GCControllerButtonInput *)element;
            if (element == gamepad.buttonA) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_SOUTH, btn.isPressed);
            else if (element == gamepad.buttonB) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_EAST, btn.isPressed);
            else if (element == gamepad.buttonX) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_WEST, btn.isPressed);
            else if (element == gamepad.buttonY) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_NORTH, btn.isPressed);
            else if (element == gamepad.leftShoulder) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_LB, btn.isPressed);
            else if (element == gamepad.rightShoulder) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_RB, btn.isPressed);
            
            if (@available(macOS 10.14.1, *)) {
                if (element == gamepad.leftThumbstickButton) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_L_THUMB, btn.isPressed);
                else if (element == gamepad.rightThumbstickButton) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_R_THUMB, btn.isPressed);
            }
            
            if (@available(macOS 10.15, *)) {
                if (element == gamepad.buttonMenu) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_START, btn.isPressed);
            }
            
            if (@available(macOS 11.0, *)) {
                if (element == gamepad.buttonOptions) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_BACK, btn.isPressed);
                else if (element == gamepad.buttonHome) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_GUIDE, btn.isPressed);
            }
        }
        
        if (element == gamepad.dpad) {
            _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_UP, gamepad.dpad.up.isPressed);
            _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_DOWN, gamepad.dpad.down.isPressed);
            _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_LEFT, gamepad.dpad.left.isPressed);
            _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_RIGHT, gamepad.dpad.right.isPressed);
        }
        
        block_c->analog_states[MARU_CONTROLLER_ANALOG_LEFT_X].value = (MARU_Scalar)gamepad.leftThumbstick.xAxis.value;
        block_c->analog_states[MARU_CONTROLLER_ANALOG_LEFT_Y].value = (MARU_Scalar)gamepad.leftThumbstick.yAxis.value;
        block_c->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_X].value = (MARU_Scalar)gamepad.rightThumbstick.xAxis.value;
        block_c->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_Y].value = (MARU_Scalar)gamepad.rightThumbstick.yAxis.value;
        block_c->analog_states[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].value = (MARU_Scalar)gamepad.leftTrigger.value;
        block_c->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].value = (MARU_Scalar)gamepad.rightTrigger.value;
    };

    return c;
}

static void _maru_cocoa_ensure_observer(void) {
    if (!g_maru_cocoa.controller_observer) {
        g_maru_cocoa.controller_observer = [[MARU_ControllerObserver alloc] init];
        [[NSNotificationCenter defaultCenter] addObserver:g_maru_cocoa.controller_observer
                                                 selector:@selector(controllerDidConnect:)
                                                     name:GCControllerDidConnectNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:g_maru_cocoa.controller_observer
                                                 selector:@selector(controllerDidDisconnect:)
                                                     name:GCControllerDidDisconnectNotification
                                                   object:nil];
    }
}

void _maru_cocoa_sync_controllers(MARU_Context_Base *ctx_base) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)ctx_base;
    _maru_cocoa_ensure_observer();
    
    atomic_store(&ctx->controllers_dirty, false);
    
    NSArray *controllers = [GCController controllers];
    
    for (uint32_t i = 0; i < ctx->controller_cache_count; ++i) {
        ctx->controller_cache[i]->is_active = false;
    }
    
    for (GCController *gc in controllers) {
        if (!gc.extendedGamepad) continue;
        
        MARU_Controller_Cocoa *found = NULL;
        for (uint32_t i = 0; i < ctx->controller_cache_count; ++i) {
            MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)ctx->controller_cache[i];
            if (c->gc_controller == gc) {
                found = c;
                break;
            }
        }
        
        if (found) {
            found->base.is_active = true;
            found->base.pub.flags &= ~((uint64_t)MARU_CONTROLLER_STATE_LOST);
        } else {
            MARU_Controller_Cocoa *c = _maru_cocoa_create_controller(ctx, gc);
            if (!c) continue;
            
            if (ctx->controller_cache_count == ctx->controller_cache_capacity) {
                uint32_t old_cap = ctx->controller_cache_capacity;
                ctx->controller_cache_capacity = old_cap == 0 ? 4 : old_cap * 2;
                ctx->controller_cache = maru_context_realloc(&ctx->base, ctx->controller_cache, sizeof(MARU_Controller_Base *) * old_cap, sizeof(MARU_Controller_Base *) * ctx->controller_cache_capacity);
            }
            ctx->controller_cache[ctx->controller_cache_count++] = &c->base;
            
            MARU_Event event = {0};
            event.controller_connection.controller = (MARU_Controller *)c;
            event.controller_connection.connected = true;
            _maru_post_event_internal(&ctx->base, MARU_EVENT_CONTROLLER_CONNECTION_CHANGED, NULL, &event);
        }
    }
    
    // Mark inactive ones as lost and fire disconnect events if not already marked.
    for (uint32_t i = 0; i < ctx->controller_cache_count; ++i) {
        MARU_Controller_Base *c = ctx->controller_cache[i];
        if (!c->is_active && !(c->pub.flags & MARU_CONTROLLER_STATE_LOST)) {
            c->pub.flags |= MARU_CONTROLLER_STATE_LOST;
            MARU_Event event = {0};
            event.controller_connection.controller = (MARU_Controller *)c;
            event.controller_connection.connected = false;
            _maru_post_event_internal(&ctx->base, MARU_EVENT_CONTROLLER_CONNECTION_CHANGED, NULL, &event);
        }
    }
}

MARU_Status maru_getControllers_Cocoa(MARU_Context *context,
                                        MARU_ControllerList *out_list) {
    _maru_cocoa_sync_controllers((MARU_Context_Base *)context);
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
    out_list->controllers = (MARU_Controller **)ctx->controller_cache;
    out_list->count = ctx->controller_cache_count;
    
    return MARU_SUCCESS;
}

MARU_Status maru_retainController_Cocoa(MARU_Controller *controller) {
    MARU_Controller_Base *c = (MARU_Controller_Base *)controller;
    c->ref_count++;
    return MARU_SUCCESS;
}

MARU_Status maru_releaseController_Cocoa(MARU_Controller *controller) {
    MARU_Controller_Base *c = (MARU_Controller_Base *)controller;
    if (--c->ref_count == 0) {
        _maru_controller_free(c);
    }
    return MARU_SUCCESS;
}

MARU_Status maru_resetControllerMetrics_Cocoa(MARU_Controller *controller) {
    MARU_Controller_Base *c = (MARU_Controller_Base *)controller;
    memset(&c->metrics, 0, sizeof(MARU_ControllerMetrics));
    return MARU_SUCCESS;
}

MARU_Status maru_getControllerInfo_Cocoa(MARU_Controller *controller,
                                           MARU_ControllerInfo *out_info) {
    MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)controller;
    memset(out_info, 0, sizeof(MARU_ControllerInfo));
    out_info->name = [c->gc_controller.vendorName UTF8String];
    out_info->is_standardized = (c->gc_controller.extendedGamepad != nil);
    return MARU_SUCCESS;
}

MARU_Status maru_setControllerHapticLevels_Cocoa(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
    return MARU_FAILURE;
}
