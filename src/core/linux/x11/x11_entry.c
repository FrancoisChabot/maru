#include "maru_internal.h"
#include "maru_backend.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/cursors.h"
#include "maru/c/monitors.h"
#include "maru/c/native/linux.h"
#include "linux_internal.h"
#include "x11_internal.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static uint64_t _maru_x11_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return ((uint64_t)ts.tv_sec * 1000ull) + ((uint64_t)ts.tv_nsec / 1000000ull);
}

static void _maru_x11_dispatch_presentation_state(MARU_Window_X11 *window,
                                                   uint32_t changed_fields,
                                                   bool icon_effective) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)window->base.ctx_base;
  MARU_Event evt = {0};
  evt.presentation.changed_fields = changed_fields;
  evt.presentation.visible = (window->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
  evt.presentation.minimized = (window->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
  evt.presentation.maximized = (window->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
  evt.presentation.focused = (window->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
  evt.presentation.icon_effective = icon_effective;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED,
                       (MARU_Window *)window, &evt);
}

static void _maru_x11_send_net_wm_state(MARU_Context_X11 *ctx, MARU_Window_X11 *win,
                                        long action, Atom atom1, Atom atom2) {
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
                          SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

static void _maru_x11_apply_size_hints(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
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

static void _maru_x11_apply_cursor(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (win->base.pub.cursor_mode != MARU_CURSOR_NORMAL) {
    return;
  }

  if (win->base.pub.current_cursor) {
    MARU_Cursor_X11 *cursor = (MARU_Cursor_X11 *)win->base.pub.current_cursor;
    if (cursor->base.ctx_base == &ctx->base && cursor->handle) {
      ctx->x11_lib.XDefineCursor(ctx->display, win->handle, cursor->handle);
      return;
    }
  }

  ctx->x11_lib.XUndefineCursor(ctx->display, win->handle);
}

extern MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context);
extern MARU_Status maru_destroyContext_X11(MARU_Context *context);
extern MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask,
                                    const MARU_ContextAttributes *attributes);
extern MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, 
                                        MARU_EventCallback callback, void *userdata);
extern MARU_Status maru_wakeContext_X11(MARU_Context *context);
extern void *maru_getContextNativeHandle_X11(MARU_Context *context);
extern void *maru_getWindowNativeHandle_X11(MARU_Window *window);

extern MARU_Status maru_createWindow_X11(MARU_Context *context,
                                 const MARU_WindowCreateInfo *create_info,
                                 MARU_Window **out_window);
extern MARU_Status maru_destroyWindow_X11(MARU_Window *window);
extern void maru_getWindowGeometry_X11(MARU_Window *window,
                               MARU_WindowGeometry *out_geometry);

extern MARU_Status maru_createCursor_X11(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor);
extern MARU_Status maru_destroyCursor_X11(MARU_Cursor *cursor);
extern MARU_Status maru_createImage_X11(MARU_Context *context,
                                        const MARU_ImageCreateInfo *create_info,
                                        MARU_Image **out_image);
extern MARU_Status maru_destroyImage_X11(MARU_Image *image);

extern MARU_Monitor *const *maru_getMonitors_X11(MARU_Context *context, uint32_t *out_count);
extern void maru_retainMonitor_X11(MARU_Monitor *monitor);
extern void maru_releaseMonitor_X11(MARU_Monitor *monitor);
extern const MARU_VideoMode *maru_getMonitorModes_X11(const MARU_Monitor *monitor, uint32_t *out_count);
extern MARU_Status maru_setMonitorMode_X11(const MARU_Monitor *monitor, MARU_VideoMode mode);
extern void maru_resetMonitorMetrics_X11(MARU_Monitor *monitor);

MARU_Status maru_updateWindow_X11(MARU_Window *window, uint64_t field_mask,
                                  const MARU_WindowAttributes *attributes) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  MARU_WindowAttributes *requested = &win->base.attrs_requested;
  MARU_WindowAttributes *effective = &win->base.attrs_effective;
  MARU_Status status = MARU_SUCCESS;
  uint32_t presentation_changed = 0u;

  win->base.attrs_dirty_mask |= field_mask;

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
                            win->base.pub.title ? win->base.pub.title : "MARU Window");
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
    ctx->x11_lib.XMoveWindow(ctx->display, win->handle, (int)effective->position.x,
                             (int)effective->position.y);
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
    requested->fullscreen = attributes->fullscreen;
    effective->fullscreen = attributes->fullscreen;
    if (effective->fullscreen) {
      win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
      _maru_x11_send_net_wm_state(ctx, win, 1, ctx->net_wm_state_fullscreen, None);
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_FULLSCREEN);
      _maru_x11_send_net_wm_state(ctx, win, 0, ctx->net_wm_state_fullscreen, None);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
    requested->maximized = attributes->maximized;
    const bool was_maximized = (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
    if (requested->maximized) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
      _maru_x11_send_net_wm_state(ctx, win, 1, ctx->net_wm_state_maximized_vert,
                                  ctx->net_wm_state_maximized_horz);
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MAXIMIZED);
      _maru_x11_send_net_wm_state(ctx, win, 0, ctx->net_wm_state_maximized_vert,
                                  ctx->net_wm_state_maximized_horz);
    }
    effective->maximized = (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
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
  if (field_mask & (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE |
                    MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE)) {
    _maru_x11_apply_size_hints(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    requested->visible = attributes->visible;
    const bool was_visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    const bool was_minimized = (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;

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
    const bool was_minimized = (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
    const bool was_visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;

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

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
    requested->event_mask = attributes->event_mask;
    effective->event_mask = attributes->event_mask;
    win->base.pub.event_mask = attributes->event_mask;
  }

  if (field_mask & MARU_WINDOW_ATTR_ACCEPT_DROP) {
    requested->accept_drop = attributes->accept_drop;
    effective->accept_drop = attributes->accept_drop;
  }

  if (field_mask & MARU_WINDOW_ATTR_PRIMARY_SELECTION) {
    requested->primary_selection = attributes->primary_selection;
    effective->primary_selection = attributes->primary_selection;
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    if (attributes->cursor &&
        ((MARU_Cursor_X11 *)attributes->cursor)->base.ctx_base != &ctx->base) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_INVALID_ARGUMENT,
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
    if (attributes->cursor_mode != MARU_CURSOR_NORMAL) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 cursor hidden/locked modes are not implemented yet");
      status = MARU_FAILURE;
    }
  }

  if (field_mask & (MARU_WINDOW_ATTR_CURSOR | MARU_WINDOW_ATTR_CURSOR_MODE)) {
    _maru_x11_apply_cursor(ctx, win);
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
    requested->mouse_passthrough = attributes->mouse_passthrough;
    effective->mouse_passthrough = attributes->mouse_passthrough;
    if (effective->mouse_passthrough) {
      win->base.pub.flags |= MARU_WINDOW_STATE_MOUSE_PASSTHROUGH;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 mouse passthrough is not implemented yet");
      status = MARU_FAILURE;
    } else {
      win->base.pub.flags &= ~((uint64_t)MARU_WINDOW_STATE_MOUSE_PASSTHROUGH);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
    requested->monitor = attributes->monitor;
    effective->monitor = attributes->monitor;
    if (attributes->monitor) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 monitor targeting is not implemented yet");
      status = MARU_FAILURE;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
    requested->text_input_type = attributes->text_input_type;
    effective->text_input_type = attributes->text_input_type;
    if (attributes->text_input_type != MARU_TEXT_INPUT_TYPE_NONE) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "X11 text input hints are not implemented yet");
      status = MARU_FAILURE;
    }
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
      win->base.surrounding_text_storage = (char *)maru_context_alloc(&ctx->base, len + 1);
      if (win->base.surrounding_text_storage) {
        memcpy(win->base.surrounding_text_storage, attributes->surrounding_text, len + 1);
        requested->surrounding_text = win->base.surrounding_text_storage;
        effective->surrounding_text = win->base.surrounding_text_storage;
      } else {
        status = MARU_FAILURE;
      }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET) {
    requested->surrounding_cursor_offset = attributes->surrounding_cursor_offset;
    requested->surrounding_anchor_offset = attributes->surrounding_anchor_offset;
    effective->surrounding_cursor_offset = attributes->surrounding_cursor_offset;
    effective->surrounding_anchor_offset = attributes->surrounding_anchor_offset;
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
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                           "X11 window icon updates are not implemented yet");
    status = MARU_FAILURE;
    presentation_changed |= MARU_WINDOW_PRESENTATION_CHANGED_ICON;
  }

  if (presentation_changed != 0) {
    _maru_x11_dispatch_presentation_state(win, presentation_changed, true);
  }

  ctx->x11_lib.XFlush(ctx->display);
  win->base.attrs_dirty_mask &= ~field_mask;
  return status;
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
                          SubstructureNotifyMask | SubstructureRedirectMask, &ev);
  ctx->x11_lib.XSetInputFocus(ctx->display, win->handle, RevertToParent, CurrentTime);
  ctx->x11_lib.XRaiseWindow(ctx->display, win->handle);
  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  MARU_Event evt = {0};
  evt.frame.timestamp_ms = (uint32_t)_maru_x11_now_ms();
  return _maru_post_event_internal(&ctx->base, MARU_EVENT_WINDOW_FRAME, window, &evt);
}

MARU_Status maru_requestWindowAttention_X11(MARU_Window *window) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  _maru_x11_send_net_wm_state(ctx, win, 1, ctx->net_wm_state_demands_attention, None);
  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

extern const char **maru_getVkExtensions_X11(const MARU_Context *context,
                                             uint32_t *out_count);
extern MARU_Status maru_createVkSurface_X11(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

static MARU_Status maru_getControllers_X11(MARU_Context *context,
                                           MARU_ControllerList *out_list) {
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)context;
  MARU_Context_Linux_Common *common = &ctx->linux_common;

  if (common->controller_count > ctx->controller_list_capacity) {
    uint32_t new_capacity = common->controller_count;
    if (new_capacity < 8) new_capacity = 8;
    MARU_Controller **new_list = (MARU_Controller **)maru_context_realloc(
        &ctx->base, ctx->controller_list_storage,
        ctx->controller_list_capacity * sizeof(MARU_Controller *),
        new_capacity * sizeof(MARU_Controller *));
    if (!new_list) return MARU_FAILURE;
    ctx->controller_list_storage = new_list;
    ctx->controller_list_capacity = new_capacity;
  }

  uint32_t i = 0;
  for (MARU_LinuxController * it = common->controllers; it; it = it->next) {
    ctx->controller_list_storage[i++] = (MARU_Controller *)it;
  }

  out_list->controllers = ctx->controller_list_storage;
  out_list->count = common->controller_count;
  return MARU_SUCCESS;
}

static MARU_Status maru_retainController_X11(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
  return MARU_SUCCESS;
}

static MARU_Status maru_releaseController_X11(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u) {
        MARU_Context_X11 *ctx = (MARU_Context_X11 *)ctrl->base.context;
        _maru_linux_controller_destroy(&ctx->base, ctrl);
      }
      break;
    }
  }
  return MARU_SUCCESS;
}

static MARU_Status maru_resetControllerMetrics_X11(MARU_Controller *controller) {
  (void)controller;
  return MARU_SUCCESS;
}

static MARU_Status maru_getControllerInfo_X11(MARU_Controller *controller,
                                              MARU_ControllerInfo *out_info) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  memset(out_info, 0, sizeof(*out_info));
  out_info->name = ctrl->name;
  out_info->vendor_id = ctrl->vendor_id;
  out_info->product_id = ctrl->product_id;
  out_info->version = ctrl->version;
  memcpy(out_info->guid, ctrl->guid, 16);
  out_info->is_standardized = ctrl->is_standardized;
  return MARU_SUCCESS;
}

static MARU_Status
maru_setControllerHapticLevels_X11(MARU_Controller *controller,
                                   uint32_t first_haptic, uint32_t count,
                                   const MARU_Scalar *intensities) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)ctrl->base.context;
  return _maru_linux_common_set_haptic_levels(&ctx->linux_common, ctrl, first_haptic, count, intensities);
}

static MARU_Status maru_announceData_X11(MARU_Window *window,
                                         MARU_DataExchangeTarget target,
                                         const char **mime_types, uint32_t count,
                                         MARU_DropActionMask allowed_actions) {
  (void)window;
  (void)target;
  (void)mime_types;
  (void)count;
  (void)allowed_actions;
  return MARU_FAILURE;
}

static MARU_Status
maru_provideData_X11(const MARU_DataRequestEvent *request_event, const void *data,
                     size_t size, MARU_DataProvideFlags flags) {
  (void)request_event;
  (void)data;
  (void)size;
  (void)flags;
  return MARU_FAILURE;
}

static MARU_Status maru_requestData_X11(MARU_Window *window,
                                        MARU_DataExchangeTarget target,
                                        const char *mime_type, void *user_tag) {
  (void)window;
  (void)target;
  (void)mime_type;
  (void)user_tag;
  return MARU_FAILURE;
}

static MARU_Status maru_getAvailableMIMETypes_X11(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
  (void)window;
  (void)target;
  out_list->mime_types = NULL;
  out_list->count = 0;
  return MARU_FAILURE;
}

#ifdef MARU_INDIRECT_BACKEND
static void *maru_getWindowNativeHandle_X11_internal(MARU_Window *window) {
  return maru_getWindowNativeHandle_X11(window);
}

const MARU_Backend maru_backend_X11 = {
  .destroyContext = maru_destroyContext_X11,
  .updateContext = maru_updateContext_X11,
  .pumpEvents = maru_pumpEvents_X11,
  .wakeContext = maru_wakeContext_X11,
  .createWindow = maru_createWindow_X11,
  .destroyWindow = maru_destroyWindow_X11,
  .getWindowGeometry = maru_getWindowGeometry_X11,
  .updateWindow = maru_updateWindow_X11,
  .requestWindowFocus = maru_requestWindowFocus_X11,
  .requestWindowFrame = maru_requestWindowFrame_X11,
  .requestWindowAttention = maru_requestWindowAttention_X11,
  .createCursor = maru_createCursor_X11,
  .destroyCursor = maru_destroyCursor_X11,
  .createImage = maru_createImage_X11,
  .destroyImage = maru_destroyImage_X11,
  .getControllers = maru_getControllers_X11,
  .retainController = maru_retainController_X11,
  .releaseController = maru_releaseController_X11,
  .resetControllerMetrics = maru_resetControllerMetrics_X11,
  .getControllerInfo = maru_getControllerInfo_X11,
  .setControllerHapticLevels = maru_setControllerHapticLevels_X11,
  .announceData = maru_announceData_X11,
  .provideData = maru_provideData_X11,
  .requestData = maru_requestData_X11,
  .getAvailableMIMETypes = maru_getAvailableMIMETypes_X11,
  .getMonitors = maru_getMonitors_X11,
  .retainMonitor = maru_retainMonitor_X11,
  .releaseMonitor = maru_releaseMonitor_X11,
  .getMonitorModes = maru_getMonitorModes_X11,
  .setMonitorMode = maru_setMonitorMode_X11,
  .resetMonitorMetrics = maru_resetMonitorMetrics_X11,
  .getContextNativeHandle = maru_getContextNativeHandle_X11,
  .getWindowNativeHandle = maru_getWindowNativeHandle_X11_internal,
  .getVkExtensions = maru_getVkExtensions_X11,
  .createVkSurface = maru_createVkSurface_X11,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_X11(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_X11(context);
}

MARU_API MARU_Status
maru_updateContext(MARU_Context *context, uint64_t field_mask,
                   const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  return maru_updateContext_X11(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_X11(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_X11(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_X11(window);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_X11(window, out_geometry);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_X11(window, field_mask, attributes);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_X11(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_X11(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_X11(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_X11(image);
}

MARU_API MARU_Status maru_getControllers(MARU_Context *context,
                                         MARU_ControllerList *out_list) {
  MARU_API_VALIDATE(getControllers, context, out_list);
  return maru_getControllers_X11(context, out_list);
}

MARU_API MARU_Status maru_retainController(MARU_Controller *controller) {
  MARU_API_VALIDATE(retainController, controller);
  return maru_retainController_X11(controller);
}

MARU_API MARU_Status maru_releaseController(MARU_Controller *controller) {
  MARU_API_VALIDATE(releaseController, controller);
  return maru_releaseController_X11(controller);
}

MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller *controller) {
  MARU_API_VALIDATE(resetControllerMetrics, controller);
  return maru_resetControllerMetrics_X11(controller);
}

MARU_API MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                            MARU_ControllerInfo *out_info) {
  MARU_API_VALIDATE(getControllerInfo, controller, out_info);
  return maru_getControllerInfo_X11(controller, out_info);
}

MARU_API MARU_Status
maru_setControllerHapticLevels(MARU_Controller *controller, uint32_t first_haptic,
                               uint32_t count,
                               const MARU_Scalar *intensities) {
  MARU_API_VALIDATE(setControllerHapticLevels, controller, first_haptic, count,
                    intensities);
  return maru_setControllerHapticLevels_X11(controller, first_haptic, count,
                                            intensities);
}

MARU_API MARU_Status maru_announceData(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       const char **mime_types, uint32_t count,
                                       MARU_DropActionMask allowed_actions) {
  MARU_API_VALIDATE(announceData, window, target, mime_types, count,
                    allowed_actions);
  return maru_announceData_X11(window, target, mime_types, count,
                               allowed_actions);
}

MARU_API MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event,
                                      const void *data, size_t size,
                                      MARU_DataProvideFlags flags) {
  MARU_API_VALIDATE(provideData, request_event, data, size, flags);
  return maru_provideData_X11(request_event, data, size, flags);
}

MARU_API MARU_Status maru_requestData(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char *mime_type, void *user_tag) {
  MARU_API_VALIDATE(requestData, window, target, mime_type, user_tag);
  return maru_requestData_X11(window, target, mime_type, user_tag);
}

MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list) {
  MARU_API_VALIDATE(getAvailableMIMETypes, window, target, out_list);
  return maru_getAvailableMIMETypes_X11(window, target, out_list);
}

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_X11(window);
}

MARU_API MARU_Status maru_requestWindowFrame(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFrame, window);
  return maru_requestWindowFrame_X11(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_X11(window);
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_X11(context);
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  return maru_getMonitors_X11(context, out_count);
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  maru_retainMonitor_X11(monitor);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  maru_releaseMonitor_X11(monitor);
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_X11(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_X11(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  maru_resetMonitorMetrics_X11(monitor);
}

MARU_API MARU_Status maru_getX11ContextHandle(MARU_Context *context,
                                              MARU_X11ContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->display = (Display *)maru_getContextNativeHandle_X11(context);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getX11WindowHandle(MARU_Window *window,
                                             MARU_X11WindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  out_handle->display = (Display *)maru_getContextNativeHandle_X11(context);
  out_handle->window = (Window)(uintptr_t)maru_getWindowNativeHandle_X11(window);
  return out_handle->display ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandContextHandle(
    MARU_Context *context, MARU_WaylandContextHandle *out_handle) {
  (void)context;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getWaylandWindowHandle(
    MARU_Window *window, MARU_WaylandWindowHandle *out_handle) {
  (void)window;
  if (!out_handle) return MARU_FAILURE;
  out_handle->display = NULL;
  out_handle->surface = NULL;
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getLinuxContextHandle(
    MARU_Context *context, MARU_LinuxContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_X11;
  return maru_getX11ContextHandle(context, &out_handle->x11);
}

MARU_API MARU_Status maru_getLinuxWindowHandle(
    MARU_Window *window, MARU_LinuxWindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  out_handle->backend = MARU_BACKEND_X11;
  return maru_getX11WindowHandle(window, &out_handle->x11);
}
#endif
