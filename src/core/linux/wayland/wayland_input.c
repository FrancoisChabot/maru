#define _GNU_SOURCE
#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include <linux/input-event-codes.h>

#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
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

void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial) {
    if (!ctx->wl.pointer) return;

    MARU_Cursor *cursor_handle = (MARU_Cursor *)window->base.pub.current_cursor;
    MARU_CursorMode mode = window->base.pub.cursor_mode;

    if (mode == MARU_CURSOR_HIDDEN || mode == MARU_CURSOR_LOCKED) {
        maru_wl_pointer_set_cursor(ctx, ctx->wl.pointer, ctx->linux_common.pointer.enter_serial, NULL, 0, 0);
        return;
    }

    struct wl_cursor *wl_cursor = NULL;
    struct wl_buffer *buffer = NULL;
    int32_t hotspot_x = 0;
    int32_t hotspot_y = 0;
    int32_t width = 0;
    int32_t height = 0;

    if (cursor_handle) {
        MARU_Cursor_WL *cursor = (MARU_Cursor_WL *)cursor_handle;
        wl_cursor = cursor->wl_cursor;

        if (cursor->buffer) {
            buffer = cursor->buffer;
            hotspot_x = cursor->hotspot_x;
            hotspot_y = cursor->hotspot_y;
            width = (int32_t)cursor->width;
            height = (int32_t)cursor->height;
        }
    } else {
        if (!ctx->wl.cursor_theme) {
             if (ctx->protocols.wl_shm) {
                 const char *size_env = getenv("XCURSOR_SIZE");
                 int size = 24;
                 if (size_env) size = atoi(size_env);
                 if (size == 0) size = 24;
                 ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, NULL, size, ctx->protocols.wl_shm);
             }
        }
        if (ctx->wl.cursor_theme) {
            const char* shape_name = "left_ptr";
            // If the user hasn't set a cursor handle, we use default.
            // But we could support more logic here.
            wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, shape_name);
        }
    }

    if (!buffer) {
        if (!wl_cursor) return;

        struct wl_cursor_image *image = wl_cursor->images[0];
        buffer = maru_wl_cursor_image_get_buffer(ctx, image);
        if (!buffer) return;

        hotspot_x = (int32_t)image->hotspot_x;
        hotspot_y = (int32_t)image->hotspot_y;
        width = (int32_t)image->width;
        height = (int32_t)image->height;
    }

    if (!ctx->wl.cursor_surface) {
        ctx->wl.cursor_surface = maru_wl_compositor_create_surface(ctx, ctx->protocols.wl_compositor);
        if (!ctx->wl.cursor_surface) return;
    }

    maru_wl_surface_attach(ctx, ctx->wl.cursor_surface, buffer, 0, 0);
    maru_wl_surface_damage(ctx, ctx->wl.cursor_surface, 0, 0, width, height);
    maru_wl_surface_commit(ctx, ctx->wl.cursor_surface);

    maru_wl_pointer_set_cursor(ctx, ctx->wl.pointer, serial, ctx->wl.cursor_surface, hotspot_x, hotspot_y);
}

static void _pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, struct wl_surface *surface,
                                  wl_fixed_t sx, wl_fixed_t sy) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    if (!surface) return;

    MARU_Window_WL *window = (MARU_Window_WL *)ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *)surface);

    if (window) {
        ctx->linux_common.pointer.focused_window = (MARU_Window *)window;
        ctx->linux_common.pointer.x = wl_fixed_to_double(sx);
        ctx->linux_common.pointer.y = wl_fixed_to_double(sy);
        ctx->linux_common.pointer.enter_serial = serial;
        _maru_wayland_update_cursor(ctx, window, serial);
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
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.pointer.focused_window;

    if (!window) return;

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
    
    _maru_dispatch_event(&ctx->base, MARU_MOUSE_MOVED, (MARU_Window *)window, &evt);
}

static void _pointer_handle_button(void *data, struct wl_pointer *pointer,
                                   uint32_t serial, uint32_t time, uint32_t button,
                                   uint32_t state) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.pointer.focused_window;

    if (!window) return;

    MARU_Event evt = {0};
    
    if (button == BTN_LEFT) evt.mouse_button.button = MARU_MOUSE_BUTTON_LEFT;
    else if (button == BTN_RIGHT) evt.mouse_button.button = MARU_MOUSE_BUTTON_RIGHT;
    else if (button == BTN_MIDDLE) evt.mouse_button.button = MARU_MOUSE_BUTTON_MIDDLE;
    else if (button == BTN_SIDE) evt.mouse_button.button = MARU_MOUSE_BUTTON_4;
    else if (button == BTN_EXTRA) evt.mouse_button.button = MARU_MOUSE_BUTTON_5;
    else evt.mouse_button.button = MARU_MOUSE_BUTTON_8; // Fallback
    
    evt.mouse_button.state = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? 
                             MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    evt.mouse_button.modifiers = _get_modifiers(ctx);

    _maru_dispatch_event(&ctx->base, MARU_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)window, &evt);
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
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.pointer.focused_window;

    if (ctx->scroll.active) {
        if (window) {
            MARU_Event evt = {0};
            evt.mouse_scroll.delta = ctx->scroll.delta;
            evt.mouse_scroll.steps.x = ctx->scroll.steps.x;
            evt.mouse_scroll.steps.y = ctx->scroll.steps.y;
            evt.mouse_scroll.modifiers = _get_modifiers(ctx);
            _maru_dispatch_event(&ctx->base, MARU_MOUSE_SCROLLED, (MARU_Window *)window, &evt);
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

    _maru_dispatch_event(&ctx->base, MARU_MOUSE_MOVED, (MARU_Window *)window, &evt);
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
        
        MARU_Event evt = {0};
        evt.focus.focused = true;
        _maru_dispatch_event(&ctx->base, MARU_FOCUS_CHANGED, (MARU_Window *)window, &evt);
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

        MARU_Event evt = {0};
        evt.focus.focused = false;
        _maru_dispatch_event(&ctx->base, MARU_FOCUS_CHANGED, ctx->linux_common.xkb.focused_window, &evt);
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
    const bool text_input_active =
        (window->text_input_type != MARU_TEXT_INPUT_TYPE_NONE) &&
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

        _maru_dispatch_event(&ctx->base, MARU_KEY_STATE_CHANGED, (MARU_Window *)window, &evt);
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
            _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_COMMIT, (MARU_Window *)window, &text_evt);

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
    
    MARU_Event evt = {0};
    evt.text_edit_start.session_id = window->text_input_session_id;
    _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_START, (MARU_Window *)window, &evt);
}

static void _text_input_handle_leave(void *data, struct zwp_text_input_v3 *text_input, struct wl_surface *surface) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    (void)surface;
    window->ime_preedit_active = false;

    MARU_Event evt = {0};
    evt.text_edit_end.session_id = window->text_input_session_id;
    evt.text_edit_end.canceled = false;
    _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_END, (MARU_Window *)window, &evt);
}

static void _text_input_handle_preedit_string(void *data, struct zwp_text_input_v3 *text_input,
                                               const char *text, int32_t cursor_begin, int32_t cursor_end) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    window->ime_preedit_active = (text && text[0] != '\0');

    MARU_Event evt = {0};
    evt.text_edit_update.session_id = window->text_input_session_id;
    evt.text_edit_update.preedit_utf8 = text ? text : "";
    evt.text_edit_update.preedit_length = text ? (uint32_t)strlen(text) : 0;
    
    evt.text_edit_update.caret.start_byte = (uint32_t)cursor_begin;
    evt.text_edit_update.caret.length_byte = 0;
    
    evt.text_edit_update.selection.start_byte = (uint32_t)cursor_begin;
    evt.text_edit_update.selection.length_byte = (uint32_t)(cursor_end - cursor_begin);
    
    _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_UPDATE, (MARU_Window *)window, &evt);
}

static void _text_input_handle_commit_string(void *data, struct zwp_text_input_v3 *text_input, const char *text) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    window->ime_preedit_active = false;

    MARU_Event evt = {0};
    evt.text_edit_commit.session_id = window->text_input_session_id;
    evt.text_edit_commit.committed_utf8 = text ? text : "";
    evt.text_edit_commit.committed_length = text ? (uint32_t)strlen(text) : 0;
    
    _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_COMMIT, (MARU_Window *)window, &evt);
}

static void _text_input_handle_delete_surrounding_text(void *data, struct zwp_text_input_v3 *text_input,
                                                       uint32_t before_length, uint32_t after_length) {
    MARU_Window_WL *window = (MARU_Window_WL *)data;
    MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;
    (void)text_input;
    window->ime_preedit_active = false;

    MARU_Event evt = {0};
    evt.text_edit_commit.session_id = window->text_input_session_id;
    evt.text_edit_commit.delete_before_bytes = before_length;
    evt.text_edit_commit.delete_after_bytes = after_length;
    evt.text_edit_commit.committed_utf8 = "";
    evt.text_edit_commit.committed_length = 0;
    
    _maru_dispatch_event(&ctx->base, MARU_TEXT_EDIT_COMMIT, (MARU_Window *)window, &evt);
}

static void _text_input_handle_done(void *data, struct zwp_text_input_v3 *text_input, uint32_t serial) {
    (void)data;
    (void)text_input;
    (void)serial;
    // text-input-v3 'done' event is for synchronization of multiple state changes
}

const struct zwp_text_input_v3_listener _maru_wayland_text_input_listener = {
    .enter = _text_input_handle_enter,
    .leave = _text_input_handle_leave,
    .preedit_string = _text_input_handle_preedit_string,
    .commit_string = _text_input_handle_commit_string,
    .delete_surrounding_text = _text_input_handle_delete_surrounding_text,
    .done = _text_input_handle_done,
};


MARU_Status maru_getStandardCursor_WL(MARU_Context *context, MARU_CursorShape shape,
                                     MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  if (shape <= 0 || shape >= 16) return MARU_FAILURE;

  if (ctx->standard_cursors[shape]) {
      *out_cursor = ctx->standard_cursors[shape];
      return MARU_SUCCESS;
  }

  if (!ctx->wl.cursor_theme) {
      if (!ctx->protocols.wl_shm) return MARU_FAILURE;
      
      const char *size_env = getenv("XCURSOR_SIZE");
      int size = 24;
      if (size_env) size = atoi(size_env);
      if (size == 0) size = 24;

      ctx->wl.cursor_theme = maru_wl_cursor_theme_load(ctx, NULL, size, ctx->protocols.wl_shm);
      if (!ctx->wl.cursor_theme) return MARU_FAILURE;
  }

  struct wl_cursor *wl_cursor = maru_wl_cursor_theme_get_cursor(ctx, ctx->wl.cursor_theme, _maru_cursor_shape_to_name(shape));
  if (!wl_cursor) return MARU_FAILURE;

  MARU_Cursor_WL *cursor = maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) return MARU_FAILURE;
  memset(cursor, 0, sizeof(MARU_Cursor_WL));

  cursor->base.ctx_base = &ctx->base;
  #ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  cursor->base.backend = &maru_backend_WL;
  #endif
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.flags = MARU_CURSOR_FLAG_SYSTEM;
  cursor->wl_cursor = wl_cursor;
  cursor->cursor_shape = shape;
  cursor->shm_fd = -1;
  
  ctx->standard_cursors[shape] = (MARU_Cursor *)cursor;
  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  const int32_t width = create_info->size.x;
  const int32_t height = create_info->size.y;

  if (!ctx->protocols.wl_shm || !create_info->pixels || width <= 0 || height <= 0) {
    return MARU_FAILURE;
  }

  const int32_t stride = width * 4;
  const size_t data_size = (size_t)stride * (size_t)height;

  int fd = memfd_create("maru-cursor", MFD_CLOEXEC);
  if (fd < 0) {
    return MARU_FAILURE;
  }

  if (ftruncate(fd, (off_t)data_size) != 0) {
    close(fd);
    return MARU_FAILURE;
  }

  void *data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    close(fd);
    return MARU_FAILURE;
  }

  memcpy(data, create_info->pixels, data_size);

  struct wl_shm_pool *pool =
      maru_wl_shm_create_pool(ctx, ctx->protocols.wl_shm, fd, (int32_t)data_size);
  if (!pool) {
    munmap(data, data_size);
    close(fd);
    return MARU_FAILURE;
  }

  struct wl_buffer *buffer = maru_wl_shm_pool_create_buffer(
      ctx, pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
  maru_wl_shm_pool_destroy(ctx, pool);
  if (!buffer) {
    munmap(data, data_size);
    close(fd);
    return MARU_FAILURE;
  }

  MARU_Cursor_WL *cursor = maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_WL));
  if (!cursor) {
    maru_wl_buffer_destroy(ctx, buffer);
    munmap(data, data_size);
    close(fd);
    return MARU_FAILURE;
  }
  memset(cursor, 0, sizeof(MARU_Cursor_WL));

  cursor->base.ctx_base = &ctx->base;
  #ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  cursor->base.backend = &maru_backend_WL;
  #endif
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.userdata = create_info->userdata;
  cursor->buffer = buffer;
  cursor->shm_data = data;
  cursor->shm_size = data_size;
  cursor->shm_fd = fd;
  cursor->width = (uint32_t)width;
  cursor->height = (uint32_t)height;
  cursor->hotspot_x = create_info->hot_spot.x;
  cursor->hotspot_y = create_info->hot_spot.y;

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor) {
  MARU_Cursor_WL *cursor_wl = (MARU_Cursor_WL *)cursor;
  MARU_Context_WL *ctx = (MARU_Context_WL *)cursor_wl->base.ctx_base;

  if (cursor_wl->buffer) {
      maru_wl_buffer_destroy(ctx, cursor_wl->buffer);
  }
  if (cursor_wl->shm_data && cursor_wl->shm_size > 0) {
      munmap(cursor_wl->shm_data, cursor_wl->shm_size);
  }
  if (cursor_wl->shm_fd >= 0) {
      close(cursor_wl->shm_fd);
  }

  maru_context_free(&ctx->base, cursor);
  return MARU_SUCCESS;
}
