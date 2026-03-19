// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <GameController/GameController.h>
#import <stdatomic.h>

typedef struct MARU_Controller_Cocoa {
  MARU_Controller_Base base;
  GCController *gc_controller;
  
  MARU_ChannelInfo *analog_channel_infos;
  MARU_AnalogInputState *analog_states;
  MARU_ChannelInfo *button_channel_infos;
  MARU_ButtonState8 *button_states;
  MARU_ChannelInfo *haptic_channel_infos;

  CHHapticEngine *haptic_engine;
  id<CHHapticAdvancedPatternPlayer> low_freq_player;
  id<CHHapticAdvancedPatternPlayer> high_freq_player;
} MARU_Controller_Cocoa;

static void _maru_cocoa_handle_controller_connected(MARU_Context_Cocoa *ctx,
                                                    GCController *gc);
static void _maru_cocoa_handle_controller_disconnected(MARU_Context_Cocoa *ctx,
                                                       GCController *gc);

@interface MARU_ControllerObserver : NSObject
@property (nonatomic, assign) MARU_Context_Cocoa *context;
@end

@implementation MARU_ControllerObserver

- (void)handleControllerConnectedObject:(id)object {
    MARU_Context_Cocoa *ctx = self.context;
    if (!ctx) return;

    GCController *gc = [object isKindOfClass:[GCController class]]
                           ? (GCController *)object
                           : nil;
    if (gc) {
        _maru_cocoa_handle_controller_connected(ctx, gc);
    } else {
        atomic_store(&ctx->controllers_dirty, true);
    }
}

- (void)handleControllerDisconnectedObject:(id)object {
    MARU_Context_Cocoa *ctx = self.context;
    if (!ctx) return;

    GCController *gc = [object isKindOfClass:[GCController class]]
                           ? (GCController *)object
                           : nil;
    if (gc) {
        _maru_cocoa_handle_controller_disconnected(ctx, gc);
    } else {
        atomic_store(&ctx->controllers_dirty, true);
    }
}

- (void)controllerDidConnect:(NSNotification *)notification {
    if ([NSThread isMainThread]) {
        [self handleControllerConnectedObject:notification.object];
    } else {
        [self performSelectorOnMainThread:@selector(handleControllerConnectedObject:)
                               withObject:notification.object
                            waitUntilDone:NO];
    }
}

- (void)controllerDidDisconnect:(NSNotification *)notification {
    if ([NSThread isMainThread]) {
        [self handleControllerDisconnectedObject:notification.object];
    } else {
        [self performSelectorOnMainThread:@selector(handleControllerDisconnectedObject:)
                               withObject:notification.object
                            waitUntilDone:NO];
    }
}

@end

void _maru_controller_free(MARU_Controller_Base *controller) {
    MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)controller;
    MARU_Context_Base *ctx = controller->ctx_base;
    
    if (c->gc_controller.extendedGamepad) {
        c->gc_controller.extendedGamepad.valueChangedHandler = nil;
    }
    if (c->gc_controller.microGamepad) {
        c->gc_controller.microGamepad.valueChangedHandler = nil;
    }
    if (c->gc_controller) {
        [c->gc_controller release];
    }
    
    if (c->haptic_engine) {
        [c->low_freq_player release];
        [c->high_freq_player release];
        [c->haptic_engine stopWithCompletionHandler:nil];
        [c->haptic_engine release];
    }

    maru_context_free(ctx, c->analog_channel_infos);
    maru_context_free(ctx, c->analog_states);
    maru_context_free(ctx, c->button_channel_infos);
    maru_context_free(ctx, c->button_states);
    maru_context_free(ctx, c->haptic_channel_infos);
    maru_context_free(ctx, c);
}

static void _maru_cocoa_dispatch_button_event(MARU_Controller_Cocoa *c, uint32_t button_id, bool pressed) {
    MARU_ButtonState state = pressed ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (c->button_states[button_id] == (MARU_ButtonState8)state) return;
    
    c->button_states[button_id] = (MARU_ButtonState8)state;
    
    MARU_Event event = {0};
    event.controller_button_changed.controller = (MARU_Controller *)c;
    event.controller_button_changed.button_id = button_id;
    event.controller_button_changed.state = state;
    _maru_post_event_internal(c->base.ctx_base, MARU_EVENT_CONTROLLER_BUTTON_CHANGED, NULL, &event);
}

static void _maru_cocoa_release_queued_controller(MARU_Context_Base *ctx_base,
                                                  void *userdata) {
    (void)ctx_base;
    if (!userdata) {
        return;
    }
    maru_releaseController_Cocoa((MARU_Controller *)userdata);
}

static int32_t _maru_cocoa_find_controller_index(MARU_Context_Cocoa *ctx,
                                                 GCController *gc) {
    if (!ctx || !gc) {
        return -1;
    }

    for (uint32_t i = 0; i < ctx->controller_cache_count; ++i) {
        MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)ctx->controller_cache[i];
        if (c->gc_controller == gc) {
            return (int32_t)i;
        }
    }
    return -1;
}

static bool _maru_cocoa_ensure_controller_capacity(MARU_Context_Cocoa *ctx) {
    if (!ctx) {
        return false;
    }

    if (ctx->controller_cache_count < ctx->controller_cache_capacity) {
        return true;
    }

    const uint32_t old_cap = ctx->controller_cache_capacity;
    const uint32_t new_cap = old_cap == 0 ? 4 : old_cap * 2;
    MARU_Controller_Base **new_cache =
        maru_context_realloc(&ctx->base,
                             ctx->controller_cache,
                             sizeof(MARU_Controller_Base *) * old_cap,
                             sizeof(MARU_Controller_Base *) * new_cap);
    if (!new_cache) {
        return false;
    }

    ctx->controller_cache = new_cache;
    ctx->controller_cache_capacity = new_cap;
    return true;
}

static void _maru_cocoa_queue_controller_changed_event(MARU_Context_Cocoa *ctx,
                                                       MARU_Controller_Cocoa *controller,
                                                       bool connected,
                                                       bool release_after_dispatch) {
    if (!ctx || !controller) {
        return;
    }

    MARU_Event event = {0};
    event.controller_changed.controller = (MARU_Controller *)controller;
    event.controller_changed.connected = connected;

    if (release_after_dispatch) {
        (void)_maru_post_event_internal_owned(&ctx->base,
                                              MARU_EVENT_CONTROLLER_CHANGED,
                                              NULL,
                                              &event,
                                              _maru_cocoa_release_queued_controller,
                                              controller);
    } else {
        (void)_maru_post_event_internal(&ctx->base,
                                        MARU_EVENT_CONTROLLER_CHANGED,
                                        NULL,
                                        &event);
    }
}

static MARU_Controller_Cocoa *_maru_cocoa_create_controller(MARU_Context_Cocoa *ctx, GCController *gc) {
    MARU_Controller_Cocoa *c = maru_context_alloc(&ctx->base, sizeof(MARU_Controller_Cocoa));
    if (!c) return NULL;
    memset(c, 0, sizeof(MARU_Controller_Cocoa));

    c->base.ctx_base = &ctx->base;
    c->base.pub.context = (MARU_Context *)ctx;
    c->base.is_active = true;
    atomic_init(&c->base.ref_count, 1u);
    c->gc_controller = [gc retain];
    
    c->base.pub.analog_count = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;
    c->base.pub.button_count = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;
    
    c->analog_channel_infos =
        maru_context_alloc(&ctx->base, sizeof(MARU_ChannelInfo) *
                                           c->base.pub.analog_count);
    c->analog_states =
        maru_context_alloc(&ctx->base, sizeof(MARU_AnalogInputState) *
                                           c->base.pub.analog_count);
    c->button_channel_infos =
        maru_context_alloc(&ctx->base, sizeof(MARU_ChannelInfo) *
                                           c->base.pub.button_count);
    c->button_states =
        maru_context_alloc(&ctx->base, sizeof(MARU_ButtonState8) *
                                           c->base.pub.button_count);
    if (!c->analog_channel_infos || !c->analog_states ||
        !c->button_channel_infos || !c->button_states) {
        _maru_controller_free(&c->base);
        return NULL;
    }
    memset(c->analog_channel_infos, 0, sizeof(MARU_ChannelInfo) * c->base.pub.analog_count);
    memset(c->analog_states, 0, sizeof(MARU_AnalogInputState) * c->base.pub.analog_count);
    memset(c->button_channel_infos, 0, sizeof(MARU_ChannelInfo) * c->base.pub.button_count);
    memset(c->button_states, 0, sizeof(MARU_ButtonState8) * c->base.pub.button_count);
    
    c->base.pub.analog_channels = c->analog_channel_infos;
    c->base.pub.analogs = c->analog_states;
    c->base.pub.button_channels = c->button_channel_infos;
    c->base.pub.buttons = c->button_states;

    if (gc.vendorName) {
        c->base.pub.name = [gc.vendorName UTF8String];
    } else {
        c->base.pub.name = "Unknown Game Controller";
    }
    c->base.pub.is_standardized = (gc.extendedGamepad != nil);

    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_X].name = "Left X";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_X].min_value = -1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_X].max_value = 1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_Y].name = "Left Y";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_Y].min_value = -1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_Y].max_value = 1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_X].name = "Right X";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_X].min_value = -1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_X].max_value = 1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_Y].name = "Right Y";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_Y].min_value = -1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_Y].max_value = 1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].name = "Left Trigger";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].min_value = 0.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].max_value = 1.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].name = "Right Trigger";
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].min_value = 0.0f;
    c->analog_channel_infos[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].max_value = 1.0f;

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
    for (uint32_t i = 0; i < c->base.pub.button_count; ++i) {
        c->button_channel_infos[i].min_value = 0.0f;
        c->button_channel_infos[i].max_value = 1.0f;
    }

    if (@available(macOS 11.0, *)) {
        if (gc.haptics) {
            c->base.pub.haptic_count = MARU_CONTROLLER_HAPTIC_STANDARD_COUNT;
            c->haptic_channel_infos =
                maru_context_alloc(&ctx->base, sizeof(MARU_ChannelInfo) *
                                               c->base.pub.haptic_count);
            if (c->haptic_channel_infos) {
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_LOW_FREQ].name = "Low Frequency";
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_LOW_FREQ].min_value = 0.0f;
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_LOW_FREQ].max_value = 1.0f;
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_HIGH_FREQ].name = "High Frequency";
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_HIGH_FREQ].min_value = 0.0f;
                c->haptic_channel_infos[MARU_CONTROLLER_HAPTIC_HIGH_FREQ].max_value = 1.0f;
                c->base.pub.haptic_channels = c->haptic_channel_infos;
            }
        }
    }

    __block MARU_Controller_Cocoa *block_c = c;
    if (gc.extendedGamepad) {
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
    } else if (gc.microGamepad) {
        gc.microGamepad.valueChangedHandler = ^(GCMicroGamepad * _Nonnull gamepad, GCControllerElement * _Nonnull element) {
            if ([element isKindOfClass:[GCControllerButtonInput class]]) {
                GCControllerButtonInput *btn = (GCControllerButtonInput *)element;
                if (element == gamepad.buttonA) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_SOUTH, btn.isPressed);
                else if (element == gamepad.buttonX) _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_WEST, btn.isPressed);
            }
            if (element == gamepad.dpad) {
                _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_UP, gamepad.dpad.up.isPressed);
                _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_DOWN, gamepad.dpad.down.isPressed);
                _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_LEFT, gamepad.dpad.left.isPressed);
                _maru_cocoa_dispatch_button_event(block_c, MARU_CONTROLLER_BUTTON_DPAD_RIGHT, gamepad.dpad.right.isPressed);
            }
        };
    }

    return c;
}

static void _maru_cocoa_handle_controller_connected(MARU_Context_Cocoa *ctx,
                                                    GCController *gc) {
    if (!ctx || !gc) {
        return;
    }

    const int32_t existing_index = _maru_cocoa_find_controller_index(ctx, gc);
    if (existing_index >= 0) {
        MARU_Controller_Cocoa *existing =
            (MARU_Controller_Cocoa *)ctx->controller_cache[existing_index];
        existing->base.is_active = true;
        existing->base.pub.flags &= ~((uint64_t)MARU_CONTROLLER_STATE_LOST);
        return;
    }

    if (!_maru_cocoa_ensure_controller_capacity(ctx)) {
        atomic_store(&ctx->controllers_dirty, true);
        return;
    }

    MARU_Controller_Cocoa *c = _maru_cocoa_create_controller(ctx, gc);
    if (!c) {
        atomic_store(&ctx->controllers_dirty, true);
        return;
    }

    ctx->controller_cache[ctx->controller_cache_count++] = &c->base;
    ctx->controller_snapshot_dirty = true;
    _maru_cocoa_queue_controller_changed_event(ctx, c, true, false);
}

static void _maru_cocoa_handle_controller_disconnected(MARU_Context_Cocoa *ctx,
                                                       GCController *gc) {
    if (!ctx || !gc) {
        return;
    }

    const int32_t index = _maru_cocoa_find_controller_index(ctx, gc);
    if (index < 0) {
        atomic_store(&ctx->controllers_dirty, true);
        return;
    }

    MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)ctx->controller_cache[index];
    const uint32_t last = ctx->controller_cache_count - 1u;
    for (uint32_t i = (uint32_t)index; i < last; ++i) {
        ctx->controller_cache[i] = ctx->controller_cache[i + 1u];
    }
    ctx->controller_cache_count = last;
    ctx->controller_snapshot_dirty = true;

    c->base.is_active = false;
    c->base.pub.flags |= MARU_CONTROLLER_STATE_LOST;
    _maru_cocoa_queue_controller_changed_event(ctx, c, false, true);
}

static void _maru_cocoa_ensure_observer(MARU_Context_Cocoa *ctx) {
    if (!ctx || ctx->controller_observer) {
        return;
    }

    MARU_ControllerObserver *observer = [[MARU_ControllerObserver alloc] init];
    observer.context = ctx;
    [[NSNotificationCenter defaultCenter] addObserver:observer
                                             selector:@selector(controllerDidConnect:)
                                                 name:GCControllerDidConnectNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:observer
                                             selector:@selector(controllerDidDisconnect:)
                                                 name:GCControllerDidDisconnectNotification
                                               object:nil];
    ctx->controller_observer = observer;
}

void _maru_cocoa_cleanup_controller_observer(MARU_Context_Cocoa *ctx) {
    if (!ctx || !ctx->controller_observer) {
        return;
    }
    [[NSNotificationCenter defaultCenter] removeObserver:ctx->controller_observer];
    [ctx->controller_observer release];
    ctx->controller_observer = nil;
}

void _maru_cocoa_sync_controllers(MARU_Context_Base *ctx_base) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)ctx_base;
    _maru_cocoa_ensure_observer(ctx);
    
    atomic_store(&ctx->controllers_dirty, false);
    
    NSArray *controllers = [GCController controllers];

    for (int32_t i = (int32_t)ctx->controller_cache_count - 1; i >= 0; --i) {
        MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)ctx->controller_cache[i];
        bool still_present = false;
        for (GCController *gc in controllers) {
            if (c->gc_controller == gc) {
                still_present = true;
                break;
            }
        }

        if (!still_present) {
            _maru_cocoa_handle_controller_disconnected(ctx, c->gc_controller);
        }
    }

    for (GCController *gc in controllers) {
        _maru_cocoa_handle_controller_connected(ctx, gc);
    }
}

MARU_Status maru_getControllers_Cocoa(const MARU_Context *context,
                                      MARU_ControllerList *out_list) {
    MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
    if (ctx->controller_snapshot_dirty) {
        ctx->controller_snapshot_count = ctx->controller_cache_count;
        ctx->controller_snapshot_dirty = false;
    }
    out_list->controllers = (MARU_Controller **)ctx->controller_cache;
    out_list->count = ctx->controller_snapshot_count;
    
    return MARU_SUCCESS;
}

void maru_retainController_Cocoa(MARU_Controller *controller) {
    MARU_Controller_Base *c = (MARU_Controller_Base *)controller;
    atomic_fetch_add_explicit(&c->ref_count, 1u, memory_order_relaxed);
}

void maru_releaseController_Cocoa(MARU_Controller *controller) {
    MARU_Controller_Base *c = (MARU_Controller_Base *)controller;
    uint32_t current = atomic_load_explicit(&c->ref_count, memory_order_acquire);
    while (current > 0u) {
        if (atomic_compare_exchange_weak_explicit(&c->ref_count,
                                                  &current,
                                                  current - 1u,
                                                  memory_order_acq_rel,
                                                  memory_order_acquire)) {
            if (current == 1u) {
                _maru_controller_free(c);
            }
            return;
        }
    }
}

static id<CHHapticAdvancedPatternPlayer> _maru_cocoa_create_haptic_player(CHHapticEngine *engine) {
    if (!engine) return nil;
    
    NSDictionary *hapticDict = @{
        CHHapticPatternKeyVersion: @1.0,
        CHHapticPatternKeyPattern: @[
            @{
                CHHapticPatternKeyEvent: @{
                    CHHapticPatternKeyEventType: CHHapticEventTypeHapticContinuous,
                    CHHapticPatternKeyTime: @0.0,
                    CHHapticPatternKeyEventDuration: @(GCHapticDurationInfinite),
                    CHHapticPatternKeyEventParameters: @[
                        @{ CHHapticPatternKeyParameterID: CHHapticEventParameterIDHapticIntensity, CHHapticPatternKeyParameterValue: @1.0 },
                        @{ CHHapticPatternKeyParameterID: CHHapticEventParameterIDHapticSharpness, CHHapticPatternKeyParameterValue: @0.5 }
                    ]
                }
            }
        ]
    };
    
    NSError *error = nil;
    CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithDictionary:hapticDict error:&error];
    if (!pattern) return nil;
    
    id<CHHapticAdvancedPatternPlayer> player = [engine createAdvancedPlayerWithPattern:pattern error:&error];
    [pattern release];
    
    if (player) {
        [player startAtTime:0 error:nil];
    }
    return [player retain];
}

MARU_Status maru_setControllerHapticLevels_Cocoa(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
    MARU_Controller_Cocoa *c = (MARU_Controller_Cocoa *)controller;
    
    if (@available(macOS 11.0, *)) {
        if (!c->gc_controller.haptics) return MARU_FAILURE;
        
        if (!c->haptic_engine) {
            c->haptic_engine = [[c->gc_controller.haptics createEngineWithLocality:GCHapticsLocalityDefault] retain];
            if (!c->haptic_engine) return MARU_FAILURE;
            
            NSError *error = nil;
            [c->haptic_engine startAndReturnError:&error];
            if (error) {
                [c->haptic_engine release];
                c->haptic_engine = nil;
                return MARU_FAILURE;
            }
            
            c->low_freq_player = _maru_cocoa_create_haptic_player(c->haptic_engine);
            c->high_freq_player = _maru_cocoa_create_haptic_player(c->haptic_engine);
        }
        
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t haptic_id = first_haptic + i;
            MARU_Scalar intensity = intensities[i];
            
            id<CHHapticAdvancedPatternPlayer> player = nil;
            if (haptic_id == MARU_CONTROLLER_HAPTIC_LOW_FREQ) player = c->low_freq_player;
            else if (haptic_id == MARU_CONTROLLER_HAPTIC_HIGH_FREQ) player = c->high_freq_player;
            
            if (player) {
                CHHapticDynamicParameter *param = [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl 
                                                                                                 value:(float)intensity 
                                                                                          relativeTime:0];
                [player sendParameters:@[param] atTime:0 error:nil];
                [param release];
            }
        }
        return MARU_SUCCESS;
    }
    
    return MARU_FAILURE;
}
