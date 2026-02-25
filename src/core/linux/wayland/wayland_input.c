#define _GNU_SOURCE
#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include <linux/input-event-codes.h>

#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <linux/memfd.h>
#include <unistd.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1
#endif

#define WL_SEAT_CAPABILITY_POINTER 1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_SEAT_CAPABILITY_TOUCH 4

#define WL_KEYBOARD_KEY_STATE_PRESSED 1
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 1
#define WL_POINTER_BUTTON_STATE_PRESSED 1
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1

static MARU_ModifierFlags _get_modifiers(MARU_Context_WL *ctx);

static MARU_Window_WL *_maru_wayland_resolve_registered_window(
    MARU_Context_WL *ctx, const MARU_Window *candidate) {
    if (!ctx || !candidate) {
        return NULL;
    }

    for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
        if ((const MARU_Window *)it == candidate) {
            return (MARU_Window_WL *)it;
        }
    }

    return NULL;
}

static bool _maru_wayland_map_native_mouse_button(uint32_t native_code,
                                                  uint32_t *out_channel) {
    switch (native_code) {
        case BTN_LEFT: *out_channel = 0; return true;
        case BTN_RIGHT: *out_channel = 1; return true;
        case BTN_MIDDLE: *out_channel = 2; return true;
        case BTN_SIDE: *out_channel = 3; return true;
        case BTN_EXTRA: *out_channel = 4; return true;
        case BTN_FORWARD: *out_channel = 5; return true;
        case BTN_BACK: *out_channel = 6; return true;
        case BTN_TASK: *out_channel = 7; return true;
        default: return false;
    }
}

static void _maru_wayland_report_unknown_mouse_button_once(MARU_Context_WL *ctx,
                                                           uint32_t native_code) {
    for (uint32_t i = 0; i < ctx->unknown_mouse_buttons.count; ++i) {
        if (ctx->unknown_mouse_buttons.codes[i] == native_code) {
            return;
        }
    }

    if (ctx->unknown_mouse_buttons.count < (uint32_t)(sizeof(ctx->unknown_mouse_buttons.codes) /
                                                      sizeof(ctx->unknown_mouse_buttons.codes[0]))) {
        ctx->unknown_mouse_buttons.codes[ctx->unknown_mouse_buttons.count++] = native_code;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Unknown Wayland mouse button code dropped: %u",
             native_code);
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED, msg);
}

static void _maru_wayland_replace_pending_utf8(MARU_Window_WL *window, char **slot,
                                               const char *value) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    if (*slot) {
        maru_context_free(&ctx->base, *slot);
        *slot = NULL;
    }

    if (!value) {
        return;
    }

    size_t len = strlen(value);
    char *copy = (char *)maru_context_alloc(&ctx->base, len + 1);
    if (!copy) {
        return;
    }
    memcpy(copy, value, len + 1);
    *slot = copy;
}

void _maru_wayland_clear_text_input_pending(MARU_Window_WL *window) {
    if (!window) {
        return;
    }
    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.preedit_utf8, NULL);
    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.commit_utf8, NULL);
    memset(&window->text_input_pending, 0, sizeof(window->text_input_pending));
}

static const char *_maru_cursor_shape_to_name(MARU_CursorShape shape) {
    switch (shape) {
        case MARU_CURSOR_SHAPE_DEFAULT: return "left_ptr";
        case MARU_CURSOR_SHAPE_HELP: return "question_arrow";
        case MARU_CURSOR_SHAPE_HAND: return "hand1";
        case MARU_CURSOR_SHAPE_WAIT: return "watch";
        case MARU_CURSOR_SHAPE_CROSSHAIR: return "crosshair";
        case MARU_CURSOR_SHAPE_TEXT: return "xterm";
        case MARU_CURSOR_SHAPE_MOVE: return "move";
        case MARU_CURSOR_SHAPE_NOT_ALLOWED: return "forbidden";
        case MARU_CURSOR_SHAPE_EW_RESIZE: return "sb_h_double_arrow";
        case MARU_CURSOR_SHAPE_NS_RESIZE: return "sb_v_double_arrow";
        case MARU_CURSOR_SHAPE_NESW_RESIZE: return "fd_double_arrow";
        case MARU_CURSOR_SHAPE_NWSE_RESIZE: return "bd_double_arrow";
        default: return "left_ptr";
    }
}

static bool _maru_cursor_shape_to_protocol_shape(MARU_CursorShape shape, uint32_t *out_shape) {
    switch (shape) {
        case MARU_CURSOR_SHAPE_DEFAULT:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
            return true;
        case MARU_CURSOR_SHAPE_HELP:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP;
            return true;
        case MARU_CURSOR_SHAPE_HAND:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
            return true;
        case MARU_CURSOR_SHAPE_WAIT:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
            return true;
        case MARU_CURSOR_SHAPE_CROSSHAIR:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR;
            return true;
        case MARU_CURSOR_SHAPE_TEXT:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
            return true;
        case MARU_CURSOR_SHAPE_MOVE:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE;
            return true;
        case MARU_CURSOR_SHAPE_NOT_ALLOWED:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
            return true;
        case MARU_CURSOR_SHAPE_EW_RESIZE:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
            return true;
        case MARU_CURSOR_SHAPE_NS_RESIZE:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
            return true;
        case MARU_CURSOR_SHAPE_NESW_RESIZE:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE;
            return true;
        case MARU_CURSOR_SHAPE_NWSE_RESIZE:
            *out_shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE;
            return true;
        default:
            return false;
    }
}

static void _maru_wayland_clear_cursor_animation(MARU_Context_WL *ctx) {
    memset(&ctx->cursor_animation, 0, sizeof(ctx->cursor_animation));
}

static bool _maru_wayland_ensure_cursor_theme(MARU_Context_WL *ctx) {
    if (ctx->wl.cursor_theme) {
        return true;
    }
    if (!ctx->protocols.wl_shm) {
        return false;
    }
    const char *size_env = getenv("XCURSOR_SIZE");
    int size = 24;
    if (size_env) size = atoi(size_env);
    if (size <= 0) size = 24;
    ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, NULL, size, ctx->protocols.wl_shm);
    return ctx->wl.cursor_theme != NULL;
}

static bool _maru_wayland_commit_cursor_buffer(MARU_Context_WL *ctx, uint32_t serial,
                                               struct wl_buffer *buffer,
                                               int32_t hotspot_x, int32_t hotspot_y,
                                               int32_t width, int32_t height) {
    if (!ctx->wl.pointer || !buffer) {
        return false;
    }
    if (!ctx->wl.cursor_surface) {
        ctx->wl.cursor_surface = maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);
        if (!ctx->wl.cursor_surface) {
            return false;
        }
    }

    maru_wl_surface_attach(ctx, ctx->wl.cursor_surface, buffer, 0, 0);
    maru_wl_surface_damage(ctx, ctx->wl.cursor_surface, 0, 0, width, height);
    maru_wl_surface_commit(ctx, ctx->wl.cursor_surface);
    maru_wl_pointer_set_cursor(ctx, ctx->wl.pointer, serial, ctx->wl.cursor_surface, hotspot_x, hotspot_y);
    return true;
}

void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial) {
    if (!ctx->wl.pointer) return;
    if (!window) return;

    MARU_Cursor *cursor_handle = (MARU_Cursor *)window->base.pub.current_cursor;
    MARU_CursorMode mode = window->base.pub.cursor_mode;

    if (mode == MARU_CURSOR_HIDDEN || mode == MARU_CURSOR_LOCKED) {
        maru_wl_pointer_set_cursor(ctx, ctx->wl.pointer, serial, NULL, 0, 0);
        _maru_wayland_clear_cursor_animation(ctx);
        return;
    }

    MARU_Cursor_WL *cursor = NULL;
    struct wl_cursor *wl_cursor = NULL;
    MARU_CursorShape system_shape = MARU_CURSOR_SHAPE_DEFAULT;
    bool system_cursor = (cursor_handle == NULL);

    if (cursor_handle) {
        cursor = (MARU_Cursor_WL *)cursor_handle;
        wl_cursor = cursor->wl_cursor;
        system_cursor = (cursor->base.pub.flags & MARU_CURSOR_FLAG_SYSTEM) != 0;
        system_shape = cursor->cursor_shape;
    }

    if (cursor && cursor->frame_count > 0) {
        const MARU_WaylandCursorFrame *frame = &cursor->frames[0];
        if (_maru_wayland_commit_cursor_buffer(ctx, serial, frame->buffer, frame->hotspot_x,
                                               frame->hotspot_y, (int32_t)frame->width,
                                               (int32_t)frame->height)) {
            _maru_wayland_clear_cursor_animation(ctx);
            if (cursor->frame_count > 1) {
                uint32_t delay_ms = frame->delay_ms == 0 ? 1u : frame->delay_ms;
                uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
                if (now_ns != 0) {
                    ctx->cursor_animation.window = window;
                    ctx->cursor_animation.cursor = cursor;
                    ctx->cursor_animation.theme_cursor = NULL;
                    ctx->cursor_animation.frame_index = 0;
                    ctx->cursor_animation.serial = serial;
                    ctx->cursor_animation.next_frame_ns = now_ns + ((uint64_t)delay_ms * 1000000ull);
                    ctx->cursor_animation.active = true;
                }
            }
        }
        return;
    }

    if (ctx->wl.cursor_shape_device && system_cursor) {
        uint32_t shape = 0;
        if (_maru_cursor_shape_to_protocol_shape(system_shape, &shape)) {
            maru_wp_cursor_shape_device_v1_set_shape(ctx, ctx->wl.cursor_shape_device, serial, shape);
            _maru_wayland_clear_cursor_animation(ctx);
            return;
        }
    }

    if (!wl_cursor && _maru_wayland_ensure_cursor_theme(ctx)) {
        const char *shape_name = _maru_cursor_shape_to_name(system_shape);
        wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, shape_name);
    }
    if (!wl_cursor || wl_cursor->image_count == 0) {
        return;
    }

    struct wl_cursor_image *image = wl_cursor->images[0];
    struct wl_buffer *buffer = maru_wl_cursor_image_get_buffer(ctx, image);
    if (!buffer) {
        return;
    }
    if (!_maru_wayland_commit_cursor_buffer(ctx, serial, buffer, (int32_t)image->hotspot_x,
                                            (int32_t)image->hotspot_y, (int32_t)image->width,
                                            (int32_t)image->height)) {
        return;
    }

    _maru_wayland_clear_cursor_animation(ctx);
    if (wl_cursor->image_count > 1) {
        uint32_t delay_ms = image->delay == 0 ? 1u : image->delay;
        uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
        if (now_ns != 0) {
            ctx->cursor_animation.window = window;
            ctx->cursor_animation.cursor = NULL;
            ctx->cursor_animation.theme_cursor = wl_cursor;
            ctx->cursor_animation.frame_index = 0;
            ctx->cursor_animation.serial = serial;
            ctx->cursor_animation.next_frame_ns = now_ns + ((uint64_t)delay_ms * 1000000ull);
            ctx->cursor_animation.active = true;
        }
    }
}

uint64_t _maru_wayland_cursor_next_frame_ns(const MARU_Context_WL *ctx) {
    if (!ctx->cursor_animation.active) {
        return 0;
    }
    return ctx->cursor_animation.next_frame_ns;
}

void _maru_wayland_advance_cursor_animation(MARU_Context_WL *ctx) {
    if (!ctx->cursor_animation.active || !ctx->wl.pointer) {
        return;
    }
    if (!ctx->linux_common.pointer.focused_window ||
        ctx->linux_common.pointer.focused_window != (MARU_Window *)ctx->cursor_animation.window) {
        _maru_wayland_clear_cursor_animation(ctx);
        return;
    }

    MARU_Window_WL *window = ctx->cursor_animation.window;
    if (!window || window->base.pub.cursor_mode != MARU_CURSOR_NORMAL) {
        _maru_wayland_clear_cursor_animation(ctx);
        return;
    }

    uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
    if (now_ns == 0 || now_ns < ctx->cursor_animation.next_frame_ns) {
        return;
    }

    const uint32_t max_advance_per_pump = 16;
    uint32_t advanced = 0;
    while (ctx->cursor_animation.active &&
           now_ns >= ctx->cursor_animation.next_frame_ns &&
           advanced < max_advance_per_pump) {
        uint32_t delay_ms = 0;
        bool committed = false;

        if (ctx->cursor_animation.cursor) {
            MARU_Cursor_WL *cursor = ctx->cursor_animation.cursor;
            if (!cursor->frames || cursor->frame_count <= 1) {
                _maru_wayland_clear_cursor_animation(ctx);
                break;
            }
            ctx->cursor_animation.frame_index =
                (ctx->cursor_animation.frame_index + 1u) % cursor->frame_count;
            const MARU_WaylandCursorFrame *frame =
                &cursor->frames[ctx->cursor_animation.frame_index];
            committed = _maru_wayland_commit_cursor_buffer(
                ctx, ctx->linux_common.pointer.enter_serial, frame->buffer, frame->hotspot_x,
                frame->hotspot_y, (int32_t)frame->width, (int32_t)frame->height);
            delay_ms = frame->delay_ms;
        } else if (ctx->cursor_animation.theme_cursor) {
            struct wl_cursor *wl_cursor = ctx->cursor_animation.theme_cursor;
            if (wl_cursor->image_count <= 1) {
                _maru_wayland_clear_cursor_animation(ctx);
                break;
            }
            ctx->cursor_animation.frame_index =
                (ctx->cursor_animation.frame_index + 1u) % wl_cursor->image_count;
            struct wl_cursor_image *image = wl_cursor->images[ctx->cursor_animation.frame_index];
            struct wl_buffer *buffer = maru_wl_cursor_image_get_buffer(ctx, image);
            if (buffer) {
                committed = _maru_wayland_commit_cursor_buffer(
                    ctx, ctx->linux_common.pointer.enter_serial, buffer,
                    (int32_t)image->hotspot_x, (int32_t)image->hotspot_y,
                    (int32_t)image->width, (int32_t)image->height);
            }
            delay_ms = image->delay;
        } else {
            _maru_wayland_clear_cursor_animation(ctx);
            break;
        }

        if (!committed) {
            _maru_wayland_clear_cursor_animation(ctx);
            break;
        }

        if (delay_ms == 0) {
            delay_ms = 1;
        }
        ctx->cursor_animation.next_frame_ns += ((uint64_t)delay_ms * 1000000ull);
        advanced++;
    }

    if (ctx->cursor_animation.active &&
        advanced == max_advance_per_pump &&
        now_ns >= ctx->cursor_animation.next_frame_ns) {
        ctx->cursor_animation.next_frame_ns = now_ns + 1000000ull;
    }
}

static void _pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, struct wl_surface *surface,
                                  wl_fixed_t sx, wl_fixed_t sy) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    if (!surface) return;

    MARU_Window_WL *window =
        (MARU_Window_WL *)ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *)surface);
    window = _maru_wayland_resolve_registered_window(ctx, (MARU_Window *)window);

    if (window) {
        ctx->linux_common.pointer.focused_window = (MARU_Window *)window;
        ctx->linux_common.pointer.x = wl_fixed_to_double(sx);
        ctx->linux_common.pointer.y = wl_fixed_to_double(sy);
        ctx->linux_common.pointer.enter_serial = serial;
        _maru_wayland_update_cursor(ctx, window, serial);
    } else {
        ctx->linux_common.pointer.focused_window = NULL;
    }
}

static void _pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, struct wl_surface *surface) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    ctx->linux_common.pointer.focused_window = NULL;
    _maru_wayland_clear_cursor_animation(ctx);
}

static void _pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                   uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = _maru_wayland_resolve_registered_window(
        ctx, ctx->linux_common.pointer.focused_window);

    if (!window) {
        ctx->linux_common.pointer.focused_window = NULL;
        return;
    }

    MARU_Event evt = {0};
    evt.mouse_motion.position.x = wl_fixed_to_double(sx);
    evt.mouse_motion.position.y = wl_fixed_to_double(sy);
    
    evt.mouse_motion.delta.x = evt.mouse_motion.position.x - ctx->linux_common.pointer.x;
    evt.mouse_motion.delta.y = evt.mouse_motion.position.y - ctx->linux_common.pointer.y;
    evt.mouse_motion.raw_delta.x = 0.0;
    evt.mouse_motion.raw_delta.y = 0.0;
    evt.mouse_motion.modifiers = _get_modifiers(ctx);
    
    ctx->linux_common.pointer.x = evt.mouse_motion.position.x;
    ctx->linux_common.pointer.y = evt.mouse_motion.position.y;
    
    _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED, (MARU_Window *)window, &evt);
}

static void _pointer_handle_button(void *data, struct wl_pointer *pointer,
                                   uint32_t serial, uint32_t time, uint32_t button,
                                   uint32_t state) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = _maru_wayland_resolve_registered_window(
        ctx, ctx->linux_common.pointer.focused_window);

    if (!window) {
        ctx->linux_common.pointer.focused_window = NULL;
        return;
    }
    uint32_t channel = 0;
    if (!_maru_wayland_map_native_mouse_button(button, &channel)) {
        _maru_wayland_report_unknown_mouse_button_once(ctx, button);
        return;
    }

    if (channel >= window->base.pub.mouse_button_count ||
        !window->base.mouse_button_states) {
        _maru_wayland_report_unknown_mouse_button_once(ctx, button);
        return;
    }

    MARU_ButtonState btn_state = (state == WL_POINTER_BUTTON_STATE_PRESSED) ?
                                 MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (btn_state == MARU_BUTTON_STATE_PRESSED) {
        ctx->clipboard.serial = serial;
    }
    window->base.mouse_button_states[channel] = (MARU_ButtonState8)btn_state;
    if (ctx->base.mouse_button_states && channel < ctx->base.pub.mouse_button_count) {
        ctx->base.mouse_button_states[channel] = (MARU_ButtonState8)btn_state;
    }

    MARU_Event evt = {0};
    evt.mouse_button.button_id = channel;
    evt.mouse_button.state = btn_state;
    evt.mouse_button.modifiers = _get_modifiers(ctx);
    _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED,
                         (MARU_Window *)window, &evt);
}

static void _pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                 uint32_t time, uint32_t axis, wl_fixed_t value) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        ctx->scroll.delta.y -= wl_fixed_to_double(value);
    } else {
        ctx->scroll.delta.x -= wl_fixed_to_double(value);
    }
    ctx->scroll.active = true;
}

static void _pointer_handle_frame(void *data, struct wl_pointer *wl_pointer) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = _maru_wayland_resolve_registered_window(
        ctx, ctx->linux_common.pointer.focused_window);
    if (!window) {
        ctx->linux_common.pointer.focused_window = NULL;
    }

    if (ctx->scroll.active) {
        if (window) {
            MARU_Event evt = {0};
            evt.mouse_scroll.delta = ctx->scroll.delta;
            evt.mouse_scroll.steps.x = ctx->scroll.steps.x;
            evt.mouse_scroll.steps.y = ctx->scroll.steps.y;
            evt.mouse_scroll.modifiers = _get_modifiers(ctx);
            _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_SCROLLED, (MARU_Window *)window, &evt);
        }
        memset(&ctx->scroll, 0, sizeof(ctx->scroll));
    }
}
static void _pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {}
static void _pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {}
static void _pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        ctx->scroll.steps.y -= discrete;
    } else {
        ctx->scroll.steps.x -= discrete;
    }
    ctx->scroll.active = true;
}

static MARU_ModifierFlags _get_modifiers(MARU_Context_WL *ctx) {
    MARU_ModifierFlags mods = 0;
    if (!ctx->linux_common.xkb.state) return 0;

    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.shift))
        mods |= MARU_MODIFIER_SHIFT;
    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.ctrl))
        mods |= MARU_MODIFIER_CONTROL;
    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.alt))
        mods |= MARU_MODIFIER_ALT;
    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.meta))
        mods |= MARU_MODIFIER_META;
    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.caps))
        mods |= MARU_MODIFIER_CAPS_LOCK;
    if (maru_xkb_state_mod_index_is_active(ctx, ctx->linux_common.xkb.state, ctx->linux_common.xkb.mod_indices.num))
        mods |= MARU_MODIFIER_NUM_LOCK;

    return mods;
}

static void _relative_pointer_handle_relative_motion(void *data, struct zwp_relative_pointer_v1 *pointer,
                                                     uint32_t utime_hi, uint32_t utime_lo,
                                                     wl_fixed_t dx, wl_fixed_t dy,
                                                     wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

    // printf("DEBUG: Relative motion: %f, %f\n", wl_fixed_to_double(dx), wl_fixed_to_double(dy));

    MARU_Event evt = {0};
    evt.mouse_motion.position.x = ctx->linux_common.pointer.x;
    evt.mouse_motion.position.y = ctx->linux_common.pointer.y;
    evt.mouse_motion.delta.x = wl_fixed_to_double(dx);
    evt.mouse_motion.delta.y = wl_fixed_to_double(dy);
    evt.mouse_motion.raw_delta.x = wl_fixed_to_double(dx_unaccel);
    evt.mouse_motion.raw_delta.y = wl_fixed_to_double(dy_unaccel);
    evt.mouse_motion.modifiers = _get_modifiers(ctx);

    _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED, (MARU_Window *)window, &evt);
}

const struct zwp_relative_pointer_v1_listener _maru_wayland_relative_pointer_listener = {
    .relative_motion = _relative_pointer_handle_relative_motion,
};

static void _locked_pointer_handle_locked(void *data, struct zwp_locked_pointer_v1 *locked_pointer) {}
static void _locked_pointer_handle_unlocked(void *data, struct zwp_locked_pointer_v1 *locked_pointer) {}

const struct zwp_locked_pointer_v1_listener _maru_wayland_locked_pointer_listener = {
    .locked = _locked_pointer_handle_locked,
    .unlocked = _locked_pointer_handle_unlocked,
};

const struct wl_pointer_listener _maru_wayland_pointer_listener = {
    .enter = _pointer_handle_enter,
    .leave = _pointer_handle_leave,
    .motion = _pointer_handle_motion,
    .button = _pointer_handle_button,
    .axis = _pointer_handle_axis,
    .frame = _pointer_handle_frame,
    .axis_source = _pointer_handle_axis_source,
    .axis_stop = _pointer_handle_axis_stop,
    .axis_discrete = _pointer_handle_axis_discrete,
};

static MARU_Key _linux_scancode_to_maru_key(uint32_t scancode) {
    switch (scancode) {
        case KEY_ESC: return MARU_KEY_ESCAPE;
        case KEY_1: return MARU_KEY_1;
        case KEY_2: return MARU_KEY_2;
        case KEY_3: return MARU_KEY_3;
        case KEY_4: return MARU_KEY_4;
        case KEY_5: return MARU_KEY_5;
        case KEY_6: return MARU_KEY_6;
        case KEY_7: return MARU_KEY_7;
        case KEY_8: return MARU_KEY_8;
        case KEY_9: return MARU_KEY_9;
        case KEY_0: return MARU_KEY_0;
        case KEY_MINUS: return MARU_KEY_MINUS;
        case KEY_EQUAL: return MARU_KEY_EQUAL;
        case KEY_BACKSPACE: return MARU_KEY_BACKSPACE;
        case KEY_TAB: return MARU_KEY_TAB;
        case KEY_Q: return MARU_KEY_Q;
        case KEY_W: return MARU_KEY_W;
        case KEY_E: return MARU_KEY_E;
        case KEY_R: return MARU_KEY_R;
        case KEY_T: return MARU_KEY_T;
        case KEY_Y: return MARU_KEY_Y;
        case KEY_U: return MARU_KEY_U;
        case KEY_I: return MARU_KEY_I;
        case KEY_O: return MARU_KEY_O;
        case KEY_P: return MARU_KEY_P;
        case KEY_LEFTBRACE: return MARU_KEY_LEFT_BRACKET;
        case KEY_RIGHTBRACE: return MARU_KEY_RIGHT_BRACKET;
        case KEY_ENTER: return MARU_KEY_ENTER;
        case KEY_LEFTCTRL: return MARU_KEY_LEFT_CONTROL;
        case KEY_A: return MARU_KEY_A;
        case KEY_S: return MARU_KEY_S;
        case KEY_D: return MARU_KEY_D;
        case KEY_F: return MARU_KEY_F;
        case KEY_G: return MARU_KEY_G;
        case KEY_H: return MARU_KEY_H;
        case KEY_J: return MARU_KEY_J;
        case KEY_K: return MARU_KEY_K;
        case KEY_L: return MARU_KEY_L;
        case KEY_SEMICOLON: return MARU_KEY_SEMICOLON;
        case KEY_APOSTROPHE: return MARU_KEY_APOSTROPHE;
        case KEY_GRAVE: return MARU_KEY_GRAVE_ACCENT;
        case KEY_LEFTSHIFT: return MARU_KEY_LEFT_SHIFT;
        case KEY_BACKSLASH: return MARU_KEY_BACKSLASH;
        case KEY_Z: return MARU_KEY_Z;
        case KEY_X: return MARU_KEY_X;
        case KEY_C: return MARU_KEY_C;
        case KEY_V: return MARU_KEY_V;
        case KEY_B: return MARU_KEY_B;
        case KEY_N: return MARU_KEY_N;
        case KEY_M: return MARU_KEY_M;
        case KEY_COMMA: return MARU_KEY_COMMA;
        case KEY_DOT: return MARU_KEY_PERIOD;
        case KEY_SLASH: return MARU_KEY_SLASH;
        case KEY_RIGHTSHIFT: return MARU_KEY_RIGHT_SHIFT;
        case KEY_KPASTERISK: return MARU_KEY_KP_MULTIPLY;
        case KEY_LEFTALT: return MARU_KEY_LEFT_ALT;
        case KEY_SPACE: return MARU_KEY_SPACE;
        case KEY_CAPSLOCK: return MARU_KEY_CAPS_LOCK;
        case KEY_F1: return MARU_KEY_F1;
        case KEY_F2: return MARU_KEY_F2;
        case KEY_F3: return MARU_KEY_F3;
        case KEY_F4: return MARU_KEY_F4;
        case KEY_F5: return MARU_KEY_F5;
        case KEY_F6: return MARU_KEY_F6;
        case KEY_F7: return MARU_KEY_F7;
        case KEY_F8: return MARU_KEY_F8;
        case KEY_F9: return MARU_KEY_F9;
        case KEY_F10: return MARU_KEY_F10;
        case KEY_NUMLOCK: return MARU_KEY_NUM_LOCK;
        case KEY_SCROLLLOCK: return MARU_KEY_SCROLL_LOCK;
        case KEY_KP7: return MARU_KEY_KP_7;
        case KEY_KP8: return MARU_KEY_KP_8;
        case KEY_KP9: return MARU_KEY_KP_9;
        case KEY_KPMINUS: return MARU_KEY_KP_SUBTRACT;
        case KEY_KP4: return MARU_KEY_KP_4;
        case KEY_KP5: return MARU_KEY_KP_5;
        case KEY_KP6: return MARU_KEY_KP_6;
        case KEY_KPPLUS: return MARU_KEY_KP_ADD;
        case KEY_KP1: return MARU_KEY_KP_1;
        case KEY_KP2: return MARU_KEY_KP_2;
        case KEY_KP3: return MARU_KEY_KP_3;
        case KEY_KP0: return MARU_KEY_KP_0;
        case KEY_KPDOT: return MARU_KEY_KP_DECIMAL;
        case KEY_F11: return MARU_KEY_F11;
        case KEY_F12: return MARU_KEY_F12;
        case KEY_KPENTER: return MARU_KEY_KP_ENTER;
        case KEY_RIGHTCTRL: return MARU_KEY_RIGHT_CONTROL;
        case KEY_KPSLASH: return MARU_KEY_KP_DIVIDE;
        case KEY_SYSRQ: return MARU_KEY_PRINT_SCREEN;
        case KEY_RIGHTALT: return MARU_KEY_RIGHT_ALT;
        case KEY_HOME: return MARU_KEY_HOME;
        case KEY_UP: return MARU_KEY_UP;
        case KEY_PAGEUP: return MARU_KEY_PAGE_UP;
        case KEY_LEFT: return MARU_KEY_LEFT;
        case KEY_RIGHT: return MARU_KEY_RIGHT;
        case KEY_END: return MARU_KEY_END;
        case KEY_DOWN: return MARU_KEY_DOWN;
        case KEY_PAGEDOWN: return MARU_KEY_PAGE_DOWN;
        case KEY_INSERT: return MARU_KEY_INSERT;
        case KEY_DELETE: return MARU_KEY_DELETE;
        case KEY_LEFTMETA: return MARU_KEY_LEFT_META;
        case KEY_RIGHTMETA: return MARU_KEY_RIGHT_META;
        case KEY_COMPOSE: return MARU_KEY_MENU;
        default: return MARU_KEY_UNKNOWN;
    }
}

static void _keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
                                    uint32_t format, int32_t fd, uint32_t size) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    char *map_str;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    if (ctx->linux_common.xkb.keymap)
        maru_xkb_keymap_unref(ctx, ctx->linux_common.xkb.keymap);
    if (ctx->linux_common.xkb.state)
        maru_xkb_state_unref(ctx, ctx->linux_common.xkb.state);

    ctx->linux_common.xkb.keymap = maru_xkb_keymap_new_from_string(
        ctx, ctx->linux_common.xkb.ctx, map_str);

    munmap(map_str, size);
    close(fd);

    if (!ctx->linux_common.xkb.keymap) {
        return;
    }

    ctx->linux_common.xkb.state = maru_xkb_state_new(ctx, ctx->linux_common.xkb.keymap);

    ctx->linux_common.xkb.mod_indices.shift = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_SHIFT);
    ctx->linux_common.xkb.mod_indices.ctrl = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_CTRL);
    ctx->linux_common.xkb.mod_indices.alt = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_ALT);
    ctx->linux_common.xkb.mod_indices.meta = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_LOGO);
    ctx->linux_common.xkb.mod_indices.caps = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_CAPS);
    ctx->linux_common.xkb.mod_indices.num = maru_xkb_mod_index(ctx, ctx->linux_common.xkb.keymap, XKB_MOD_NAME_NUM);
}

static void _keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t serial, struct wl_surface *surface,
                                   struct wl_array *keys) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    if (!surface) return;

    MARU_Window_WL *window = (MARU_Window_WL *)ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *)surface);
    if (window) {
        ctx->linux_common.xkb.focused_window = (MARU_Window *)window;
        window->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;

        // Re-sync key cache from compositor-provided pressed keys on focus enter.
        memset(window->base.keyboard_state, 0, sizeof(window->base.keyboard_state));
        if (keys) {
            const uint32_t *key = NULL;
            wl_array_for_each(key, keys) {
                MARU_Key maru_key = _linux_scancode_to_maru_key(*key);
                if (maru_key != MARU_KEY_UNKNOWN) {
                    window->base.keyboard_state[maru_key] = (MARU_ButtonState8)MARU_BUTTON_STATE_PRESSED;
                }
            }
        }
        
        _maru_wayland_dispatch_presentation_state(
            window, MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED, true);
    }
}

static void _keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t serial, struct wl_surface *surface) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    
    if (ctx->linux_common.xkb.focused_window) {
        MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.xkb.focused_window;
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);

        // Clear keyboard state on focus loss
        memset(window->base.keyboard_state, 0, sizeof(window->base.keyboard_state));

        _maru_wayland_dispatch_presentation_state(
            window, MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED, true);
        ctx->linux_common.xkb.focused_window = NULL;
    }

    ctx->repeat.repeat_key = 0;
    ctx->repeat.next_repeat_ns = 0;
    ctx->repeat.interval_ns = 0;
}

static void _keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                 uint32_t serial, uint32_t time, uint32_t key,
                                 uint32_t state) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.xkb.focused_window;
    if (!window || !ctx->linux_common.xkb.state) return;

    uint32_t keycode = key + 8;
    MARU_Key maru_key = _linux_scancode_to_maru_key(key);
    MARU_ButtonState maru_state = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (maru_state == MARU_BUTTON_STATE_PRESSED) {
        ctx->clipboard.serial = serial;
    }
    const bool text_input_active =
        (window->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE) &&
        (window->ext.text_input != NULL) &&
        (ctx->protocols.opt.zwp_text_input_manager_v3 != NULL) &&
        window->ime_preedit_active;

    // Update keyboard cache
    if (maru_key != MARU_KEY_UNKNOWN) {
        window->base.keyboard_state[maru_key] = (MARU_ButtonState8)maru_state;
    }

    const bool is_repeat = (state == WL_KEYBOARD_KEY_STATE_PRESSED) && (ctx->repeat.repeat_key == keycode);

    if (!is_repeat) {
        MARU_Event evt = {0};
        evt.key.raw_key = maru_key;
        evt.key.state = maru_state;
        evt.key.modifiers = _get_modifiers(ctx);

        _maru_dispatch_event(&ctx->base, MARU_EVENT_KEY_STATE_CHANGED, (MARU_Window *)window, &evt);
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (text_input_active) {
            // When text-input-v3 is active, committed text should come from the IME protocol path.
            ctx->repeat.repeat_key = 0;
            ctx->repeat.next_repeat_ns = 0;
            ctx->repeat.interval_ns = 0;
            return;
        }

        char buf[32];
        int n = maru_xkb_state_key_get_utf8(ctx, ctx->linux_common.xkb.state, keycode, buf, sizeof(buf));
        if (n > 0) {
            MARU_Event text_evt = {0};
            text_evt.text_edit_commit.committed_utf8 = buf;
            text_evt.text_edit_commit.committed_length = (uint32_t)n;
            _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)window, &text_evt);

            if (ctx->repeat.rate > 0 && ctx->repeat.delay >= 0) {
                const uint64_t now_ns = _maru_wayland_get_monotonic_time_ns();
                const uint64_t delay_ns = ((uint64_t)(uint32_t)ctx->repeat.delay) * 1000000ull;
                uint64_t interval_ns = 1000000000ull / (uint64_t)(uint32_t)ctx->repeat.rate;
                if (interval_ns == 0) {
                    interval_ns = 1;
                }

                ctx->repeat.repeat_key = keycode;
                ctx->repeat.interval_ns = interval_ns;
                ctx->repeat.next_repeat_ns = now_ns + delay_ns;
            } else {
                ctx->repeat.repeat_key = 0;
                ctx->repeat.next_repeat_ns = 0;
                ctx->repeat.interval_ns = 0;
            }
        } else {
            ctx->repeat.repeat_key = 0;
            ctx->repeat.next_repeat_ns = 0;
            ctx->repeat.interval_ns = 0;
        }
    } else {
        if (ctx->repeat.repeat_key == keycode) {
            ctx->repeat.repeat_key = 0;
            ctx->repeat.next_repeat_ns = 0;
            ctx->repeat.interval_ns = 0;
        }
    }
}

static void _keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                       uint32_t serial, uint32_t mods_depressed,
                                       uint32_t mods_latched, uint32_t mods_locked,
                                       uint32_t group) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    if (ctx->linux_common.xkb.state) {
        maru_xkb_state_update_mask(ctx, ctx->linux_common.xkb.state,
                                   mods_depressed, mods_latched, mods_locked, group);
    }
}

static void _keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                         int32_t rate, int32_t delay) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    ctx->repeat.rate = rate;
    ctx->repeat.delay = delay;
}

const struct wl_keyboard_listener _maru_wayland_keyboard_listener = {
    .keymap = _keyboard_handle_keymap,
    .enter = _keyboard_handle_enter,
    .leave = _keyboard_handle_leave,
    .key = _keyboard_handle_key,
    .modifiers = _keyboard_handle_modifiers,
    .repeat_info = _keyboard_handle_repeat_info,
};

static void _text_input_handle_enter(void *data, struct zwp_text_input_v3 *text_input, struct wl_surface *surface) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    (void)surface;

    window->text_input_session_id++;
    window->ime_preedit_active = false;
    _maru_wayland_clear_text_input_pending(window);
    
    MARU_Event evt = {0};
    evt.text_edit_start.session_id = window->text_input_session_id;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_START, (MARU_Window *)window, &evt);
}

static void _text_input_handle_leave(void *data, struct zwp_text_input_v3 *text_input, struct wl_surface *surface) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    (void)surface;
    window->ime_preedit_active = false;
    _maru_wayland_clear_text_input_pending(window);

    MARU_Event evt = {0};
    evt.text_edit_end.session_id = window->text_input_session_id;
    evt.text_edit_end.canceled = false;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_END, (MARU_Window *)window, &evt);
}

static void _text_input_handle_preedit_string(void *data, struct zwp_text_input_v3 *text_input,
                                               const char *text, int32_t cursor_begin, int32_t cursor_end) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    (void)text_input;

    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.preedit_utf8, text ? text : "");
    window->text_input_pending.caret_begin = (cursor_begin >= 0) ? (uint32_t)cursor_begin : 0u;
    window->text_input_pending.caret_end = (cursor_end >= 0) ? (uint32_t)cursor_end : 0u;
    window->text_input_pending.has_preedit = true;
}

static void _text_input_handle_commit_string(void *data, struct zwp_text_input_v3 *text_input, const char *text) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    (void)text_input;

    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.commit_utf8, text ? text : "");
    window->text_input_pending.has_commit = true;
}

static void _text_input_handle_delete_surrounding_text(void *data, struct zwp_text_input_v3 *text_input,
                                                       uint32_t before_length, uint32_t after_length) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    (void)text_input;

    window->text_input_pending.delete_before_bytes = before_length;
    window->text_input_pending.delete_after_bytes = after_length;
    window->text_input_pending.has_delete = true;
}

static void _text_input_handle_done(void *data, struct zwp_text_input_v3 *text_input, uint32_t serial) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    (void)serial;

    if (window->text_input_pending.has_preedit) {
        MARU_Event evt = {0};
        const char *preedit = window->text_input_pending.preedit_utf8 ? window->text_input_pending.preedit_utf8 : "";
        const uint32_t preedit_length = (uint32_t)strlen(preedit);
        uint32_t safe_begin = window->text_input_pending.caret_begin;
        uint32_t safe_end = window->text_input_pending.caret_end;

        if (safe_begin > preedit_length) safe_begin = preedit_length;
        if (safe_end < safe_begin) safe_end = safe_begin;
        if (safe_end > preedit_length) safe_end = preedit_length;

        evt.text_edit_update.session_id = window->text_input_session_id;
        evt.text_edit_update.preedit_utf8 = preedit;
        evt.text_edit_update.preedit_length = preedit_length;
        evt.text_edit_update.caret.start_byte = safe_begin;
        evt.text_edit_update.caret.length_byte = 0;
        evt.text_edit_update.selection.start_byte = safe_begin;
        evt.text_edit_update.selection.length_byte = safe_end - safe_begin;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_UPDATE, (MARU_Window *)window, &evt);

        window->ime_preedit_active = (preedit_length != 0);
    }

    if (window->text_input_pending.has_commit || window->text_input_pending.has_delete) {
        MARU_Event evt = {0};
        const char *commit = window->text_input_pending.commit_utf8 ? window->text_input_pending.commit_utf8 : "";
        evt.text_edit_commit.session_id = window->text_input_session_id;
        evt.text_edit_commit.delete_before_bytes = window->text_input_pending.has_delete
                                                       ? window->text_input_pending.delete_before_bytes
                                                       : 0u;
        evt.text_edit_commit.delete_after_bytes = window->text_input_pending.has_delete
                                                      ? window->text_input_pending.delete_after_bytes
                                                      : 0u;
        evt.text_edit_commit.committed_utf8 = commit;
        evt.text_edit_commit.committed_length = (uint32_t)strlen(commit);
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)window, &evt);

        window->ime_preedit_active = false;
    }

    _maru_wayland_clear_text_input_pending(window);
}

const struct zwp_text_input_v3_listener _maru_wayland_text_input_listener = {
    .enter = _text_input_handle_enter,
    .leave = _text_input_handle_leave,
    .preedit_string = _text_input_handle_preedit_string,
    .commit_string = _text_input_handle_commit_string,
    .delete_surrounding_text = _text_input_handle_delete_surrounding_text,
    .done = _text_input_handle_done,
};


static bool _maru_wayland_create_owned_cursor_frame(MARU_Context_WL *ctx,
                                                    const MARU_Image_Base *image,
                                                    MARU_Vec2Px hot_spot,
                                                    uint32_t delay_ms,
                                                    MARU_WaylandCursorFrame *out_frame) {
  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->shm_fd = -1;

  const int32_t width = (int32_t)image->width;
  const int32_t height = (int32_t)image->height;
  if (width <= 0 || height <= 0 || !image->pixels || !ctx->protocols.wl_shm) {
    return false;
  }

  const int32_t stride = width * 4;
  const size_t data_size = (size_t)stride * (size_t)height;
  int fd = memfd_create("maru-cursor", MFD_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  if (ftruncate(fd, (off_t)data_size) != 0) {
    close(fd);
    return false;
  }

  void *data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    close(fd);
    return false;
  }
  memcpy(data, image->pixels, data_size);

  struct wl_shm_pool *pool =
      maru_wl_shm_create_pool(ctx, ctx->protocols.wl_shm, fd, (int32_t)data_size);
  if (!pool) {
    munmap(data, data_size);
    close(fd);
    return false;
  }

  struct wl_buffer *buffer = maru_wl_shm_pool_create_buffer(
      ctx, pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
  maru_wl_shm_pool_destroy(ctx, pool);
  if (!buffer) {
    munmap(data, data_size);
    close(fd);
    return false;
  }

  out_frame->buffer = buffer;
  out_frame->shm_data = data;
  out_frame->shm_size = data_size;
  out_frame->shm_fd = fd;
  out_frame->hotspot_x = hot_spot.x;
  out_frame->hotspot_y = hot_spot.y;
  out_frame->width = image->width;
  out_frame->height = image->height;
  out_frame->delay_ms = (delay_ms == 0) ? 1u : delay_ms;
  out_frame->owns_buffer = true;
  return true;
}

static void _maru_wayland_destroy_cursor_frames(MARU_Context_WL *ctx, MARU_Cursor_WL *cursor) {
  if (!cursor || !cursor->frames) {
    return;
  }
  for (uint32_t i = 0; i < cursor->frame_count; ++i) {
    MARU_WaylandCursorFrame *frame = &cursor->frames[i];
    if (frame->owns_buffer && frame->buffer) {
      maru_wl_buffer_destroy(ctx, frame->buffer);
      frame->buffer = NULL;
    }
    if (frame->shm_data && frame->shm_size > 0) {
      munmap(frame->shm_data, frame->shm_size);
      frame->shm_data = NULL;
      frame->shm_size = 0;
    }
    if (frame->shm_fd >= 0) {
      close(frame->shm_fd);
      frame->shm_fd = -1;
    }
  }
  maru_context_free(&ctx->base, cursor->frames);
  cursor->frames = NULL;
  cursor->frame_count = 0;
}

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  const bool system_cursor = (create_info->source == MARU_CURSOR_SOURCE_SYSTEM);

  MARU_Cursor_WL *cursor = maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) return MARU_FAILURE;
  memset(cursor, 0, sizeof(MARU_Cursor_WL));

  cursor->base.ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  cursor->base.backend = &maru_backend_WL;
#endif
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.userdata = create_info->userdata;

  if (system_cursor) {
    if (create_info->system_shape <= 0 || create_info->system_shape >= 16) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    const bool has_shape_protocol = ctx->protocols.opt.wp_cursor_shape_manager_v1 != NULL;
    if (_maru_wayland_ensure_cursor_theme(ctx)) {
      cursor->wl_cursor = maru_wl_cursor_theme_get_cursor(
          ctx, ctx->wl.cursor_theme, _maru_cursor_shape_to_name(create_info->system_shape));
    }
    if (!has_shape_protocol && !cursor->wl_cursor) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    cursor->base.pub.flags = MARU_CURSOR_FLAG_SYSTEM;
    cursor->cursor_shape = create_info->system_shape;
  } else {
    if (!create_info->frames || create_info->frame_count == 0) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    cursor->frames = (MARU_WaylandCursorFrame *)maru_context_alloc(
        &ctx->base, sizeof(MARU_WaylandCursorFrame) * create_info->frame_count);
    if (!cursor->frames) {
      maru_context_free(&ctx->base, cursor);
      return MARU_FAILURE;
    }
    memset(cursor->frames, 0, sizeof(MARU_WaylandCursorFrame) * create_info->frame_count);
    for (uint32_t i = 0; i < create_info->frame_count; ++i) {
      const MARU_CursorFrame *frame = &create_info->frames[i];
      if (!frame->image) {
        _maru_wayland_destroy_cursor_frames(ctx, cursor);
        maru_context_free(&ctx->base, cursor);
        return MARU_FAILURE;
      }
      const MARU_Image_Base *image = (const MARU_Image_Base *)frame->image;
      if (image->ctx_base != &ctx->base) {
        _maru_wayland_destroy_cursor_frames(ctx, cursor);
        maru_context_free(&ctx->base, cursor);
        return MARU_FAILURE;
      }
      if (!_maru_wayland_create_owned_cursor_frame(ctx, image, frame->hot_spot,
                                                   frame->delay_ms, &cursor->frames[i])) {
        _maru_wayland_destroy_cursor_frames(ctx, cursor);
        maru_context_free(&ctx->base, cursor);
        return MARU_FAILURE;
      }
    }
    cursor->frame_count = create_info->frame_count;
  }

  ctx->base.metrics.cursor_create_count_total++;
  if (system_cursor) ctx->base.metrics.cursor_create_count_system++;
  else ctx->base.metrics.cursor_create_count_custom++;
  ctx->base.metrics.cursor_alive_current++;
  if (ctx->base.metrics.cursor_alive_current > ctx->base.metrics.cursor_alive_peak) {
    ctx->base.metrics.cursor_alive_peak = ctx->base.metrics.cursor_alive_current;
  }

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor) {
  MARU_Cursor_WL *cursor_wl = (MARU_Cursor_WL *)cursor;
  MARU_Context_WL *ctx = (MARU_Context_WL *)cursor_wl->base.ctx_base;

  if (ctx->cursor_animation.active && ctx->cursor_animation.cursor == cursor_wl) {
    _maru_wayland_clear_cursor_animation(ctx);
  }

  for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
      MARU_Window_WL *window = (MARU_Window_WL *)it;
      if (window->base.pub.current_cursor != cursor) {
          continue;
      }

      window->base.pub.current_cursor = NULL;
      window->base.attrs_requested.cursor = NULL;
      window->base.attrs_effective.cursor = NULL;

      if (ctx->linux_common.pointer.focused_window == (MARU_Window *)window) {
          _maru_wayland_update_cursor(ctx, window, ctx->linux_common.pointer.enter_serial);
      }
  }

  _maru_wayland_destroy_cursor_frames(ctx, cursor_wl);

  ctx->base.metrics.cursor_destroy_count_total++;
  if (ctx->base.metrics.cursor_alive_current > 0) {
    ctx->base.metrics.cursor_alive_current--;
  }

  maru_context_free(&ctx->base, cursor);
  return MARU_SUCCESS;
}

MARU_Status maru_createImage_WL(MARU_Context *context,
                                const MARU_ImageCreateInfo *create_info,
                                MARU_Image **out_image) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  if (!create_info->pixels || create_info->size.x <= 0 || create_info->size.y <= 0) {
    return MARU_FAILURE;
  }

  const uint32_t width = (uint32_t)create_info->size.x;
  const uint32_t height = (uint32_t)create_info->size.y;
  const uint32_t min_stride = width * 4u;
  const uint32_t stride = (create_info->stride_bytes == 0) ? min_stride : create_info->stride_bytes;
  if (stride < min_stride) {
    return MARU_FAILURE;
  }

  MARU_Image_Base *image = (MARU_Image_Base *)maru_context_alloc(&ctx->base, sizeof(MARU_Image_Base));
  if (!image) {
    return MARU_FAILURE;
  }
  memset(image, 0, sizeof(MARU_Image_Base));

  const size_t packed_stride = (size_t)min_stride;
  const size_t dst_size = packed_stride * (size_t)height;
  image->pixels = (uint8_t *)maru_context_alloc(&ctx->base, dst_size);
  if (!image->pixels) {
    maru_context_free(&ctx->base, image);
    return MARU_FAILURE;
  }

  const uint8_t *src = (const uint8_t *)create_info->pixels;
  for (uint32_t y = 0; y < height; ++y) {
    memcpy(image->pixels + (size_t)y * packed_stride,
           src + (size_t)y * (size_t)stride,
           packed_stride);
  }

  image->ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  image->backend = &maru_backend_WL;
#endif
  image->pub.userdata = create_info->userdata;
  image->width = width;
  image->height = height;
  image->stride_bytes = min_stride;

  *out_image = (MARU_Image *)image;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyImage_WL(MARU_Image *image_handle) {
  MARU_Image_Base *image = (MARU_Image_Base *)image_handle;
  MARU_Context_Base *ctx_base = image->ctx_base;
  if (image->pixels) {
    maru_context_free(ctx_base, image->pixels);
    image->pixels = NULL;
  }
  maru_context_free(ctx_base, image);
  return MARU_SUCCESS;
}
