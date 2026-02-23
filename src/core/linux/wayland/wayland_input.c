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

void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial) {
    // TODO: Implement cursor update logic
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

    // TODO: Add modifiers
    _maru_dispatch_event(&ctx->base, MARU_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)window, &evt);
}

static void _pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                 uint32_t time, uint32_t axis, wl_fixed_t value) {
    MARU_Context_WL *ctx = (MARU_Context_WL *)data;
    MARU_Window_WL *window = (MARU_Window_WL *)ctx->linux_common.pointer.focused_window;

    if (!window) return;

    MARU_Event evt = {0};
    double val = wl_fixed_to_double(value);
    
    // Scale down scroll values a bit as they can be large on Wayland
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        evt.mouse_scroll.delta.y = -val / 10.0;
    } else {
        evt.mouse_scroll.delta.x = -val / 10.0;
    }

    _maru_dispatch_event(&ctx->base, MARU_MOUSE_SCROLLED, (MARU_Window *)window, &evt);
}

static void _pointer_handle_frame(void *data, struct wl_pointer *wl_pointer) {}
static void _pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {}
static void _pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {}
static void _pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {}

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
}

static void _keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t serial, struct wl_surface *surface,
                                   struct wl_array *keys) {
}

static void _keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
                                   uint32_t serial, struct wl_surface *surface) {
}

static void _keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
                                 uint32_t serial, uint32_t time, uint32_t key,
                                 uint32_t state) {
}

static void _keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                       uint32_t serial, uint32_t mods_depressed,
                                       uint32_t mods_latched, uint32_t mods_locked,
                                       uint32_t group) {
}

static void _keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                         int32_t rate, int32_t delay) {
}

const struct wl_keyboard_listener _maru_wayland_keyboard_listener = {
    .keymap = _keyboard_handle_keymap,
    .enter = _keyboard_handle_enter,
    .leave = _keyboard_handle_leave,
    .key = _keyboard_handle_key,
    .modifiers = _keyboard_handle_modifiers,
    .repeat_info = _keyboard_handle_repeat_info,
};


MARU_Status maru_getStandardCursor_WL(MARU_Context *context, MARU_CursorShape shape,
                                     MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  return MARU_FAILURE;
}

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  return MARU_FAILURE;
}

MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor) {
  MARU_Cursor_WL *cursor_wl = (MARU_Cursor_WL *)cursor;
  MARU_Context_WL *ctx = (MARU_Context_WL *)cursor_wl->base.ctx_base;

  return MARU_FAILURE;
}
