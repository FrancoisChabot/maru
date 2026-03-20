// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include "window_state.h"
#include <string.h>

static int32_t _maru_x11_dip_to_px(MARU_Scalar dip, MARU_Scalar scale) {
  if (scale <= (MARU_Scalar)0.0) {
    scale = (MARU_Scalar)1.0;
  }
  const MARU_Scalar pixels = dip * scale;
  if (pixels <= (MARU_Scalar)0.0) {
    return 1;
  }
  return (int32_t)(pixels + (MARU_Scalar)0.5);
}

static MARU_Scalar _maru_x11_px_to_dip(MARU_Scalar pixels, MARU_Scalar scale) {
  if (scale <= (MARU_Scalar)0.0) {
    scale = (MARU_Scalar)1.0;
  }
  return pixels / scale;
}

static int32_t _maru_x11_optional_dip_to_px(MARU_Scalar dip, MARU_Scalar scale) {
  if (dip <= (MARU_Scalar)0.0) {
    return 0;
  }
  return _maru_x11_dip_to_px(dip, scale);
}

static uint64_t _maru_x11_sync_value_to_u64(XSyncValue value) {
  return ((uint64_t)(uint32_t)value.hi << 32u) | (uint32_t)value.lo;
}

static void _maru_x11_sync_value_from_u64(XSyncValue *value, uint64_t raw) {
  value->hi = (int32_t)(raw >> 32u);
  value->lo = (uint32_t)raw;
}

static bool _maru_x11_has_xsync_support(const MARU_Context_X11 *ctx) {
  return ctx->x11_lib.XSyncQueryExtension != NULL &&
         ctx->x11_lib.XSyncCreateCounter != NULL &&
         ctx->x11_lib.XSyncSetCounter != NULL &&
         ctx->x11_lib.XSyncDestroyCounter != NULL;
}

MARU_Window_X11 *_maru_x11_find_window(MARU_Context_X11 *ctx, Window handle) {
  for (MARU_Window_Base *it = ctx->base.window_list_head; it;
       it = it->ctx_next) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)it;
    if (win->handle == handle) {
      return win;
    }
  }
  return NULL;
}

void _maru_x11_send_net_wm_state_local(MARU_Context_X11 *ctx,
                                       MARU_Window_X11 *win, long action,
                                       Atom atom1, Atom atom2) {
  if ((win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ClientMessage;
    ev.xclient.window = win->handle;
    ev.xclient.message_type = ctx->net_wm_state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = action;
    ev.xclient.data.l[1] = (long)atom1;
    ev.xclient.data.l[2] = (long)atom2;
    ev.xclient.data.l[3] = 1;
    ctx->x11_lib.XSendEvent(ctx->display, ctx->root, False,
                            SubstructureNotifyMask | SubstructureRedirectMask,
                            &ev);
  } else {
    // For unmapped windows, we should update the _NET_WM_STATE property
    // directly.
    Atom *atoms = NULL;
    unsigned long count = 0;
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;
    unsigned char *prop = NULL;

    if (ctx->x11_lib.XGetWindowProperty(
            ctx->display, win->handle, ctx->net_wm_state, 0, 1024, False,
            XA_ATOM, &actual_type, &actual_format, &count, &bytes_after,
            &prop) == Success) {
      atoms = (Atom *)prop;
    }

    Atom new_atoms[1024];
    unsigned long new_count = 0;

    // Copy existing atoms, skipping the ones we might be removing
    for (unsigned long i = 0; i < count; ++i) {
      if (atoms[i] != atom1 && (atom2 == None || atoms[i] != atom2)) {
        if (new_count < 1024)
          new_atoms[new_count++] = atoms[i];
      }
    }

    // Add atoms if action is 1 (Add) or 2 (Toggle and not present)
    if (action == 1 || action == 2) {
      bool has_atom1 = false;
      bool has_atom2 = (atom2 == None);
      for (unsigned long i = 0; i < count; ++i) {
        if (atoms[i] == atom1)
          has_atom1 = true;
        if (atom2 != None && atoms[i] == atom2)
          has_atom2 = true;
      }

      if (action == 1 || !has_atom1) {
        if (new_count < 1024)
          new_atoms[new_count++] = atom1;
      }
      if (atom2 != None && (action == 1 || !has_atom2)) {
        if (new_count < 1024)
          new_atoms[new_count++] = atom2;
      }
    }

    ctx->x11_lib.XChangeProperty(ctx->display, win->handle, ctx->net_wm_state,
                                 XA_ATOM, 32, PropModeReplace,
                                 (unsigned char *)new_atoms, (int)new_count);

    if (prop)
      ctx->x11_lib.XFree(prop);
  }
}

void _maru_x11_apply_size_hints_local(MARU_Context_X11 *ctx,
                                      MARU_Window_X11 *win) {
  const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));

  int32_t min_w =
      _maru_x11_optional_dip_to_px(win->base.attrs_effective.dip_min_size.x, scale);
  int32_t min_h =
      _maru_x11_optional_dip_to_px(win->base.attrs_effective.dip_min_size.y, scale);
  int32_t max_w =
      _maru_x11_optional_dip_to_px(win->base.attrs_effective.dip_max_size.x, scale);
  int32_t max_h =
      _maru_x11_optional_dip_to_px(win->base.attrs_effective.dip_max_size.y, scale);

  if ((win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) == 0) {
    min_w = _maru_x11_dip_to_px(win->base.attrs_effective.dip_size.x, scale);
    min_h = _maru_x11_dip_to_px(win->base.attrs_effective.dip_size.y, scale);
    max_w = min_w;
    max_h = min_h;
  }

  if (min_w > 0 && min_h > 0) {
    hints.flags |= PMinSize;
    hints.min_width = min_w;
    hints.min_height = min_h;
  }
  if (max_w > 0 && max_h > 0) {
    hints.flags |= PMaxSize;
    hints.max_width = max_w;
    hints.max_height = max_h;
  }
  if (win->base.attrs_effective.aspect_ratio.num > 0 &&
      win->base.attrs_effective.aspect_ratio.denom > 0) {
    hints.flags |= PAspect;
    hints.min_aspect.x = (int32_t)win->base.attrs_effective.aspect_ratio.num;
    hints.min_aspect.y = (int32_t)win->base.attrs_effective.aspect_ratio.denom;
    hints.max_aspect.x = (int32_t)win->base.attrs_effective.aspect_ratio.num;
    hints.max_aspect.y = (int32_t)win->base.attrs_effective.aspect_ratio.denom;
  }

  ctx->x11_lib.XSetWMNormalHints(ctx->display, win->handle, &hints);
}

static void _maru_x11_dispatch_state_changed(MARU_Window_X11 *window,
                                                  uint32_t changed_fields) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)window->base.ctx_base;
  MARU_Event evt = {0};
  evt.window_state_changed.changed_fields = changed_fields;
  const uint64_t flags = window->base.pub.flags;
  evt.window_state_changed.presentation_state =
      _maru_window_presentation_state_from_flags(flags);
  window->base.attrs_effective.presentation_state =
      evt.window_state_changed.presentation_state;
  evt.window_state_changed.visible =
      (flags & MARU_WINDOW_STATE_VISIBLE) != 0;
  evt.window_state_changed.focused =
      (flags & MARU_WINDOW_STATE_FOCUSED) != 0;
  evt.window_state_changed.resizable =
      (flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
  evt.window_state_changed.icon = window->base.pub.icon;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_STATE_CHANGED,
                       (MARU_Window *)window, &evt);
}

static bool _maru_x11_window_has_state_atom(const Atom *atoms,
                                            unsigned long count, Atom target) {
  for (unsigned long i = 0; i < count; ++i) {
    if (atoms[i] == target) {
      return true;
    }
  }
  return false;
}

static void _maru_x11_reconcile_wm_state_changed(MARU_Context_X11 *ctx,
                                                       MARU_Window_X11 *win,
                                                       uint32_t changed_seed) {
  uint32_t changed = changed_seed;
  const MARU_WindowPresentationState old_state =
      _maru_window_presentation_state_from_flags(win->base.pub.flags);

  bool new_fullscreen =
      (win->base.pub.flags & MARU_WINDOW_STATE_FULLSCREEN) != 0;
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long count = 0;
  unsigned long bytes_after = 0;
  unsigned char *prop = NULL;
  if (ctx->x11_lib.XGetWindowProperty(ctx->display, win->handle,
                                      ctx->net_wm_state, 0, 1024, False,
                                      XA_ATOM, &actual_type, &actual_format,
                                      &count, &bytes_after, &prop) == Success) {
    if (prop && actual_type == XA_ATOM && actual_format == 32) {
      const Atom *atoms = (const Atom *)prop;
      new_fullscreen = _maru_x11_window_has_state_atom(
          atoms, count, ctx->net_wm_state_fullscreen);
    }
    if (prop) {
      ctx->x11_lib.XFree(prop);
    }
  }

  if (new_fullscreen) {
    win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  } else {
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
  }

  const MARU_WindowPresentationState new_state =
      _maru_window_presentation_state_from_flags(win->base.pub.flags);
  win->base.attrs_effective.presentation_state = new_state;

  if (old_state != new_state) {
    changed |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
  }

  if (changed != 0) {
    _maru_x11_dispatch_state_changed(win, changed);
  }
}

static void _maru_x11_apply_decorations_local(MARU_Context_X11 *ctx,
                                              MARU_Window_X11 *win) {
  struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
  } hints;

  memset(&hints, 0, sizeof(hints));
  hints.flags = 2; // MWM_HINTS_DECORATIONS
  hints.decorations =
      (win->base.pub.flags & MARU_WINDOW_STATE_DECORATED) ? 1 : 0;

  ctx->x11_lib.XChangeProperty(ctx->display, win->handle, ctx->motif_wm_hints,
                               ctx->motif_wm_hints, 32, PropModeReplace,
                               (unsigned char *)&hints, 5);
}

static bool _maru_x11_apply_icon(MARU_Context_X11 *ctx, MARU_Window_X11 *win,
                                 const MARU_Image *icon) {
  if (!icon) {
    ctx->x11_lib.XDeleteProperty(ctx->display, win->handle, ctx->net_wm_icon);
    return true;
  }

  const MARU_Image_Base *img = (const MARU_Image_Base *)icon;
  if (img->ctx_base != &ctx->base || !img->pixels || img->width == 0 ||
      img->height == 0) {
    return false;
  }

  const size_t pixel_count = (size_t)img->width * (size_t)img->height;
  const size_t elem_count = 2u + pixel_count;
  unsigned long *prop = (unsigned long *)maru_context_alloc(
      &ctx->base, elem_count * sizeof(unsigned long));
  if (!prop) {
    return false;
  }
  prop[0] = (unsigned long)img->width;
  prop[1] = (unsigned long)img->height;
  const uint32_t *pixels = (const uint32_t *)img->pixels;
  for (size_t i = 0; i < pixel_count; ++i) {
    prop[2u + i] = (unsigned long)pixels[i];
  }

  ctx->x11_lib.XChangeProperty(ctx->display, win->handle, ctx->net_wm_icon,
                               XA_CARDINAL, 32, PropModeReplace,
                               (const unsigned char *)prop, (int)elem_count);
  maru_context_free(&ctx->base, prop);
  return true;
}

static MARU_Status
_maru_x11_apply_attributes(MARU_Window_X11 *win, uint64_t field_mask,
                           const MARU_WindowAttributes *attributes) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  MARU_WindowAttributes *requested = &win->base.attrs_requested;
  MARU_WindowAttributes *effective = &win->base.attrs_effective;
  MARU_Status status = MARU_SUCCESS;
  uint32_t state_changed_mask = 0u;

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    if (win->base.title_storage) {
      maru_context_free(&ctx->base, win->base.title_storage);
      win->base.title_storage = NULL;
    }
    requested->title = NULL;
    effective->title = NULL;
    win->base.pub.title = NULL;

    if (attributes->title) {
      size_t len = strlen(attributes->title);
      win->base.title_storage = (char *)maru_context_alloc(&ctx->base, len + 1);
      if (win->base.title_storage) {
        memcpy(win->base.title_storage, attributes->title, len + 1);
        requested->title = win->base.title_storage;
        effective->title = win->base.title_storage;
        win->base.pub.title = win->base.title_storage;
      } else {
        status = MARU_FAILURE;
      }
    }
    ctx->x11_lib.XStoreName(ctx->display, win->handle,
                            win->base.pub.title ? win->base.pub.title
                                                : "MARU Window");
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_SIZE) {
    requested->dip_size = attributes->dip_size;
    effective->dip_size = attributes->dip_size;
    const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
    const int32_t px_w = _maru_x11_dip_to_px(effective->dip_size.x, scale);
    const int32_t px_h = _maru_x11_dip_to_px(effective->dip_size.y, scale);
    ctx->x11_lib.XResizeWindow(ctx->display, win->handle, (unsigned int)px_w,
                               (unsigned int)px_h);
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_POSITION) {
    requested->dip_position = attributes->dip_position;
    effective->dip_position = attributes->dip_position;
    ctx->x11_lib.XMoveWindow(ctx->display, win->handle,
                             (int)effective->dip_position.x,
                             (int)effective->dip_position.y);
  }

  if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
    requested->presentation_state = attributes->presentation_state;
    ctx->x11_lib.XMapWindow(ctx->display, win->handle);
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    effective->presentation_state = attributes->presentation_state;
    state_changed_mask |= MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE;
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_MIN_SIZE) {
    requested->dip_min_size = attributes->dip_min_size;
    effective->dip_min_size = attributes->dip_min_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_DIP_MAX_SIZE) {
    requested->dip_max_size = attributes->dip_max_size;
    effective->dip_max_size = attributes->dip_max_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
    requested->aspect_ratio = attributes->aspect_ratio;
    effective->aspect_ratio = attributes->aspect_ratio;
  }
  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
    requested->resizable = attributes->resizable;
    const bool was_resizable =
        (win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
    effective->resizable = attributes->resizable;
    if (effective->resizable) {
      win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
    }
    if (was_resizable != effective->resizable) {
      state_changed_mask |= MARU_WINDOW_STATE_CHANGED_RESIZABLE;
    }
  }
  if (field_mask &
      (MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE |
       MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE)) {
    _maru_x11_apply_size_hints_local(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
    requested->accept_drop = attributes->accept_drop;
    effective->accept_drop = attributes->accept_drop;
    (void)_maru_x11_apply_window_drop_target(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    if (attributes->cursor &&
        ((MARU_Cursor_X11 *)attributes->cursor)->base.ctx_base != &ctx->base) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_INVALID_ARGUMENT,
                             "Cursor does not belong to this context");
      status = MARU_FAILURE;
    }
    requested->cursor = attributes->cursor;
    effective->cursor = attributes->cursor;
    win->base.pub.current_cursor = attributes->cursor;
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    requested->cursor_mode = attributes->cursor_mode;
    effective->cursor_mode = attributes->cursor_mode;
    win->base.pub.cursor_mode = attributes->cursor_mode;
  }

  if (field_mask & (MARU_WINDOW_ATTR_CURSOR | MARU_WINDOW_ATTR_CURSOR_MODE)) {
    if (!_maru_x11_apply_window_cursor_mode(ctx, win)) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 cursor mode transition failed");
      status = MARU_FAILURE;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
    if (attributes->monitor &&
        ((const MARU_Monitor_Base *)attributes->monitor)->ctx_base !=
            &ctx->base) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_INVALID_ARGUMENT,
                             "Monitor does not belong to this context");
      status = MARU_FAILURE;
    }
    requested->monitor = attributes->monitor;
    effective->monitor = attributes->monitor;
    if (attributes->monitor) {
      const MARU_Monitor_X11 *monitor =
          (const MARU_Monitor_X11 *)attributes->monitor;
      const int x = (int)monitor->base.pub.dip_position.x;
      const int y = (int)monitor->base.pub.dip_position.y;
      ctx->x11_lib.XMoveWindow(ctx->display, win->handle, x, y);
      win->base.attrs_effective.dip_position.x = (MARU_Scalar)x;
      win->base.attrs_effective.dip_position.y = (MARU_Scalar)y;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
    requested->text_input_type = attributes->text_input_type;
    effective->text_input_type = attributes->text_input_type;
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT) {
    requested->dip_text_input_rect = attributes->dip_text_input_rect;
    effective->dip_text_input_rect = attributes->dip_text_input_rect;
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT) {
    if (win->base.surrounding_text_storage) {
      maru_context_free(&ctx->base, win->base.surrounding_text_storage);
      win->base.surrounding_text_storage = NULL;
    }
    requested->surrounding_text = NULL;
    effective->surrounding_text = NULL;
    if (attributes->surrounding_text) {
      size_t len = strlen(attributes->surrounding_text);
      win->base.surrounding_text_storage =
          (char *)maru_context_alloc(&ctx->base, len + 1);
      if (win->base.surrounding_text_storage) {
        memcpy(win->base.surrounding_text_storage, attributes->surrounding_text,
               len + 1);
        requested->surrounding_text = win->base.surrounding_text_storage;
        effective->surrounding_text = win->base.surrounding_text_storage;
      } else {
        status = MARU_FAILURE;
      }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE) {
    requested->surrounding_cursor_byte =
        attributes->surrounding_cursor_byte;
    requested->surrounding_anchor_byte =
        attributes->surrounding_anchor_byte;
    effective->surrounding_cursor_byte =
        attributes->surrounding_cursor_byte;
    effective->surrounding_anchor_byte =
        attributes->surrounding_anchor_byte;
  }

  if (field_mask &
      (MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT |
       MARU_WINDOW_ATTR_SURROUNDING_TEXT |
       MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE)) {
    _maru_x11_refresh_text_input_state(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
    requested->dip_viewport_size = attributes->dip_viewport_size;
    effective->dip_viewport_size = attributes->dip_viewport_size;
    const bool viewport_disabled =
        (effective->dip_viewport_size.x <= (MARU_Scalar)0.0) ||
        (effective->dip_viewport_size.y <= (MARU_Scalar)0.0);
    if (viewport_disabled) {
      // When viewport override is disabled, consume the latest server size.
      const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
      effective->dip_size.x =
          _maru_x11_px_to_dip(win->server_size.x, scale);
      effective->dip_size.y =
          _maru_x11_px_to_dip(win->server_size.y, scale);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_ICON) {
    requested->icon = attributes->icon;
    effective->icon = attributes->icon;
    if (!_maru_x11_apply_icon(ctx, win, attributes->icon)) {
      status = MARU_FAILURE;
    }
    state_changed_mask |= MARU_WINDOW_STATE_CHANGED_ICON;
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    requested->visible = attributes->visible;
    const bool was_visible =
        (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;

    if (attributes->visible) {
      ctx->x11_lib.XMapWindow(ctx->display, win->handle);
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      effective->visible = true;
    } else {
      ctx->x11_lib.XUnmapWindow(ctx->display, win->handle);
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      effective->visible = false;
    }

    if (was_visible != effective->visible) {
      state_changed_mask |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
    }
  }

  if (state_changed_mask != 0) {
    _maru_x11_dispatch_state_changed(win, state_changed_mask);
  }

  maru_getWindowGeometry_X11((MARU_Window *)win, NULL);
  ctx->x11_lib.XFlush(ctx->display);
  return status;
}

void maru_getWindowGeometry_X11(MARU_Window *window,
                                MARU_WindowGeometry *out_geometry) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
  MARU_WindowGeometry geometry = {0};

  geometry.dip_size = win->base.attrs_effective.dip_size;
  geometry.px_size.x = (int32_t)win->server_size.x;
  geometry.px_size.y = (int32_t)win->server_size.y;
  geometry.scale = scale;
  geometry.buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
  win->base.pub.geometry = geometry;
  if (out_geometry) {
    *out_geometry = geometry;
  }
}

MARU_Status maru_updateWindow_X11(MARU_Window *window, uint64_t field_mask,
                                  const MARU_WindowAttributes *attributes) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  win->base.attrs_dirty_mask |= field_mask;
  MARU_Status status = _maru_x11_apply_attributes(win, field_mask, attributes);
  win->base.attrs_dirty_mask &= ~field_mask;
  return status;
}

MARU_Status maru_createWindow_X11(MARU_Context *context,
                                  const MARU_WindowCreateInfo *create_info,
                                  MARU_Window **out_window) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  MARU_Window_X11 *win = (MARU_Window_X11 *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_X11));
  if (!win)
    return MARU_FAILURE;

  memset(win, 0, sizeof(*win));
  win->base.ctx_base = &ctx->base;
  win->base.pub.context = context;
  win->base.pub.userdata = create_info->userdata;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = create_info->attributes.cursor;
  win->base.pub.title = NULL;

#ifdef MARU_INDIRECT_BACKEND
  win->base.backend = ctx->base.backend;
#endif

  MARU_Scalar create_logical_w = create_info->attributes.dip_size.x;
  MARU_Scalar create_logical_h = create_info->attributes.dip_size.y;
  if (create_logical_w <= (MARU_Scalar)0.0) {
    create_logical_w = (MARU_Scalar)640.0;
  }
  if (create_logical_h <= (MARU_Scalar)0.0) {
    create_logical_h = (MARU_Scalar)480.0;
  }
  const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
  const int32_t width = _maru_x11_dip_to_px(create_logical_w, scale);
  const int32_t height = _maru_x11_dip_to_px(create_logical_h, scale);

  Visual *visual = DefaultVisual(ctx->display, ctx->screen);
  int depth = DefaultDepth(ctx->display, ctx->screen);

  XSetWindowAttributes swa;
  swa.colormap =
      ctx->x11_lib.XCreateColormap(ctx->display, ctx->root, visual, AllocNone);
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                   StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
  swa.border_pixel = 0;
  swa.background_pixel = 0;

  win->colormap = swa.colormap;
  win->handle = ctx->x11_lib.XCreateWindow(
      ctx->display, ctx->root, 0, 0, (uint32_t)width, (uint32_t)height, 0,
      depth, InputOutput, visual,
      CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa);

  if (!win->handle) {
    ctx->x11_lib.XFreeColormap(ctx->display, win->colormap);
    maru_context_free(&ctx->base, win);
    return MARU_FAILURE;
  }

  Atom protocols[2];
  int protocol_count = 0;
  protocols[protocol_count++] = ctx->wm_delete_window;

  int sync_event, sync_error;
  if (_maru_x11_has_xsync_support(ctx) &&
      ctx->x11_lib.XSyncQueryExtension(ctx->display, &sync_event, &sync_error)) {
    protocols[protocol_count++] = ctx->net_wm_sync_request;
    XSyncValue zero_value;
    _maru_x11_sync_int_to_value(&zero_value, 0);
    win->basic_sync_counter =
        ctx->x11_lib.XSyncCreateCounter(ctx->display, zero_value);
    if (ctx->compositor_supports_extended_frame_sync) {
      win->extended_sync_counter =
          ctx->x11_lib.XSyncCreateCounter(ctx->display, zero_value);
      win->has_extended_frame_sync = (win->extended_sync_counter != None);
    }
    if (win->basic_sync_counter != None) {
      XSyncCounter counters[2];
      int counter_count = 0;
      counters[counter_count++] = win->basic_sync_counter;
      if (win->extended_sync_counter != None) {
        counters[counter_count++] = win->extended_sync_counter;
      }
      ctx->x11_lib.XChangeProperty(ctx->display, win->handle,
                                   ctx->net_wm_sync_request_counter,
                                   XA_CARDINAL, 32, PropModeReplace,
                                   (unsigned char *)counters, counter_count);
    }
  }

  ctx->x11_lib.XSetWMProtocols(ctx->display, win->handle, protocols,
                               protocol_count);

  _maru_register_window(&ctx->base, (MARU_Window *)win);

  win->base.pub.flags = MARU_WINDOW_STATE_READY;
  if (create_info->has_decorations) {
    win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  _maru_x11_apply_decorations_local(ctx, win);

  if (create_info->fullscreen_monitor) {
    const MARU_Monitor_X11 *monitor = (const MARU_Monitor_X11 *)create_info->fullscreen_monitor;
    const int x = (int)monitor->base.pub.dip_position.x;
    const int y = (int)monitor->base.pub.dip_position.y;
    ctx->x11_lib.XMoveWindow(ctx->display, win->handle, x, y);
    win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
    _maru_x11_send_net_wm_state_local(ctx, win, 1, ctx->net_wm_state_fullscreen, None);
  }

  win->server_size.x = (MARU_Scalar)width;
  win->server_size.y = (MARU_Scalar)height;
  win->base.attrs_effective.dip_size.x =
      _maru_x11_px_to_dip(win->server_size.x, scale);
  win->base.attrs_effective.dip_size.y =
      _maru_x11_px_to_dip(win->server_size.y, scale);

  MARU_Status status = _maru_x11_apply_attributes(win, MARU_WINDOW_ATTR_ALL,
                                                  &create_info->attributes);
  if (status != MARU_SUCCESS) {
    maru_destroyWindow_X11((MARU_Window *)win);
    return status;
  }

  // Window creation happens outside maru_pumpEvents(), so dispatching directly
  // would drop these events when no pump callback is installed yet.
  MARU_Event mevt = {0};
  maru_getWindowGeometry_X11((MARU_Window *)win, &mevt.window_ready.geometry);
  _maru_post_event_internal(&ctx->base, MARU_EVENT_WINDOW_READY,
                            (MARU_Window *)win, &mevt);

  MARU_Event revt = {0};
  maru_getWindowGeometry_X11((MARU_Window *)win, &revt.window_resized.geometry);
  _maru_post_event_internal(&ctx->base, MARU_EVENT_WINDOW_RESIZED,
                            (MARU_Window *)win, &revt);

  *out_window = (MARU_Window *)win;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  _maru_x11_clear_window_dataexchange_refs(ctx, win);
  _maru_x11_end_text_session(ctx, win, true);
  if (win->xic) {
    ctx->x11_lib.XDestroyIC(win->xic);
    win->xic = NULL;
  }
  if (win->ime_preedit_storage) {
    maru_context_free(&ctx->base, win->ime_preedit_storage);
    win->ime_preedit_storage = NULL;
  }
  if (win->extended_sync_counter != None) {
    ctx->x11_lib.XSyncDestroyCounter(ctx->display, win->extended_sync_counter);
    win->extended_sync_counter = None;
  }
  if (win->basic_sync_counter != None) {
    ctx->x11_lib.XSyncDestroyCounter(ctx->display, win->basic_sync_counter);
    win->basic_sync_counter = None;
  }
  if (ctx->locked_window == win) {
    _maru_x11_release_pointer_lock(ctx, win);
  }

  if (win->handle) {
    ctx->x11_lib.XUnmapWindow(ctx->display, win->handle);
    ctx->x11_lib.XDestroyWindow(ctx->display, win->handle);
    win->handle = 0;
  }

  if (win->colormap) {
    ctx->x11_lib.XFreeColormap(ctx->display, win->colormap);
    win->colormap = 0;
  }

  _maru_unregister_window(&ctx->base, (MARU_Window *)win);
  if (win->base.title_storage) {
    maru_context_free(&ctx->base, win->base.title_storage);
    win->base.title_storage = NULL;
  }
  if (win->base.surrounding_text_storage) {
    maru_context_free(&ctx->base, win->base.surrounding_text_storage);
    win->base.surrounding_text_storage = NULL;
  }
  maru_context_free(&ctx->base, win);
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;

  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = win->handle;
  ev.xclient.message_type = ctx->net_active_window;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1;
  ev.xclient.data.l[1] = CurrentTime;
  ctx->x11_lib.XSendEvent(ctx->display, ctx->root, False,
                          SubstructureNotifyMask | SubstructureRedirectMask,
                          &ev);
  ctx->x11_lib.XSetInputFocus(ctx->display, win->handle, RevertToParent,
                              CurrentTime);
  ctx->x11_lib.XRaiseWindow(ctx->display, win->handle);
  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;

  if (win->has_extended_frame_sync) {
    if (win->awaiting_frame_drawn) {
      win->pending_frame_request = true;
      return MARU_SUCCESS;
    }
  }

  win->pending_frame_request = true;
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  _maru_x11_send_net_wm_state_local(ctx, win, 1,
                                    ctx->net_wm_state_demands_attention, None);
  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

void *maru_getWindowNativeHandle_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  return (void *)(uintptr_t)win->handle;
}

bool _maru_x11_process_window_event(MARU_Context_X11 *ctx, XEvent *ev) {
  switch (ev->type) {
  case ConfigureNotify: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xconfigure.window);
    if (win) {
      _maru_x11_reconcile_wm_state_changed(ctx, win, 0);
      const MARU_Scalar scale = _maru_x11_get_global_scale(ctx);
      const MARU_Scalar new_w = (MARU_Scalar)ev->xconfigure.width;
      const MARU_Scalar new_h = (MARU_Scalar)ev->xconfigure.height;
      const bool size_changed = (win->server_size.x != new_w) ||
                                (win->server_size.y != new_h);
      const bool scale_changed = (win->base.pub.geometry.scale != scale);

      if (size_changed) {
        win->server_size.x = new_w;
        win->server_size.y = new_h;
        if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED &&
            ctx->locked_window == win) {
          win->lock_center_x = ev->xconfigure.width / 2;
          win->lock_center_y = ev->xconfigure.height / 2;
          (void)_maru_x11_apply_window_cursor_mode(ctx, win);
        }

        const bool viewport_active =
            (win->base.attrs_effective.dip_viewport_size.x > (MARU_Scalar)0.0) &&
            (win->base.attrs_effective.dip_viewport_size.y > (MARU_Scalar)0.0);
        if (!viewport_active) {
          win->base.attrs_effective.dip_size.x =
              _maru_x11_px_to_dip(win->server_size.x, scale);
          win->base.attrs_effective.dip_size.y =
              _maru_x11_px_to_dip(win->server_size.y, scale);
        }
      } else if (scale_changed) {
        const bool viewport_active =
            (win->base.attrs_effective.dip_viewport_size.x > (MARU_Scalar)0.0) &&
            (win->base.attrs_effective.dip_viewport_size.y > (MARU_Scalar)0.0);
        if (!viewport_active) {
          win->base.attrs_effective.dip_size.x =
              _maru_x11_px_to_dip(win->server_size.x, scale);
          win->base.attrs_effective.dip_size.y =
              _maru_x11_px_to_dip(win->server_size.y, scale);
        }
      }

      if (size_changed || scale_changed) {
        MARU_Event mevt = {0};
        maru_getWindowGeometry_X11((MARU_Window *)win, &mevt.window_resized.geometry);
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_RESIZED,
                             (MARU_Window *)win, &mevt);
      }
    }
    return true;
  }
  case Expose: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xexpose.window);
    if (win && ev->xexpose.count == 0) {
      if (win->has_extended_frame_sync) {
        if (!win->awaiting_frame_drawn) {
          win->pending_frame_request = true;
        }
      } else {
        win->pending_frame_request = true;
      }
    }
    return true;
  }
  case MapNotify: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xmap.window);
    if (win) {
      uint32_t changed = 0;
      const bool was_visible =
          (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      win->base.attrs_effective.visible = true;
      if (!was_visible) {
        changed |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
      }

      _maru_x11_reconcile_wm_state_changed(ctx, win, changed);
    }
    return true;
  }
  case UnmapNotify: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xunmap.window);
    if (win) {
      uint32_t changed = 0;
      const bool was_visible =
          (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      win->base.attrs_effective.visible = false;
      if (was_visible) {
        changed |= MARU_WINDOW_STATE_CHANGED_VISIBLE;
      }
      _maru_x11_reconcile_wm_state_changed(ctx, win, changed);
    }
    return true;
  }
  case PropertyNotify: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xproperty.window);
    if (!win) {
      return false;
    }
    if (ev->xproperty.atom == ctx->net_wm_state ||
        ev->xproperty.atom == ctx->wm_state) {
      _maru_x11_reconcile_wm_state_changed(ctx, win, 0);
      return true;
    }
    return false;
  }
  case FocusIn: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xfocus.window);
    if (win) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
      ctx->linux_common.xkb.focused_window = (MARU_Window *)win;
      (void)_maru_x11_apply_window_cursor_mode(ctx, win);
      _maru_x11_refresh_text_input_state(ctx, win);
      MARU_Event mevt = {0};
      mevt.window_state_changed.changed_fields =
          MARU_WINDOW_STATE_CHANGED_FOCUSED;
      mevt.window_state_changed.focused = true;
      _maru_dispatch_event(&ctx->base,
                           MARU_EVENT_WINDOW_STATE_CHANGED,
                           (MARU_Window *)win, &mevt);
    }
    return true;
  }
  case FocusOut: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xfocus.window);
    if (win) {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FOCUSED);
      _maru_x11_end_text_session(ctx, win, false);
      if (ctx->locked_window == win) {
        _maru_x11_release_pointer_lock(ctx, win);
      }
      memset(ctx->base.keyboard_state, 0, sizeof(ctx->base.keyboard_state));
      if (ctx->linux_common.xkb.focused_window == (MARU_Window *)win) {
        ctx->linux_common.xkb.focused_window = NULL;
      }
      MARU_Event mevt = {0};
      mevt.window_state_changed.changed_fields =
          MARU_WINDOW_STATE_CHANGED_FOCUSED;
      mevt.window_state_changed.focused = false;
      _maru_dispatch_event(&ctx->base,
                           MARU_EVENT_WINDOW_STATE_CHANGED,
                           (MARU_Window *)win, &mevt);
    }
    return true;
  }
  case ClientMessage: {
    const Atom message_type = ev->xclient.message_type;
    if (message_type == ctx->wm_protocols) {
      const Atom protocol = (Atom)ev->xclient.data.l[0];
      if (protocol == ctx->wm_delete_window) {
        MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
        if (win) {
          MARU_Event mevt = {0};
          _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED,
                               (MARU_Window *)win, &mevt);
        }
        return true;
      } else if (protocol == ctx->net_wm_sync_request) {
        MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
        if (win) {
          _maru_x11_sync_ints_to_value(&win->sync_request_value,
                                       (uint32_t)ev->xclient.data.l[2],
                                       (int)ev->xclient.data.l[3]);
        }
        return true;
      }
    } else if (message_type == ctx->net_wm_frame_drawn ||
               message_type == ctx->net_wm_frame_timings) {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
      if (!win) {
        return false;
      }
      const uint64_t message_frame_id =
          ((uint64_t)(uint32_t)ev->xclient.data.l[1] << 32u) |
          (uint32_t)ev->xclient.data.l[0];
      if (message_type == ctx->net_wm_frame_drawn &&
          message_frame_id >= win->frame_id) {
        win->awaiting_frame_drawn = false;
      }
      return true;
    }
    return false;
  }
  default:
    return false;
  }
}

void _maru_x11_dispatch_pending_frames(MARU_Context_X11 *ctx) {
  MARU_Window_Base *base = ctx->base.window_list_head;
  const uint64_t now_ms = _maru_x11_get_monotonic_time_ms();

  while (base) {
    MARU_Window_X11 *win = (MARU_Window_X11 *)base;
    if (win->pending_frame_request) {
      bool can_dispatch = true;
      if (win->has_extended_frame_sync && win->awaiting_frame_drawn) {
        if (now_ms - win->last_frame_dispatch_ms > 500) {
          // Stop using extended sync if the compositor stops responding.
          win->has_extended_frame_sync = false;
          win->awaiting_frame_drawn = false;
        } else {
          can_dispatch = false;
        }
      }

      if (can_dispatch) {
        win->pending_frame_request = false;
        win->last_frame_dispatch_ms = now_ms;
        win->frame_id++;

        if (win->has_extended_frame_sync &&
            win->extended_sync_counter != None) {
          const uint64_t current_value =
              _maru_x11_sync_value_to_u64(win->extended_frame_value);
          const uint64_t begin_value = current_value + 1u;
          _maru_x11_sync_value_from_u64(&win->extended_frame_value,
                                        begin_value);
          ctx->x11_lib.XSyncSetCounter(ctx->display, win->extended_sync_counter,
                                       win->extended_frame_value);
          ctx->x11_lib.XFlush(ctx->display);
        }

        MARU_Event evt = {0};
        evt.window_frame.timestamp_ms = (uint32_t)now_ms;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME,
                             (MARU_Window *)win, &evt);

        if (win->has_extended_frame_sync &&
            win->extended_sync_counter != None) {
          const uint64_t end_value =
              _maru_x11_sync_value_to_u64(win->extended_frame_value) + 1u;
          _maru_x11_sync_value_from_u64(&win->extended_frame_value, end_value);
          ctx->x11_lib.XSyncSetCounter(ctx->display, win->extended_sync_counter,
                                       win->extended_frame_value);
          win->awaiting_frame_drawn = true;
        }

        if (win->basic_sync_counter != None &&
            !_maru_x11_sync_value_is_zero(win->sync_request_value)) {
          ctx->x11_lib.XSyncSetCounter(ctx->display, win->basic_sync_counter,
                                       win->sync_request_value);
          _maru_x11_sync_int_to_value(&win->sync_request_value, 0);
          ctx->x11_lib.XFlush(ctx->display);
        }
      }
    }
    base = base->ctx_next;
  }
}
