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

#define WL_KEYBOARD_KEY_STATE_PRESSED 1
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 1
#define WL_POINTER_BUTTON_STATE_PRESSED 1
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_AXIS_VERTICAL_SCROLL 0
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1


static MARU_Window_WL *_maru_wayland_resolve_registered_window(
    MARU_Context_WL *ctx, const MARU_Window *candidate) {
    MARU_ASSUME(ctx != NULL);

    // This can happen with decorations/etc...
    if (!candidate) return NULL;

    for (MARU_Window_Base *it = ctx->base.window_list_head; it; it = it->ctx_next) {
        if ((const MARU_Window *)it == candidate) {
            return (MARU_Window_WL *)it;
        }
    }

    return NULL;
}

static void _maru_wayland_report_unknown_mouse_button_once(MARU_Context_WL *ctx,
                                                           uint32_t native_code) {
#ifdef MARU_ENABLE_INTERNAL_CHECKS
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
#else
    (void)ctx;
    (void)native_code;
#endif
}

static void _maru_wayland_replace_pending_utf8(MARU_Window_WL *window, char **slot,
                                               const char *value) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    maru_context_free(&ctx->base, *slot);
    *slot = NULL;

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
    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.preedit_utf8, NULL);
    _maru_wayland_replace_pending_utf8(window, &window->text_input_pending.commit_utf8, NULL);
    memset(&window->text_input_pending, 0, sizeof(window->text_input_pending));
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
    MARU_ASSUME(ctx->wl.pointer != NULL);
    MARU_ASSUME(window != NULL);

    MARU_CursorMode mode = window->base.pub.cursor_mode;

    if (mode == MARU_CURSOR_HIDDEN || mode == MARU_CURSOR_LOCKED) {
        maru_wl_pointer_set_cursor(ctx, ctx->wl.pointer, serial, NULL, 0, 0);
        return;
    }

    MARU_Cursor_WL *cursor = (MARU_Cursor_WL *)window->base.pub.current_cursor;

    if (cursor && (cursor->base.pub.flags & MARU_CURSOR_FLAG_SYSTEM)) {
        if (ctx->protocols.opt.wp_cursor_shape_manager_v1) {
            uint32_t shape = 0;
            if (_maru_cursor_shape_to_protocol_shape(cursor->cursor_shape, &shape)) {
                maru_wp_cursor_shape_device_v1_set_shape(ctx, ctx->wl.cursor_shape_device, serial, shape);
                return;
            }
        }

        if (cursor->wl_cursor) {
            uint32_t frame_index = 0;
            if (cursor->base.anim_enabled) {
              frame_index = cursor->base.anim_current_frame;
            }
            struct wl_cursor_image *image = cursor->wl_cursor->images[frame_index % cursor->wl_cursor->image_count];
            struct wl_buffer *buffer = maru_wl_cursor_image_get_buffer(ctx, image);
            if (buffer) {
                if (_maru_wayland_commit_cursor_buffer(ctx, serial, buffer, (int32_t)image->hotspot_x, (int32_t)image->hotspot_y, (int32_t)image->width, (int32_t)image->height)) {
                    return;
                }
            }
        }
    }

    if (cursor && cursor->frame_count > 0) {
        uint32_t frame_index = 0;
        if (cursor->base.anim_enabled) {
          frame_index = cursor->base.anim_current_frame;
        }
        const MARU_WaylandCursorFrame *frame = &cursor->frames[frame_index % cursor->frame_count];
        if (_maru_wayland_commit_cursor_buffer(ctx, serial, frame->buffer, frame->hotspot_x,
                                               frame->hotspot_y, (int32_t)frame->width,
                                               (int32_t)frame->height)) {
            return;
        }
    }

    // Default to left_ptr if nothing else worked
    if (_maru_wayland_ensure_cursor_theme(ctx)) {
        struct wl_cursor *default_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, "left_ptr");
        if (default_cursor) {
            struct wl_cursor_image *image = default_cursor->images[0];
            struct wl_buffer *buffer = maru_wl_cursor_image_get_buffer(ctx, image);
            if (buffer) {
                _maru_wayland_commit_cursor_buffer(ctx, serial, buffer, (int32_t)image->hotspot_x, (int32_t)image->hotspot_y, (int32_t)image->width, (int32_t)image->height);
            }
        }
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
    evt.mouse_moved.dip_position.x = (MARU_Scalar)wl_fixed_to_double(sx);
    evt.mouse_moved.dip_position.y = (MARU_Scalar)wl_fixed_to_double(sy);
    
    evt.mouse_moved.dip_delta.x = evt.mouse_moved.dip_position.x - (MARU_Scalar)ctx->linux_common.pointer.x;
    evt.mouse_moved.dip_delta.y = evt.mouse_moved.dip_position.y - (MARU_Scalar)ctx->linux_common.pointer.y;
    evt.mouse_moved.raw_dip_delta.x = 0.0;
    evt.mouse_moved.raw_dip_delta.y = 0.0;
    evt.mouse_moved.modifiers = _maru_wayland_get_modifiers(ctx);
    
    ctx->linux_common.pointer.x = (double)evt.mouse_moved.dip_position.x;
    ctx->linux_common.pointer.y = (double)evt.mouse_moved.dip_position.y;
    
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
    if (!_maru_linux_map_native_mouse_button(button, &channel)) {
        _maru_wayland_report_unknown_mouse_button_once(ctx, button);
        return;
    }

    MARU_ButtonState btn_state = (state == WL_POINTER_BUTTON_STATE_PRESSED) ?
                                 MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (btn_state == MARU_BUTTON_STATE_PRESSED) {
        ctx->clipboard.serial = serial;
        ctx->last_interaction_serial = serial;
    }

    if (ctx->base.mouse_button_states && channel < ctx->base.pub.mouse_button_count) {
        ctx->base.mouse_button_states[channel] = (MARU_ButtonState8)btn_state;
    }

    MARU_Event evt = {0};
    evt.mouse_button_changed.button_id = channel;
    evt.mouse_button_changed.state = btn_state;
    evt.mouse_button_changed.modifiers = _maru_wayland_get_modifiers(ctx);
    _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_BUTTON_CHANGED,
                         (MARU_Window *)window, &evt);
}

static void _pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                 uint32_t time, uint32_t axis, wl_fixed_t value) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        ctx->scroll.delta.y -= (MARU_Scalar)wl_fixed_to_double(value);
    } else {
        ctx->scroll.delta.x -= (MARU_Scalar)wl_fixed_to_double(value);
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
            evt.mouse_scrolled.dip_delta = ctx->scroll.delta;
            evt.mouse_scrolled.steps.x = ctx->scroll.steps.x;
            evt.mouse_scrolled.steps.y = ctx->scroll.steps.y;
            evt.mouse_scrolled.modifiers = _maru_wayland_get_modifiers(ctx);
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

MARU_ModifierFlags _maru_wayland_get_modifiers(MARU_Context_WL *ctx) {
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

    MARU_Event evt = {0};
    evt.mouse_moved.dip_position.x = (MARU_Scalar)ctx->linux_common.pointer.x;
    evt.mouse_moved.dip_position.y = (MARU_Scalar)ctx->linux_common.pointer.y;
    evt.mouse_moved.dip_delta.x = (MARU_Scalar)wl_fixed_to_double(dx);
    evt.mouse_moved.dip_delta.y = (MARU_Scalar)wl_fixed_to_double(dy);
    evt.mouse_moved.raw_dip_delta.x = (MARU_Scalar)wl_fixed_to_double(dx_unaccel);
    evt.mouse_moved.raw_dip_delta.y = (MARU_Scalar)wl_fixed_to_double(dy_unaccel);
    evt.mouse_moved.modifiers = _maru_wayland_get_modifiers(ctx);

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
        window = _maru_wayland_resolve_registered_window(ctx, (MARU_Window *)window);
    }
    if (window) {
        ctx->linux_common.xkb.focused_window = (MARU_Window *)window;
        window->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;

        // Re-sync key cache from compositor-provided pressed keys on focus enter.
        memset(ctx->base.keyboard_state, 0, sizeof(ctx->base.keyboard_state));
        if (keys) {
            const uint32_t *key = NULL;
            wl_array_for_each(key, keys) {
                MARU_Key maru_key = _maru_linux_scancode_to_maru_key(*key);
                if (maru_key != MARU_KEY_UNKNOWN) {
                    ctx->base.keyboard_state[maru_key] = (MARU_ButtonState8)MARU_BUTTON_STATE_PRESSED;
                }
            }
        }
        
        _maru_wayland_dispatch_state_changed(
            window, MARU_WINDOW_STATE_CHANGED_FOCUSED);
    }
}

static void _keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t serial, struct wl_surface *surface) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    
    if (ctx->linux_common.xkb.focused_window) {
        MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.xkb.focused_window;
        window->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);

        // Clear keyboard state on focus loss
        memset(ctx->base.keyboard_state, 0, sizeof(ctx->base.keyboard_state));

        _maru_wayland_dispatch_state_changed(
            window, MARU_WINDOW_STATE_CHANGED_FOCUSED);
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

    uint32_t keycode = key + MARU_WL_XKB_KEY_OFFSET;
    MARU_Key maru_key = _maru_linux_scancode_to_maru_key(key);
    MARU_ButtonState maru_state = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (maru_state == MARU_BUTTON_STATE_PRESSED) {
        ctx->clipboard.serial = serial;
        ctx->last_interaction_serial = serial;
    }
    const bool text_input_enabled =
        (window->base.attrs_effective.text_input_type != MARU_TEXT_INPUT_TYPE_NONE);

    const bool text_input_active =
        text_input_enabled &&
        (window->ext.text_input != NULL) &&
        (ctx->protocols.opt.zwp_text_input_manager_v3 != NULL) &&
        window->ime_preedit_active;

    // Update keyboard cache
    if (maru_key != MARU_KEY_UNKNOWN) {
        ctx->base.keyboard_state[maru_key] = (MARU_ButtonState8)maru_state;
    }

    const bool is_repeat = (state == WL_KEYBOARD_KEY_STATE_PRESSED) && (ctx->repeat.repeat_key == keycode);

    if (!is_repeat) {
        MARU_Event evt = {0};
        evt.key_changed.key = maru_key;
        evt.key_changed.state = maru_state;
        evt.key_changed.modifiers = _maru_wayland_get_modifiers(ctx);

        _maru_dispatch_event(&ctx->base, MARU_EVENT_KEY_CHANGED, (MARU_Window *)window, &evt);
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (text_input_active) {
            // When text-input-v3 is active, committed text should come from the IME protocol path.
            ctx->repeat.repeat_key = 0;
            ctx->repeat.next_repeat_ns = 0;
            ctx->repeat.interval_ns = 0;
            return;
        }

        bool handled_as_text = false;
        if (text_input_enabled) {
            MARU_TextEditNavigationCommand nav_cmd;
            MARU_ModifierFlags mods = _maru_wayland_get_modifiers(ctx);

            bool is_nav = false;
            switch (maru_key) {
                case MARU_KEY_LEFT:
                    is_nav = true;
                    nav_cmd = (mods & MARU_MODIFIER_CONTROL) ? MARU_TEXT_EDIT_NAVIGATE_WORD_LEFT : MARU_TEXT_EDIT_NAVIGATE_LEFT;
                    break;
                case MARU_KEY_RIGHT:
                    is_nav = true;
                    nav_cmd = (mods & MARU_MODIFIER_CONTROL) ? MARU_TEXT_EDIT_NAVIGATE_WORD_RIGHT : MARU_TEXT_EDIT_NAVIGATE_RIGHT;
                    break;
                case MARU_KEY_UP:
                    is_nav = true;
                    nav_cmd = MARU_TEXT_EDIT_NAVIGATE_UP;
                    break;
                case MARU_KEY_DOWN:
                    is_nav = true;
                    nav_cmd = MARU_TEXT_EDIT_NAVIGATE_DOWN;
                    break;
                case MARU_KEY_HOME:
                    is_nav = true;
                    nav_cmd = (mods & MARU_MODIFIER_CONTROL) ? MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_START : MARU_TEXT_EDIT_NAVIGATE_LINE_START;
                    break;
                case MARU_KEY_END:
                    is_nav = true;
                    nav_cmd = (mods & MARU_MODIFIER_CONTROL) ? MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_END : MARU_TEXT_EDIT_NAVIGATE_LINE_END;
                    break;
                default: break;
            }

            if (is_nav) {
                MARU_Event nav_evt = {0};
                nav_evt.text_edit_navigation.session_id = window->text_input_session_id;
                nav_evt.text_edit_navigation.command = nav_cmd;
                nav_evt.text_edit_navigation.extend_selection = (mods & MARU_MODIFIER_SHIFT) != 0;
                nav_evt.text_edit_navigation.is_repeat = is_repeat;
                nav_evt.text_edit_navigation.modifiers = mods;
                _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_NAVIGATION, (MARU_Window *)window, &nav_evt);
                handled_as_text = true;
            } else {
                char buf[32];
                int n = maru_xkb_state_key_get_utf8(ctx, ctx->linux_common.xkb.state, keycode, buf, sizeof(buf));
                if (n > 0) {
                    MARU_Event text_evt = {0};
                    text_evt.text_edit_committed.session_id = window->text_input_session_id;
                    text_evt.text_edit_committed.committed_utf8 = buf;
                    text_evt.text_edit_committed.committed_length_bytes = (uint32_t)n;
                    _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMITTED, (MARU_Window *)window, &text_evt);
                    handled_as_text = true;
                }
            }
        }

        if (handled_as_text) {
            if (ctx->repeat.rate > 0 && ctx->repeat.delay >= 0) {
                const uint64_t now_ns = _maru_linux_get_monotonic_time_ns();
                const uint64_t delay_ns = ((uint64_t)(uint32_t)ctx->repeat.delay) * 1000000ull;
                uint64_t interval_ns = 1000000000ull / (uint32_t)(uint32_t)ctx->repeat.rate;
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
    evt.text_edit_started.session_id = window->text_input_session_id;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_STARTED, (MARU_Window *)window, &evt);
}

static void _text_input_handle_leave(void *data, struct zwp_text_input_v3 *text_input, struct wl_surface *surface) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    (void)surface;
    window->ime_preedit_active = false;
    _maru_wayland_clear_text_input_pending(window);

    MARU_Event evt = {0};
    evt.text_edit_ended.session_id = window->text_input_session_id;
    evt.text_edit_ended.canceled = false;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_ENDED, (MARU_Window *)window, &evt);
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

        evt.text_edit_updated.session_id = window->text_input_session_id;
        evt.text_edit_updated.preedit_utf8 = preedit;
        evt.text_edit_updated.preedit_length_bytes = preedit_length;
        evt.text_edit_updated.caret.start_byte = safe_begin;
        evt.text_edit_updated.caret.length_bytes = 0;
        evt.text_edit_updated.selection.start_byte = safe_begin;
        evt.text_edit_updated.selection.length_bytes = safe_end - safe_begin;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_UPDATED, (MARU_Window *)window, &evt);

        window->ime_preedit_active = (preedit_length != 0);
    }

    if (window->text_input_pending.has_commit || window->text_input_pending.has_delete) {
        MARU_Event evt = {0};
        const char *commit = window->text_input_pending.commit_utf8 ? window->text_input_pending.commit_utf8 : "";
        evt.text_edit_committed.session_id = window->text_input_session_id;
        evt.text_edit_committed.delete_before_bytes = window->text_input_pending.has_delete
                                                       ? window->text_input_pending.delete_before_bytes
                                                       : 0u;
        evt.text_edit_committed.delete_after_bytes = window->text_input_pending.has_delete
                                                      ? window->text_input_pending.delete_after_bytes
                                                      : 0u;
        evt.text_edit_committed.committed_utf8 = commit;
        evt.text_edit_committed.committed_length_bytes = (uint32_t)strlen(commit);
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMITTED, (MARU_Window *)window, &evt);

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
