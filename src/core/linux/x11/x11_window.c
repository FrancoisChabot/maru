// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <string.h>

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
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));

  int32_t min_w = (int32_t)win->base.attrs_effective.min_size.x;
  int32_t min_h = (int32_t)win->base.attrs_effective.min_size.y;
  int32_t max_w = (int32_t)win->base.attrs_effective.max_size.x;
  int32_t max_h = (int32_t)win->base.attrs_effective.max_size.y;

  if ((win->base.pub.flags & MARU_WINDOW_STATE_RESIZABLE) == 0) {
    min_w = (int32_t)win->base.attrs_effective.logical_size.x;
    min_h = (int32_t)win->base.attrs_effective.logical_size.y;
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

static bool _maru_x11_copy_window_string(MARU_Context_X11 *ctx, const char *src,
                                         char **storage_out,
                                         const char **attr_out) {
  if (!src) {
    *storage_out = NULL;
    *attr_out = NULL;
    return true;
  }
  const size_t len = strlen(src);
  char *copy = (char *)maru_context_alloc(&ctx->base, len + 1u);
  if (!copy) {
    *storage_out = NULL;
    *attr_out = NULL;
    return false;
  }
  memcpy(copy, src, len + 1u);
  *storage_out = copy;
  *attr_out = copy;
  return true;
}

void maru_getWindowGeometry_X11(MARU_Window *window,
                                MARU_WindowGeometry *out_geometry) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  memset(out_geometry, 0, sizeof(*out_geometry));

  out_geometry->logical_size = win->base.attrs_effective.logical_size;
  out_geometry->pixel_size.x =
      (int32_t)win->base.attrs_effective.logical_size.x;
  out_geometry->pixel_size.y =
      (int32_t)win->base.attrs_effective.logical_size.y;
  out_geometry->scale = (MARU_Scalar)1.0;
  out_geometry->buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
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
  win->base.pub.metrics = &win->base.metrics;
  win->base.pub.keyboard_state = win->base.keyboard_state;
  win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  win->base.pub.mouse_button_state = ctx->base.pub.mouse_button_state;
  win->base.pub.mouse_button_channels = ctx->base.pub.mouse_button_channels;
  win->base.pub.mouse_button_count = ctx->base.pub.mouse_button_count;
  memcpy(win->base.pub.mouse_default_button_channels,
         ctx->base.pub.mouse_default_button_channels,
         sizeof(win->base.pub.mouse_default_button_channels));
  win->base.pub.event_mask = create_info->attributes.event_mask;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = create_info->attributes.cursor;
  win->base.pub.title = NULL;
  win->base.attrs_requested = create_info->attributes;
  win->base.attrs_effective = create_info->attributes;
  win->base.attrs_dirty_mask = 0;

#ifdef MARU_INDIRECT_BACKEND
  win->base.backend = ctx->base.backend;
#endif

  int32_t width = (int32_t)create_info->attributes.logical_size.x;
  int32_t height = (int32_t)create_info->attributes.logical_size.y;
  if (width <= 0)
    width = 640;
  if (height <= 0)
    height = 480;

  Visual *visual = DefaultVisual(ctx->display, ctx->screen);
  int depth = DefaultDepth(ctx->display, ctx->screen);

  XSetWindowAttributes swa;
  swa.colormap =
      ctx->x11_lib.XCreateColormap(ctx->display, ctx->root, visual, AllocNone);
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                   StructureNotifyMask | FocusChangeMask;
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

  ctx->x11_lib.XSetWMProtocols(ctx->display, win->handle,
                               &ctx->wm_delete_window, 1);

  _maru_register_window(&ctx->base, (MARU_Window *)win);

  if (!_maru_x11_copy_window_string(ctx, create_info->attributes.title,
                                    &win->base.title_storage,
                                    &win->base.attrs_requested.title)) {
    maru_destroyWindow_X11((MARU_Window *)win);
    return MARU_FAILURE;
  }
  win->base.attrs_effective.title = win->base.attrs_requested.title;
  win->base.pub.title = win->base.attrs_effective.title;

  if (!_maru_x11_copy_window_string(
          ctx, create_info->attributes.surrounding_text,
          &win->base.surrounding_text_storage,
          &win->base.attrs_requested.surrounding_text)) {
    maru_destroyWindow_X11((MARU_Window *)win);
    return MARU_FAILURE;
  }
  win->base.attrs_effective.surrounding_text =
      win->base.attrs_requested.surrounding_text;

  win->base.pub.flags = 0;
  if (create_info->attributes.visible) {
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
  }
  if (create_info->attributes.resizable) {
    win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }
  if (create_info->decorated) {
    win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  if (create_info->attributes.minimized) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
    win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
  }
  if (create_info->attributes.fullscreen) {
    win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
  }
  if (create_info->attributes.maximized) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
  }
  if (create_info->attributes.mouse_passthrough) {
    win->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
    if (!ctx->xshape_lib.base.available) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 mouse passthrough requires XShape support");
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MOUSE_PASSTHROUGH);
      win->base.attrs_effective.mouse_passthrough = false;
      win->base.attrs_requested.mouse_passthrough = false;
    }
  }
  win->base.attrs_effective.logical_size.x = (MARU_Scalar)width;
  win->base.attrs_effective.logical_size.y = (MARU_Scalar)height;
  win->server_logical_size = win->base.attrs_effective.logical_size;
  ctx->x11_lib.XStoreName(ctx->display, win->handle,
                          win->base.pub.title ? win->base.pub.title
                                              : "MARU Window");
  _maru_x11_apply_size_hints_local(ctx, win);
  ctx->x11_lib.XMoveWindow(ctx->display, win->handle,
                           (int)win->base.attrs_effective.position.x,
                           (int)win->base.attrs_effective.position.y);
  (void)_maru_x11_apply_window_drop_target(ctx, win);
  _maru_x11_refresh_text_input_state(ctx, win);

  const bool should_map =
      create_info->attributes.visible || create_info->attributes.minimized;
  if (should_map) {
    ctx->x11_lib.XMapWindow(ctx->display, win->handle);
    (void)_maru_x11_apply_window_cursor_mode(ctx, win);
    if (ctx->xshape_lib.base.available &&
        (win->base.pub.flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH) != 0) {
      ctx->xshape_lib.XShapeCombineRectangles(ctx->display, win->handle, 0, 0,
                                              ShapeInput, NULL, 0, ShapeSet,
                                              YXBanded);
    }
  }

  if (create_info->attributes.fullscreen && should_map) {
    _maru_x11_send_net_wm_state_local(ctx, win, 1, ctx->net_wm_state_fullscreen,
                                      None);
  }
  if (create_info->attributes.maximized && should_map) {
    _maru_x11_send_net_wm_state_local(ctx, win, 1,
                                      ctx->net_wm_state_maximized_vert,
                                      ctx->net_wm_state_maximized_horz);
  }
  if (create_info->attributes.minimized && should_map) {
    (void)ctx->x11_lib.XIconifyWindow(ctx->display, win->handle, ctx->screen);
  }
  if (create_info->attributes.icon) {
    const MARU_Image_Base *img =
        (const MARU_Image_Base *)create_info->attributes.icon;
    if (img->ctx_base == &ctx->base && img->pixels && img->width > 0 &&
        img->height > 0) {
      const size_t pixel_count = (size_t)img->width * (size_t)img->height;
      const size_t elem_count = 2u + pixel_count;
      unsigned long *prop = (unsigned long *)maru_context_alloc(
          &ctx->base, elem_count * sizeof(unsigned long));
      if (prop) {
        prop[0] = (unsigned long)img->width;
        prop[1] = (unsigned long)img->height;
        const uint32_t *pixels = (const uint32_t *)img->pixels;
        for (size_t i = 0; i < pixel_count; ++i) {
          prop[2u + i] = (unsigned long)pixels[i];
        }
        ctx->x11_lib.XChangeProperty(
            ctx->display, win->handle, ctx->net_wm_icon, XA_CARDINAL, 32,
            PropModeReplace, (const unsigned char *)prop, (int)elem_count);
        maru_context_free(&ctx->base, prop);
      }
    }
  }
  ctx->x11_lib.XFlush(ctx->display);

  win->base.pending_ready_event = true;

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

static void _maru_x11_dispatch_presentation_state(MARU_Window_X11 *window,
                                                  uint32_t changed_fields,
                                                  bool icon_effective) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)window->base.ctx_base;
  MARU_Event evt = {0};
  evt.presentation.changed_fields = changed_fields;
  evt.presentation.visible =
      (window->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
  evt.presentation.minimized =
      (window->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
  evt.presentation.maximized =
      (window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
  evt.presentation.focused =
      (window->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
  evt.presentation.icon_effective = icon_effective;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED,
                       (MARU_Window *)window, &evt);
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

static bool _maru_x11_apply_mouse_passthrough(MARU_Context_X11 *ctx,
                                              MARU_Window_X11 *win) {
  if (!win->handle) {
    return false;
  }

  const bool enable = win->base.attrs_effective.mouse_passthrough;

  if (ctx->xshape_lib.base.available) {
    int shape_event, shape_error;
    if (ctx->xshape_lib.XShapeQueryExtension(ctx->display, &shape_event,
                                             &shape_error)) {
      int major, minor;
      if (ctx->xshape_lib.XShapeQueryVersion(ctx->display, &major, &minor) &&
          (major > 1 || (major == 1 && minor >= 1))) {
        if (enable) {
          // Set input shape to an empty region to enable passthrough.
          ctx->xshape_lib.XShapeCombineRectangles(ctx->display, win->handle,
                                                  ShapeInput, 0, 0, NULL, 0,
                                                  ShapeSet, YXBanded);
        } else {
          // Reset input shape to follow bounding shape (default behavior).
          // We only do this if the window is already mapped or if we know we
          // previously set a custom shape. For initial creation of
          // non-passthrough windows, we MUST NOT call this as it can break some
          // compositors.
          if ((win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0) {
            ctx->xshape_lib.XShapeCombineMask(ctx->display, win->handle,
                                              ShapeInput, 0, 0, None, ShapeSet);
          }
        }
        return true;
      }
    }
  }

  return !enable;
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
  uint32_t presentation_changed = 0u;

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

  if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
    requested->logical_size = attributes->logical_size;
    effective->logical_size = attributes->logical_size;
    ctx->x11_lib.XResizeWindow(ctx->display, win->handle,
                               (unsigned int)effective->logical_size.x,
                               (unsigned int)effective->logical_size.y);
  }

  if (field_mask & MARU_WINDOW_ATTR_POSITION) {
    requested->position = attributes->position;
    effective->position = attributes->position;
    ctx->x11_lib.XMoveWindow(ctx->display, win->handle,
                             (int)effective->position.x,
                             (int)effective->position.y);
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
    requested->fullscreen = attributes->fullscreen;
    effective->fullscreen = attributes->fullscreen;
    if (effective->fullscreen) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
      _maru_x11_send_net_wm_state_local(ctx, win, 1,
                                        ctx->net_wm_state_fullscreen, None);
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
      _maru_x11_send_net_wm_state_local(ctx, win, 0,
                                        ctx->net_wm_state_fullscreen, None);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
    requested->maximized = attributes->maximized;
    const bool was_maximized =
        (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
    const bool currently_visible =
        (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    if (requested->maximized) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
      win->pending_maximize_request = !currently_visible;
      _maru_x11_send_net_wm_state_local(ctx, win, 1,
                                        ctx->net_wm_state_maximized_vert,
                                        ctx->net_wm_state_maximized_horz);
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
      win->pending_maximize_request = false;
      _maru_x11_send_net_wm_state_local(ctx, win, 0,
                                        ctx->net_wm_state_maximized_vert,
                                        ctx->net_wm_state_maximized_horz);
    }
    effective->maximized =
        (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
    if (was_maximized != effective->maximized) {
      presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MIN_SIZE) {
    requested->min_size = attributes->min_size;
    effective->min_size = attributes->min_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_MAX_SIZE) {
    requested->max_size = attributes->max_size;
    effective->max_size = attributes->max_size;
  }
  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
    requested->aspect_ratio = attributes->aspect_ratio;
    effective->aspect_ratio = attributes->aspect_ratio;
  }
  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
    requested->resizable = attributes->resizable;
    effective->resizable = attributes->resizable;
    if (effective->resizable) {
      win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_RESIZABLE);
    }
  }
  if (field_mask &
      (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE |
       MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE)) {
    _maru_x11_apply_size_hints_local(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
    requested->event_mask = attributes->event_mask;
    effective->event_mask = attributes->event_mask;
    win->base.pub.event_mask = attributes->event_mask;
  }

  if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
    requested->accept_drop = attributes->accept_drop;
    effective->accept_drop = attributes->accept_drop;
    (void)_maru_x11_apply_window_drop_target(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_PRIMARY_SELECTION) {
    requested->primary_selection = attributes->primary_selection;
    effective->primary_selection = attributes->primary_selection;
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

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
    const bool was_passthrough =
        (win->base.pub.flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH) != 0;
    const bool should_be_passthrough = attributes->mouse_passthrough;

    if (should_be_passthrough != was_passthrough || should_be_passthrough) {
      requested->mouse_passthrough = should_be_passthrough;
      effective->mouse_passthrough = should_be_passthrough;
      if (effective->mouse_passthrough) {
        win->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
      } else {
        win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MOUSE_PASSTHROUGH);
      }
      if (!_maru_x11_apply_mouse_passthrough(ctx, win)) {
        MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                               MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                               "X11 mouse passthrough requires XShape support");
        effective->mouse_passthrough = false;
        requested->mouse_passthrough = false;
        win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MOUSE_PASSTHROUGH);
        status = MARU_FAILURE;
      }
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
      const int x = (int)monitor->base.pub.logical_position.x;
      const int y = (int)monitor->base.pub.logical_position.y;
      ctx->x11_lib.XMoveWindow(ctx->display, win->handle, x, y);
      win->base.attrs_effective.position.x = (MARU_Scalar)x;
      win->base.attrs_effective.position.y = (MARU_Scalar)y;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
    requested->text_input_type = attributes->text_input_type;
    effective->text_input_type = attributes->text_input_type;
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_RECT) {
    requested->text_input_rect = attributes->text_input_rect;
    effective->text_input_rect = attributes->text_input_rect;
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

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET) {
    requested->surrounding_cursor_offset =
        attributes->surrounding_cursor_offset;
    requested->surrounding_anchor_offset =
        attributes->surrounding_anchor_offset;
    effective->surrounding_cursor_offset =
        attributes->surrounding_cursor_offset;
    effective->surrounding_anchor_offset =
        attributes->surrounding_anchor_offset;
  }

  if (field_mask &
      (MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_TEXT_INPUT_RECT |
       MARU_WINDOW_ATTR_SURROUNDING_TEXT |
       MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET)) {
    _maru_x11_refresh_text_input_state(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_VIEWPORT_SIZE) {
    requested->viewport_size = attributes->viewport_size;
    effective->viewport_size = attributes->viewport_size;
    const bool viewport_disabled =
        (effective->viewport_size.x <= (MARU_Scalar)0.0) ||
        (effective->viewport_size.y <= (MARU_Scalar)0.0);
    if (viewport_disabled) {
      // When viewport override is disabled, consume the latest server size.
      effective->logical_size = win->server_logical_size;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_ICON) {
    requested->icon = attributes->icon;
    effective->icon = attributes->icon;
    if (!_maru_x11_apply_icon(ctx, win, attributes->icon)) {
      status = MARU_FAILURE;
    }
    presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_ICON;
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    requested->visible = attributes->visible;
    const bool was_visible =
        (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    const bool was_minimized =
        (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;

    if (attributes->visible) {
      ctx->x11_lib.XMapWindow(ctx->display, win->handle);
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
      effective->visible = true;
      effective->minimized = false;
    } else {
      ctx->x11_lib.XUnmapWindow(ctx->display, win->handle);
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      effective->visible = false;
    }

    if (was_visible != effective->visible) {
      presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_VISIBLE;
    }
    if (was_minimized != effective->minimized) {
      presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MINIMIZED) {
    requested->minimized = attributes->minimized;
    const bool was_minimized =
        (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
    const bool was_visible =
        (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;

    if (attributes->minimized) {
      (void)ctx->x11_lib.XIconifyWindow(ctx->display, win->handle, ctx->screen);
      win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_VISIBLE);
      effective->minimized = true;
      effective->visible = false;
    } else {
      ctx->x11_lib.XMapWindow(ctx->display, win->handle);
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MINIMIZED);
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
      effective->minimized = false;
      effective->visible = true;
    }

    if (was_minimized != effective->minimized) {
      presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED;
    }
    if (was_visible != effective->visible) {
      presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_VISIBLE;
    }
  }

  if (presentation_changed != 0) {
    _maru_x11_dispatch_presentation_state(win, presentation_changed, true);
  }

  ctx->x11_lib.XFlush(ctx->display);
  return status;
}

void maru_getWindowGeometry_X11(MARU_Window *window,
                                MARU_WindowGeometry *out_geometry) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  memset(out_geometry, 0, sizeof(*out_geometry));

  out_geometry->logical_size = win->base.attrs_effective.logical_size;
  out_geometry->pixel_size.x =
      (int32_t)win->base.attrs_effective.logical_size.x;
  out_geometry->pixel_size.y =
      (int32_t)win->base.attrs_effective.logical_size.y;
  out_geometry->scale = (MARU_Scalar)1.0;
  out_geometry->buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
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
  win->base.pub.metrics = &win->base.metrics;
  win->base.pub.keyboard_state = win->base.keyboard_state;
  win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  win->base.pub.mouse_button_state = ctx->base.pub.mouse_button_state;
  win->base.pub.mouse_button_channels = ctx->base.pub.mouse_button_channels;
  win->base.pub.mouse_button_count = ctx->base.pub.mouse_button_count;
  memcpy(win->base.pub.mouse_default_button_channels,
         ctx->base.pub.mouse_default_button_channels,
         sizeof(win->base.pub.mouse_default_button_channels));
  win->base.pub.event_mask = create_info->attributes.event_mask;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = create_info->attributes.cursor;
  win->base.pub.title = NULL;

#ifdef MARU_INDIRECT_BACKEND
  win->base.backend = ctx->base.backend;
#endif

  int32_t width = (int32_t)create_info->attributes.logical_size.x;
  int32_t height = (int32_t)create_info->attributes.logical_size.y;
  if (width <= 0)
    width = 640;
  if (height <= 0)
    height = 480;

  Visual *visual = DefaultVisual(ctx->display, ctx->screen);
  int depth = DefaultDepth(ctx->display, ctx->screen);

  XSetWindowAttributes swa;
  swa.colormap =
      ctx->x11_lib.XCreateColormap(ctx->display, ctx->root, visual, AllocNone);
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                   StructureNotifyMask | FocusChangeMask;
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

  ctx->x11_lib.XSetWMProtocols(ctx->display, win->handle,
                               &ctx->wm_delete_window, 1);

  _maru_register_window(&ctx->base, (MARU_Window *)win);

  win->base.pub.flags = MARU_WINDOW_STATE_READY;
  if (create_info->decorated) {
    win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  _maru_x11_apply_decorations_local(ctx, win);

  win->server_logical_size.x = (MARU_Scalar)width;
  win->server_logical_size.y = (MARU_Scalar)height;

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
  maru_getWindowGeometry_X11((MARU_Window *)win, &revt.resized.geometry);
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
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  MARU_Event evt = {0};
  evt.frame.timestamp_ms = (uint32_t)_maru_x11_get_monotonic_time_ms();
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME, window, &evt);
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
      const MARU_Scalar new_w = (MARU_Scalar)ev->xconfigure.width;
      const MARU_Scalar new_h = (MARU_Scalar)ev->xconfigure.height;
      const bool changed = (win->server_logical_size.x != new_w) ||
                           (win->server_logical_size.y != new_h);

      if (changed) {
        win->server_logical_size.x = new_w;
        win->server_logical_size.y = new_h;
        if (win->base.pub.cursor_mode == MARU_CURSOR_LOCKED &&
            ctx->locked_window == win) {
          win->lock_center_x = ev->xconfigure.width / 2;
          win->lock_center_y = ev->xconfigure.height / 2;
          _maru_x11_recenter_locked_pointer(ctx, win);
        }

        const bool viewport_active =
            (win->base.attrs_effective.viewport_size.x > (MARU_Scalar)0.0) &&
            (win->base.attrs_effective.viewport_size.y > (MARU_Scalar)0.0);
        if (!viewport_active) {
          win->base.attrs_effective.logical_size = win->server_logical_size;
        }

        MARU_Event mevt = {0};
        mevt.resized.geometry.logical_size = win->server_logical_size;
        mevt.resized.geometry.pixel_size.x = ev->xconfigure.width;
        mevt.resized.geometry.pixel_size.y = ev->xconfigure.height;
        mevt.resized.geometry.scale = (MARU_Scalar)1.0;
        mevt.resized.geometry.buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_RESIZED,
                             (MARU_Window *)win, &mevt);
      }
    }
    return true;
  }
  case Expose: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xexpose.window);
    if (win && ev->xexpose.count == 0) {
      MARU_Event mevt = {0};
      _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_FRAME,
                           (MARU_Window *)win, &mevt);
    }
    return true;
  }
  case MapNotify: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xmap.window);
    if (win && win->pending_maximize_request &&
        win->base.attrs_effective.maximized) {
      win->pending_maximize_request = false;
      _maru_x11_send_net_wm_state_local(ctx, win, 1,
                                        ctx->net_wm_state_maximized_vert,
                                        ctx->net_wm_state_maximized_horz);
    }
    return true;
  }
  case FocusIn: {
    MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xfocus.window);
    if (win) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
      ctx->linux_common.xkb.focused_window = (MARU_Window *)win;
      (void)_maru_x11_apply_window_cursor_mode(ctx, win);
      _maru_x11_refresh_text_input_state(ctx, win);
      MARU_Event mevt = {0};
      mevt.presentation.changed_fields =
          MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED;
      mevt.presentation.focused = true;
      _maru_dispatch_event(&ctx->base,
                           MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED,
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
      memset(win->base.keyboard_state, 0, sizeof(win->base.keyboard_state));
      if (ctx->linux_common.xkb.focused_window == (MARU_Window *)win) {
        ctx->linux_common.xkb.focused_window = NULL;
      }
      MARU_Event mevt = {0};
      mevt.presentation.changed_fields =
          MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED;
      mevt.presentation.focused = false;
      _maru_dispatch_event(&ctx->base,
                           MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED,
                           (MARU_Window *)win, &mevt);
    }
    return true;
  }
  case ClientMessage: {
    const Atom message_type = ev->xclient.message_type;
    if (message_type == ctx->wm_protocols &&
        (Atom)ev->xclient.data.l[0] == ctx->wm_delete_window) {
      MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
      if (win) {
        MARU_Event mevt = {0};
        _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED,
                             (MARU_Window *)win, &mevt);
      }
      return true;
    }
    return false;
  }
  default:
    return false;
  }
}
