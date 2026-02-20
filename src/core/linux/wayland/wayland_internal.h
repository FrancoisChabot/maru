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

  struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
  } wl;

  struct {
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
  } input;

  struct {
    MARU_Window *keyboard_focus;
    MARU_Window *pointer_focus;
    struct wl_surface *pointer_surface;
    uint32_t pointer_serial;
  } focused;

  MARU_Wayland_Protocols_WL protocols;

  struct {
    struct libdecor *ctx;
  } libdecor;

  MARU_WaylandDecorationMode decoration_mode;

  struct {
    MARU_Lib_WaylandClient wl;
    MARU_Lib_WaylandCursor wlc;
    struct {
      MARU_Lib_Decor decor;
    } opt;
  } dlib;
} MARU_Context_WL;

typedef struct MARU_Window_WL {
  MARU_Window_Base base;

  struct {
    struct wl_surface *surface;
  } wl;

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
  MARU_WaylandDecorationMode decor_mode;
  MARU_CursorMode cursor_mode;
  bool is_maximized;
  bool is_fullscreen;
  bool is_resizable;
  bool is_decorated;
} MARU_Window_WL;

typedef struct MARU_Cursor_WL {
  MARU_Cursor_Base base;
  struct wl_cursor *wl_cursor;
  struct wl_buffer *buffer;
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

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context);
MARU_Status maru_destroyContext_WL(MARU_Context *context);
MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window);
MARU_Status maru_updateWindow_WL(MARU_Window *window, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes);
MARU_Status maru_destroyWindow_WL(MARU_Window *window);
void maru_getWindowGeometry_WL(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry);

MARU_Status maru_getStandardCursor_WL(MARU_Context *context, MARU_CursorShape shape,
                                     MARU_Cursor **out_cursor);
MARU_Status maru_createCursor_WL(MARU_Context *context,
                                const MARU_CursorCreateInfo *create_info,
                                MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_WL(MARU_Cursor *cursor);

bool _maru_wayland_create_xdg_shell_objects(MARU_Window_WL *window,
                                             const MARU_WindowCreateInfo *create_info);
void _maru_wayland_update_opaque_region(MARU_Window_WL *window);
void _maru_wayland_update_cursor(MARU_Context_WL *ctx, MARU_Window_WL *window, uint32_t serial);
MARU_WaylandDecorationMode _maru_wayland_get_decoration_mode(const MARU_ContextCreateInfo *create_info);

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

#include "protocols/generated/maru-wayland-helpers.h"
#include "protocols/generated/maru-xdg-shell-helpers.h"
#include "protocols/generated/maru-xdg-decoration-unstable-v1-helpers.h"

#endif
