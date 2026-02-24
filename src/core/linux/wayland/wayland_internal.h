// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_INTERNAL_H_INCLUDED
#define MARU_WAYLAND_INTERNAL_H_INCLUDED

#include "linux_internal.h"
#include "dlib/wayland-client.h"
#include "dlib/wayland-cursor.h"
#include "dlib/libdecor.h"
#include "protocols/maru_protocols.h"

typedef struct MARU_Context_WL {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;
  int wake_fd;

  struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct zwp_text_input_manager_v3 *text_input_manager;
    struct ext_idle_notification_v1 *idle_notification;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
  } wl;

  struct {
    int32_t rate;
    int32_t delay;
    uint32_t repeat_key;
    uint64_t next_repeat_ns;
    uint64_t interval_ns;
  } repeat;

  MARU_Wayland_Protocols_WL protocols;

  struct {
    MARU_Vec2Dip delta;
    struct {
      int32_t x;
      int32_t y;
    } steps;
    bool active;
  } scroll;

  struct {
    MARU_Lib_WaylandClient wl;
    MARU_Lib_WaylandCursor wlc;
    struct {
      MARU_Lib_Decor decor;
    } opt;
  } dlib;

  MARU_WaylandDecorationMode decor_mode;
  struct libdecor *libdecor_context;
} MARU_Context_WL;

typedef struct MARU_Window_WL {
  MARU_Window_Base base;

  struct {
    struct wl_surface *surface;
    struct wl_callback *frame_callback;
  } wl;

  struct {
    struct wp_fractional_scale_v1 *fractional_scale;
    struct wp_viewport *viewport;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor;
    struct wp_content_type_v1 *content_type;
    struct zwp_text_input_v3 *text_input;
    struct zwp_relative_pointer_v1 *relative_pointer;
    struct zwp_locked_pointer_v1 *locked_pointer;
  } ext;

  struct {
    struct xdg_surface *surface;
    struct xdg_toplevel *toplevel;
    struct zxdg_toplevel_decoration_v1 *decoration;
  } xdg;

  struct {
    struct libdecor_frame *frame;
    struct libdecor_configuration *last_configuration;
  } libdecor;

  MARU_Vec2Dip size;
  MARU_Vec2Dip min_size;
  MARU_Vec2Dip max_size;
  MARU_Vec2Dip viewport_size;
  MARU_RectDip text_input_rect;
  char *surrounding_text;
  uint32_t surrounding_cursor_offset;
  uint32_t surrounding_anchor_offset;
  uint64_t text_input_session_id;
  MARU_Fraction aspect_ratio;
  MARU_TextInputType text_input_type;
  MARU_Scalar scale;
  MARU_WaylandDecorationMode decor_mode;
  MARU_CursorMode cursor_mode;
  MARU_Monitor **monitors;
  uint32_t monitor_count;
  uint32_t monitor_capacity;
  bool is_maximized;
  bool is_fullscreen;
  bool is_resizable;
  bool is_decorated;
  bool pending_resized_event;
  bool accepts_drop;
  bool primary_selection;
  bool ime_preedit_active;
} MARU_Window_WL;

typedef struct MARU_Monitor_WL {
  MARU_Monitor_Base base;
  struct wl_output *output;
  struct zxdg_output_v1 *xdg_output;
  uint32_t name;
  char *name_storage;
  MARU_VideoMode current_mode;
  MARU_VideoMode *modes;
  uint32_t mode_count;
  uint32_t mode_capacity;
  MARU_Scalar scale;
  bool announced;
} MARU_Monitor_WL;

typedef struct MARU_Cursor_WL {
  MARU_Cursor_Base base;
  struct wl_cursor *wl_cursor;
  struct wl_buffer *buffer;
  void *shm_data;
  size_t shm_size;
  int shm_fd;
  int32_t hotspot_x;
  int32_t hotspot_y;
  uint32_t width;
  uint32_t height;
  MARU_CursorShape cursor_shape;
} MARU_Cursor_WL;

/* wayland-client core helpers */

static inline struct wl_display *
maru_wl_display_connect(MARU_Context_WL *ctx, const char *name)
{
	return ctx->dlib.wl.display_connect(name);
}

static inline void
maru_wl_display_disconnect(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	ctx->dlib.wl.display_disconnect(wl_display);
}

static inline int
maru_wl_display_roundtrip(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_roundtrip(wl_display);
}

static inline int
maru_wl_display_dispatch_pending(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_dispatch_pending(wl_display);
}

static inline int
maru_wl_display_get_error(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_get_error(wl_display);
}

static inline int
maru_wl_display_get_fd(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_get_fd(wl_display);
}

static inline int
maru_wl_display_flush(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_flush(wl_display);
}

static inline int
maru_wl_display_prepare_read(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_prepare_read(wl_display);
}

static inline int
maru_wl_display_read_events(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_read_events(wl_display);
}

static inline void
maru_wl_display_cancel_read(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	ctx->dlib.wl.display_cancel_read(wl_display);
}

/* wayland-cursor helpers */

static inline struct wl_cursor_theme *maru_wl_cursor_theme_load(const MARU_Context_WL *ctx,
                                                                const char *name, int size,
                                                                struct wl_shm *shm) {
  return ctx->dlib.wlc.theme_load(name, size, shm);
}

static inline void maru_wl_cursor_theme_destroy(const MARU_Context_WL *ctx,
                                                struct wl_cursor_theme *theme) {
  ctx->dlib.wlc.theme_destroy(theme);
}

static inline struct wl_cursor *maru_wl_cursor_theme_get_cursor(const MARU_Context_WL *ctx,
                                                               struct wl_cursor_theme *theme,
                                                               const char *name) {
  return ctx->dlib.wlc.theme_get_cursor(theme, name);
}

static inline struct wl_buffer *maru_wl_cursor_image_get_buffer(const MARU_Context_WL *ctx,
                                                                 struct wl_cursor_image *image) {
  return ctx->dlib.wlc.image_get_buffer(image);
}

/* xkbcommon helpers */

static inline struct xkb_context *maru_xkb_context_new(const MARU_Context_WL *ctx,
                                                       enum xkb_context_flags flags) {
  return ctx->linux_common.xkb_lib.context_new(flags);
}

static inline void maru_xkb_context_unref(const MARU_Context_WL *ctx, struct xkb_context *context) {
  ctx->linux_common.xkb_lib.context_unref(context);
}

static inline struct xkb_keymap *maru_xkb_keymap_new_from_string(const MARU_Context_WL *ctx,
                                                                  struct xkb_context *context,
                                                                  const char *string) {
  return ctx->linux_common.xkb_lib.keymap_new_from_string(context, string, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

static inline void maru_xkb_keymap_unref(const MARU_Context_WL *ctx, struct xkb_keymap *keymap) {
  ctx->linux_common.xkb_lib.keymap_unref(keymap);
}

static inline struct xkb_state *maru_xkb_state_new(const MARU_Context_WL *ctx, struct xkb_keymap *keymap) {
  return ctx->linux_common.xkb_lib.state_new(keymap);
}

static inline void maru_xkb_state_unref(const MARU_Context_WL *ctx, struct xkb_state *state) {
  ctx->linux_common.xkb_lib.state_unref(state);
}

static inline void maru_xkb_state_update_mask(const MARU_Context_WL *ctx, struct xkb_state *state,
                                              uint32_t depressed, uint32_t latched, uint32_t locked,
                                              uint32_t group) {
  ctx->linux_common.xkb_lib.state_update_mask(state, depressed, latched, locked, 0, 0, group);
}

static inline uint32_t maru_xkb_mod_index(const MARU_Context_WL *ctx, struct xkb_keymap *keymap, const char *name) {
  return ctx->linux_common.xkb_lib.keymap_mod_get_index(keymap, name);
}

static inline bool maru_xkb_state_mod_name_is_active(const MARU_Context_WL *ctx, struct xkb_state *state, const char *name) {
  return ctx->linux_common.xkb_lib.state_mod_name_is_active(state, name, XKB_STATE_MODS_EFFECTIVE);
}

static inline bool maru_xkb_state_mod_index_is_active(const MARU_Context_WL *ctx, struct xkb_state *state, uint32_t index) {
  return ctx->linux_common.xkb_lib.state_mod_index_is_active(state, index, XKB_STATE_MODS_EFFECTIVE);
}

static inline xkb_mod_mask_t maru_xkb_state_serialize_mods(const MARU_Context_WL *ctx, struct xkb_state *state) {
  return ctx->linux_common.xkb_lib.state_serialize_mods(state, XKB_STATE_MODS_EFFECTIVE);
}

static inline xkb_keysym_t maru_xkb_state_key_get_one_sym(const MARU_Context_WL *ctx, struct xkb_state *state, xkb_keycode_t key) {
  return ctx->linux_common.xkb_lib.state_key_get_one_sym(state, key);
}

static inline int maru_xkb_state_key_get_utf8(const MARU_Context_WL *ctx, struct xkb_state *state, xkb_keycode_t key, char *buffer, size_t size) {
  return ctx->linux_common.xkb_lib.state_key_get_utf8(state, key, buffer, size);
}

extern const struct wl_surface_listener _maru_wayland_surface_listener;
extern const struct wp_fractional_scale_v1_listener _maru_wayland_fractional_scale_listener;
extern const struct wl_pointer_listener _maru_wayland_pointer_listener;
extern const struct wl_keyboard_listener _maru_wayland_keyboard_listener;
extern const struct wl_output_listener _maru_wayland_output_listener;
extern const struct zwp_text_input_v3_listener _maru_wayland_text_input_listener;

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context);
MARU_Status maru_destroyContext_WL(MARU_Context *context);
MARU_Status maru_updateContext_WL(MARU_Context *context, uint64_t field_mask,
                                  const MARU_ContextAttributes *attributes);
MARU_Status maru_wakeContext_WL(MARU_Context *context);
MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window);
MARU_Status maru_updateWindow_WL(MARU_Window *window, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes);
MARU_Status maru_destroyWindow_WL(MARU_Window *window);
MARU_Status maru_requestWindowFocus_WL(MARU_Window *window);
MARU_Status maru_requestWindowFrame_WL(MARU_Window *window);
void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);

MARU_Monitor *const *maru_getMonitors_WL(MARU_Context *context, uint32_t *out_count);
MARU_Status maru_updateMonitors_WL(MARU_Context *context);
const MARU_VideoMode *maru_getMonitorModes_WL(const MARU_Monitor *monitor, uint32_t *out_count);
MARU_Status maru_setMonitorMode_WL(const MARU_Monitor *monitor, MARU_VideoMode mode);
void maru_destroyMonitor_WL(MARU_Monitor *monitor);

void _maru_wayland_register_xdg_output(MARU_Context_WL *ctx, MARU_Monitor_WL *monitor);

MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor);

void _maru_wayland_bind_output(MARU_Context_WL *ctx, uint32_t name, uint32_t version);
void _maru_wayland_remove_output(MARU_Context_WL *ctx, uint32_t name);

bool _maru_wayland_create_xdg_shell_objects(MARU_Window_WL *window,
                                             const MARU_WindowCreateInfo *create_info);
void _maru_wayland_update_opaque_region(MARU_Window_WL *window);
void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial);
void _maru_wayland_update_cursor_mode(MARU_Window_WL *window);
void _maru_wayland_dispatch_window_resized(MARU_Window_WL *window);
void _maru_wayland_update_text_input(MARU_Window_WL *window);
void _maru_wayland_enforce_aspect_ratio(uint32_t *width, uint32_t *height,
                                        const MARU_Window_WL *window);
extern const struct zwp_relative_pointer_v1_listener _maru_wayland_relative_pointer_listener;
extern const struct zwp_locked_pointer_v1_listener _maru_wayland_locked_pointer_listener;
MARU_WaylandDecorationMode _maru_wayland_get_decoration_mode(MARU_Context_WL *ctx);

bool _maru_wayland_create_ssd_decoration(MARU_Window_WL *window);
void _maru_wayland_destroy_ssd_decoration(MARU_Window_WL *window);

void _maru_wayland_check_activation(MARU_Context_WL *ctx);
void _maru_wayland_update_idle_inhibitor(MARU_Window_WL *window);
void _maru_wayland_request_frame(MARU_Window_WL *window);
uint64_t _maru_wayland_get_monotonic_time_ns(void);

bool _maru_wayland_init_libdecor(MARU_Context_WL *ctx);
void _maru_wayland_cleanup_libdecor(MARU_Context_WL *ctx);
bool _maru_wayland_create_libdecor_frame(MARU_Window_WL *window,
                                          const MARU_WindowCreateInfo *create_info);
void _maru_wayland_destroy_libdecor_frame(MARU_Window_WL *window);
void _maru_wayland_libdecor_set_title(MARU_Window_WL *window, const char *title);
void _maru_wayland_libdecor_set_capabilities(MARU_Window_WL *window);

/* libdecor helpers */

static inline struct libdecor *maru_libdecor_new(MARU_Context_WL *ctx,
                                                 struct wl_display *display,
                                                 struct libdecor_interface *iface) {
  return ctx->dlib.opt.decor.new(display, iface);
}

static inline void maru_libdecor_unref(MARU_Context_WL *ctx, struct libdecor *context) {
  ctx->dlib.opt.decor.unref(context);
}

static inline struct libdecor_frame *maru_libdecor_decorate(MARU_Context_WL *ctx,
                                                            struct libdecor *context,
                                                            struct wl_surface *surface,
                                                            struct libdecor_frame_interface *iface,
                                                            void *user_data) {
  return ctx->dlib.opt.decor.decorate(context, surface, iface, user_data);
}

static inline void maru_libdecor_frame_unref(MARU_Context_WL *ctx,
                                              struct libdecor_frame *frame) {
  ctx->dlib.opt.decor.frame_unref(frame);
}

static inline void maru_libdecor_frame_set_title(MARU_Context_WL *ctx,
                                                 struct libdecor_frame *frame, const char *title) {
  ctx->dlib.opt.decor.frame_set_title(frame, title);
}

static inline void maru_libdecor_frame_set_app_id(MARU_Context_WL *ctx,
                                                  struct libdecor_frame *frame,
                                                  const char *app_id) {
  ctx->dlib.opt.decor.frame_set_app_id(frame, app_id);
}

static inline void maru_libdecor_frame_set_capabilities(MARU_Context_WL *ctx,
                                                        struct libdecor_frame *frame,
                                                        enum libdecor_capabilities capabilities) {
  ctx->dlib.opt.decor.frame_set_capabilities(frame, capabilities);
}

static inline void maru_libdecor_frame_map(MARU_Context_WL *ctx, struct libdecor_frame *frame) {
  ctx->dlib.opt.decor.frame_map(frame);
}

static inline void maru_libdecor_frame_set_visibility(MARU_Context_WL *ctx,
                                                      struct libdecor_frame *frame, bool visible) {
  ctx->dlib.opt.decor.frame_set_visibility(frame, visible);
}

static inline void maru_libdecor_frame_commit(MARU_Context_WL *ctx,
                                              struct libdecor_frame *frame,
                                              struct libdecor_state *state,
                                              struct libdecor_configuration *configuration) {
  ctx->dlib.opt.decor.frame_commit(frame, state, configuration);
}

static inline struct xdg_toplevel *maru_libdecor_frame_get_xdg_toplevel(
    const MARU_Context_WL *ctx, struct libdecor_frame *frame) {
  return ctx->dlib.opt.decor.frame_get_xdg_toplevel(frame);
}

static inline struct xdg_surface *maru_libdecor_frame_get_xdg_surface(const MARU_Context_WL *ctx,
                                                                      struct libdecor_frame *frame) {
  return ctx->dlib.opt.decor.frame_get_xdg_surface(frame);
}

static inline void maru_libdecor_frame_set_min_content_size(MARU_Context_WL *ctx,
                                                            struct libdecor_frame *frame,
                                                            int content_width,
                                                            int content_height) {
  ctx->dlib.opt.decor.frame_set_min_content_size(frame, content_width, content_height);
}

static inline void maru_libdecor_frame_set_max_content_size(MARU_Context_WL *ctx,
                                                            struct libdecor_frame *frame,
                                                            int content_width,
                                                            int content_height) {
  ctx->dlib.opt.decor.frame_set_max_content_size(frame, content_width, content_height);
}

static inline void maru_libdecor_frame_get_min_content_size(const MARU_Context_WL *ctx,
                                                            const struct libdecor_frame *frame,
                                                            int *content_width,
                                                            int *content_height) {
  ctx->dlib.opt.decor.frame_get_min_content_size(frame, content_width, content_height);
}

static inline void maru_libdecor_frame_get_max_content_size(const MARU_Context_WL *ctx,
                                                            const struct libdecor_frame *frame,
                                                            int *content_width,
                                                            int *content_height) {
  ctx->dlib.opt.decor.frame_get_max_content_size(frame, content_width, content_height);
}

static inline void maru_libdecor_frame_set_fullscreen(MARU_Context_WL *ctx,
                                                      struct libdecor_frame *frame,
                                                      struct wl_output *output) {
  ctx->dlib.opt.decor.frame_set_fullscreen(frame, output);
}

static inline void maru_libdecor_frame_unset_fullscreen(MARU_Context_WL *ctx,
                                                        struct libdecor_frame *frame) {
  ctx->dlib.opt.decor.frame_unset_fullscreen(frame);
}

static inline void maru_libdecor_frame_set_maximized(MARU_Context_WL *ctx,
                                                     struct libdecor_frame *frame) {
  ctx->dlib.opt.decor.frame_set_maximized(frame);
}

static inline void maru_libdecor_frame_unset_maximized(MARU_Context_WL *ctx,
                                                        struct libdecor_frame *frame) {
  ctx->dlib.opt.decor.frame_unset_maximized(frame);
}

static inline void maru_libdecor_frame_translate_coordinate(MARU_Context_WL *ctx,
                                                             struct libdecor_frame *frame,
                                                             int surface_x, int surface_y,
                                                             int *frame_x, int *frame_y) {
  ctx->dlib.opt.decor.frame_translate_coordinate(frame, surface_x, surface_y, frame_x, frame_y);
}

static inline struct libdecor_state *maru_libdecor_state_new(MARU_Context_WL *ctx, int width,
                                                             int height) {
  return ctx->dlib.opt.decor.state_new(width, height);
}

static inline void maru_libdecor_state_free(MARU_Context_WL *ctx,
                                            struct libdecor_state *state) {
  ctx->dlib.opt.decor.state_free(state);
}

static inline bool maru_libdecor_configuration_get_content_size(
    const MARU_Context_WL *ctx, struct libdecor_configuration *configuration,
    struct libdecor_frame *frame, int *width, int *height) {
  return ctx->dlib.opt.decor.configuration_get_content_size(configuration, frame, width, height);
}

static inline bool maru_libdecor_configuration_get_window_state(
    const MARU_Context_WL *ctx, struct libdecor_configuration *configuration,
    enum libdecor_window_state *window_state) {
  return ctx->dlib.opt.decor.configuration_get_window_state(configuration, window_state);
}

static inline int maru_libdecor_dispatch(MARU_Context_WL *ctx, struct libdecor *context,
                                         int timeout) {
  return ctx->dlib.opt.decor.dispatch(context, timeout);
}

static inline int maru_libdecor_get_fd(MARU_Context_WL *ctx, struct libdecor *context) {
  return ctx->dlib.opt.decor.get_fd(context);
}

static inline void maru_libdecor_set_userdata(MARU_Context_WL *ctx, struct libdecor *context,
                                               void *userdata) {
  if (ctx->dlib.opt.decor.opt.set_userdata) {
    ctx->dlib.opt.decor.opt.set_userdata(context, userdata);
  }
}

static inline void *maru_libdecor_get_userdata(MARU_Context_WL *ctx, struct libdecor *context) {
  if (ctx->dlib.opt.decor.opt.get_userdata) {
    return ctx->dlib.opt.decor.opt.get_userdata(context);
  }
  return NULL;
}

#include "protocols/generated/maru-wayland-helpers.h"
#include "protocols/generated/maru-xdg-shell-helpers.h"
#include "protocols/generated/maru-xdg-decoration-unstable-v1-helpers.h"
#include "protocols/generated/maru-xdg-output-unstable-v1-helpers.h"
#include "protocols/generated/maru-viewporter-helpers.h"
#include "protocols/generated/maru-fractional-scale-v1-helpers.h"
#include "protocols/generated/maru-cursor-shape-v1-helpers.h"
#include "protocols/generated/maru-text-input-unstable-v3-helpers.h"
#include "protocols/generated/maru-relative-pointer-unstable-v1-helpers.h"
#include "protocols/generated/maru-pointer-constraints-unstable-v1-helpers.h"
#include "protocols/generated/maru-idle-inhibit-unstable-v1-helpers.h"
#include "protocols/generated/maru-ext-idle-notify-v1-helpers.h"
#include "protocols/generated/maru-xdg-activation-v1-helpers.h"
#include "protocols/generated/maru-primary-selection-unstable-v1-helpers.h"
#include "protocols/generated/maru-content-type-v1-helpers.h"

#endif
