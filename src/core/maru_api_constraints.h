// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

static inline MARU_Status
_maru_status_if_context_lost(const MARU_Context *context) {
  if (maru_isContextLost(context)) {
    return MARU_ERROR_CONTEXT_LOST;
  }
  return MARU_SUCCESS;
}

static inline MARU_Status
_maru_status_if_window_context_lost(const MARU_Window *window) {
  return _maru_status_if_context_lost(maru_getWindowContext(window));
}

static inline MARU_Status
_maru_status_if_cursor_context_lost(const MARU_Cursor *cursor) {
  return _maru_status_if_context_lost(
      (const MARU_Context *)((const MARU_Cursor_Base *)cursor)->ctx_base);
}

static inline MARU_Status
_maru_status_if_image_context_lost(const MARU_Image *image) {
  return _maru_status_if_context_lost(
      (const MARU_Context *)((const MARU_Image_Base *)image)->ctx_base);
}

static inline MARU_Status
_maru_status_if_monitor_context_lost(const MARU_Monitor *monitor) {
  return _maru_status_if_context_lost(maru_getMonitorContext(monitor));
}

static inline MARU_Status
_maru_status_if_controller_context_lost(const MARU_Controller *controller) {
  return _maru_status_if_context_lost(maru_getControllerContext(controller));
}

static inline MARU_Status
_maru_status_if_request_context_lost(MARU_DataRequest *request) {
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request;
  return _maru_status_if_context_lost((const MARU_Context *)handle->ctx_base);
}

#define MARU_RETURN_IF_CONTEXT_LOST(expr)                                      \
  do {                                                                         \
    const MARU_Status _maru_status = (expr);                                   \
    if (_maru_status != MARU_SUCCESS) {                                        \
      return _maru_status;                                                     \
    }                                                                          \
  } while (0)

#ifdef MARU_ENABLE_INTERNAL_CHECKS
#define MARU_ASSUME(cond)                                                      \
  do {                                                                         \
    if (!(cond))                                                               \
      abort();                                                                 \
  } while (0)
#else
#define MARU_ASSUME(cond) (void)0
#endif

#ifdef MARU_VALIDATE_API_CALLS

#define MARU_CONSTRAINT_CHECK(cond)                                            \
  do {                                                                         \
    if (!(cond))                                                               \
      abort();                                                                 \
  } while (0)

#include "maru/c/instrumentation.h"
#include "maru/c/vulkan.h"
#ifdef MARU_ENABLE_DIAGNOSTICS
extern void _maru_reportDiagnostic(const MARU_Context *ctx,
                                   MARU_Diagnostic diag, const char *msg);
#endif
extern MARU_ThreadId _maru_getCurrentThreadId(void);

static inline void _maru_validate_thread(const MARU_Context_Base *ctx_base) {
  MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId());
}

static inline void _maru_validate_window_ready(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(maru_isWindowReady(window));
}

static inline void _maru_validate_window_not_lost(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(!maru_isWindowLost(window));
}

static inline void
_maru_validate_monitor_not_lost(const MARU_Monitor *monitor) {
  MARU_CONSTRAINT_CHECK(!maru_isMonitorLost(monitor));
}

static inline void
_maru_validate_controller_not_lost(const MARU_Controller *controller) {
  MARU_CONSTRAINT_CHECK(!maru_isControllerLost(controller));
}

static inline bool _maru_validate_non_negative_vec2(MARU_Vec2Dip v) {
  return v.x >= 0 && v.y >= 0;
}

static inline bool _maru_validate_positive_vec2px(MARU_Vec2Px v) {
  return v.x > 0 && v.y > 0;
}

static inline bool _maru_validate_non_negative_vec2px(MARU_Vec2Px v) {
  return v.x >= 0 && v.y >= 0;
}

static inline bool _maru_validate_non_negative_rect(MARU_RectDip r) {
  return r.dip_size.x >= 0 && r.dip_size.y >= 0;
}

static inline bool _maru_validate_aspect_ratio(MARU_Fraction f) {
  if (f.num < 0 || f.denom < 0) {
    return false;
  }
  if (f.num == 0 || f.denom == 0) {
    return (f.num == 0 && f.denom == 0);
  }
  return true;
}

static inline void
_maru_validate_attributes(const MARU_Context_Base *ctx_base,
                          uint64_t field_mask,
                          const MARU_WindowAttributes *attributes) {
  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    MARU_CONSTRAINT_CHECK(attributes->cursor_mode >= MARU_CURSOR_NORMAL &&
                          attributes->cursor_mode <= MARU_CURSOR_LOCKED);
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
    MARU_CONSTRAINT_CHECK(
        attributes->text_input_type >= MARU_TEXT_INPUT_TYPE_NONE &&
        attributes->text_input_type <= MARU_TEXT_INPUT_TYPE_NUMERIC);
  }

  if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_aspect_ratio(attributes->aspect_ratio));
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->dip_size));
  }

  if (field_mask & MARU_WINDOW_ATTR_MIN_DIP_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->min_dip_size));
  }

  if (field_mask & MARU_WINDOW_ATTR_MAX_DIP_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->max_dip_size));
  }

  if ((field_mask & (MARU_WINDOW_ATTR_MIN_DIP_SIZE | MARU_WINDOW_ATTR_MAX_DIP_SIZE)) ==
      (MARU_WINDOW_ATTR_MIN_DIP_SIZE | MARU_WINDOW_ATTR_MAX_DIP_SIZE)) {
    const bool max_unbounded_x = attributes->max_dip_size.x == 0;
    const bool max_unbounded_y = attributes->max_dip_size.y == 0;
    if (!max_unbounded_x) {
      MARU_CONSTRAINT_CHECK(attributes->max_dip_size.x >= attributes->min_dip_size.x);
    }
    if (!max_unbounded_y) {
      MARU_CONSTRAINT_CHECK(attributes->max_dip_size.y >= attributes->min_dip_size.y);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_RECT) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_rect(attributes->text_input_rect));
  }

  if (field_mask & MARU_WINDOW_ATTR_VIEWPORT_DIP_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->viewport_dip_size));
  }

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    MARU_CONSTRAINT_CHECK(attributes->title != NULL);
  }

  if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
    if (attributes->monitor != NULL) {
      MARU_CONSTRAINT_CHECK(
          ((const MARU_Monitor_Base *)attributes->monitor)->ctx_base ==
          ctx_base);
      _maru_validate_monitor_not_lost(
          (const MARU_Monitor *)attributes->monitor);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    if (attributes->cursor != NULL) {
      MARU_CONSTRAINT_CHECK(
          ((const MARU_Cursor_Base *)attributes->cursor)->ctx_base == ctx_base);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_ICON) {
    if (attributes->icon != NULL) {
      MARU_CONSTRAINT_CHECK(
          ((const MARU_Image_Base *)attributes->icon)->ctx_base == ctx_base);
    }
  }
}

static inline void
_maru_validate_createContext(const MARU_ContextCreateInfo *create_info,
                             MARU_Context **out_context) {
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_context != NULL);
}

static inline void _maru_validate_destroyContext(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void
_maru_validate_updateContext(MARU_Context *context, uint64_t field_mask,
                             const MARU_ContextAttributes *attributes) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(attributes != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);

  const uint64_t known_fields = MARU_CONTEXT_ATTR_ALL;
  MARU_CONSTRAINT_CHECK((field_mask & ~known_fields) == 0);

  if (field_mask & MARU_CONTEXT_ATTR_DEFAULT_CURSOR) {
    if (attributes->default_cursor != NULL) {
      MARU_CONSTRAINT_CHECK(
          ((const MARU_Cursor_Base *)attributes->default_cursor)->ctx_base ==
          (const MARU_Context_Base *)context);
    }
  }
}

static inline void _maru_validate_pumpEvents(MARU_Context *context,
                                             uint32_t timeout_ms,
                                             MARU_EventMask mask,
                                             MARU_EventCallback callback,
                                             void *userdata) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  if (mask != 0) {
    MARU_CONSTRAINT_CHECK(callback != NULL);
  }
  _maru_validate_thread((const MARU_Context_Base *)context);
  (void)timeout_ms;
  (void)mask;
  (void)userdata;
}

static inline void
_maru_validate_createWindow(MARU_Context *context,
                            const MARU_WindowCreateInfo *create_info,
                            MARU_Window **out_window) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_window != NULL);

  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  _maru_validate_thread(ctx_base);
  _maru_validate_attributes(ctx_base, MARU_WINDOW_ATTR_ALL,
                            &create_info->attributes);
}

static inline void _maru_validate_destroyWindow(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void
_maru_validate_updateWindow(MARU_Window *window, uint64_t field_mask,
                            const MARU_WindowAttributes *attributes) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  MARU_CONSTRAINT_CHECK(attributes != NULL);

  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  _maru_validate_thread(win_base->ctx_base);

  const uint64_t known_fields = MARU_WINDOW_ATTR_ALL;

  MARU_CONSTRAINT_CHECK((field_mask & ~known_fields) == 0);
  _maru_validate_attributes(win_base->ctx_base, field_mask, attributes);
}

static inline void _maru_validate_requestWindowFocus(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_requestWindowFrame(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_requestWindowAttention(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_resetContextMetrics(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_resetWindowMetrics(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void
_maru_validate_createCursor(MARU_Context *context,
                            const MARU_CursorCreateInfo *create_info,
                            MARU_Cursor **out_cursor) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_cursor != NULL);

  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  _maru_validate_thread(ctx_base);

  MARU_CONSTRAINT_CHECK(create_info->source == MARU_CURSOR_SOURCE_CUSTOM ||
                        create_info->source == MARU_CURSOR_SOURCE_SYSTEM);

  if (create_info->source == MARU_CURSOR_SOURCE_CUSTOM) {
    MARU_CONSTRAINT_CHECK(create_info->frame_count > 0);
    MARU_CONSTRAINT_CHECK(create_info->frames != NULL);
    for (uint32_t i = 0; i < create_info->frame_count; ++i) {
      MARU_CONSTRAINT_CHECK(create_info->frames[i].image != NULL);
      const MARU_Image_Base *image_base =
          (const MARU_Image_Base *)create_info->frames[i].image;
      MARU_CONSTRAINT_CHECK(image_base->ctx_base == ctx_base);
      MARU_CONSTRAINT_CHECK(
          _maru_validate_non_negative_vec2px(create_info->frames[i].px_hot_spot));
      MARU_CONSTRAINT_CHECK((uint32_t)create_info->frames[i].px_hot_spot.x <
                            image_base->width);
      MARU_CONSTRAINT_CHECK((uint32_t)create_info->frames[i].px_hot_spot.y <
                            image_base->height);
    }
  } else {
    MARU_CONSTRAINT_CHECK(
        create_info->system_shape >= MARU_CURSOR_SHAPE_DEFAULT &&
        create_info->system_shape <= MARU_CURSOR_SHAPE_NWSE_RESIZE);
  }
}

static inline void _maru_validate_destroyCursor(MARU_Cursor *cursor) {
  MARU_CONSTRAINT_CHECK(cursor != NULL);
  _maru_validate_thread(((const MARU_Cursor_Base *)cursor)->ctx_base);
}

static inline void _maru_validate_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_CONSTRAINT_CHECK(cursor != NULL);
  _maru_validate_thread(((const MARU_Cursor_Base *)cursor)->ctx_base);
}

static inline void
_maru_validate_createImage(MARU_Context *context,
                           const MARU_ImageCreateInfo *create_info,
                           MARU_Image **out_image) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_image != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);

  MARU_CONSTRAINT_CHECK(_maru_validate_positive_vec2px(create_info->px_size));
  MARU_CONSTRAINT_CHECK(create_info->pixels != NULL);
  const uint64_t min_stride_u64 = (uint64_t)(uint32_t)create_info->px_size.x * 4u;
  MARU_CONSTRAINT_CHECK(min_stride_u64 <= UINT32_MAX);
  if (create_info->stride_bytes != 0) {
    MARU_CONSTRAINT_CHECK((uint64_t)create_info->stride_bytes >= min_stride_u64);
  }
}

static inline void _maru_validate_destroyImage(MARU_Image *image) {
  MARU_CONSTRAINT_CHECK(image != NULL);
  _maru_validate_thread(((const MARU_Image_Base *)image)->ctx_base);
}

static inline void
_maru_validate_getControllers(MARU_Context *context,
                              MARU_ControllerList *out_list) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void
_maru_validate_retainController(MARU_Controller *controller) {
  MARU_CONSTRAINT_CHECK(controller != NULL);
}

static inline void
_maru_validate_releaseController(MARU_Controller *controller) {
  MARU_CONSTRAINT_CHECK(controller != NULL);
}

static inline void
_maru_validate_resetControllerMetrics(MARU_Controller *controller) {
  MARU_CONSTRAINT_CHECK(controller != NULL);
  _maru_validate_thread(
      (const MARU_Context_Base *)maru_getControllerContext(controller));
}

static inline void
_maru_validate_getControllerInfo(const MARU_Controller *controller,
                                 MARU_ControllerInfo *out_info) {
  MARU_CONSTRAINT_CHECK(controller != NULL);
  MARU_CONSTRAINT_CHECK(out_info != NULL);
  _maru_validate_thread(
      (const MARU_Context_Base *)maru_getControllerContext(controller));
}

static inline void
_maru_validate_setControllerHapticLevels(MARU_Controller *controller,
                                         uint32_t first_haptic, uint32_t count,
                                         const MARU_Scalar *intensities) {
  MARU_CONSTRAINT_CHECK(controller != NULL);
  _maru_validate_thread(
      (const MARU_Context_Base *)maru_getControllerContext(controller));
  const uint32_t haptic_count = maru_getControllerHapticCount(controller);
  MARU_CONSTRAINT_CHECK(first_haptic <= haptic_count);
  MARU_CONSTRAINT_CHECK(count <= (haptic_count - first_haptic));
  if (count > 0) {
    MARU_CONSTRAINT_CHECK(intensities != NULL);
  }
}

static inline void
_maru_validate_announceData(MARU_Window *window, MARU_DataExchangeTarget target,
                            const char **mime_types, uint32_t count,
                            MARU_DropActionMask allowed_actions) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                        target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  const MARU_DropActionMask known_actions = (MARU_DropActionMask)(
      MARU_DROP_ACTION_COPY | MARU_DROP_ACTION_MOVE | MARU_DROP_ACTION_LINK);
  MARU_CONSTRAINT_CHECK((allowed_actions & ~known_actions) == 0);
  if (count > 0) {
    MARU_CONSTRAINT_CHECK(mime_types != NULL);
    for (uint32_t i = 0; i < count; ++i) {
      MARU_CONSTRAINT_CHECK(mime_types[i] != NULL);
      MARU_CONSTRAINT_CHECK(mime_types[i][0] != '\0');
    }
  }
}

static inline void
_maru_validate_provideData(MARU_DataRequest *request,
                           const void *data, size_t size,
                           MARU_DataProvideFlags flags) {
  MARU_CONSTRAINT_CHECK(request != NULL);
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request;
  _maru_validate_thread(handle->ctx_base);
  if (size > 0) {
    MARU_CONSTRAINT_CHECK(data != NULL);
  }
  const uint32_t known_flags = (uint32_t)MARU_DATA_PROVIDE_FLAG_ZERO_COPY;
  MARU_CONSTRAINT_CHECK((((uint32_t)flags) & ~known_flags) == 0);
}

static inline void _maru_validate_requestData(MARU_Window *window,
                                              MARU_DataExchangeTarget target,
                                              const char *mime_type,
                                              void *userdata) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                        target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  MARU_CONSTRAINT_CHECK(mime_type != NULL);
  MARU_CONSTRAINT_CHECK(mime_type[0] != '\0');
  (void)userdata;
}

static inline void
_maru_validate_getAvailableMIMETypes(MARU_Window *window,
                                     MARU_DataExchangeTarget target,
                                     MARU_MIMETypeList *out_list) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                        target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void _maru_validate_wakeContext(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void _maru_validate_postEvent(MARU_Context *context,
                                            MARU_EventId type,
                                            MARU_Window *window,
                                            MARU_UserDefinedEvent evt) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_isUserEventId(type));
  if (window != NULL) {
    MARU_CONSTRAINT_CHECK(maru_getWindowContext(window) == context);
  }
  (void)type;
  (void)evt;
}

static inline void _maru_validate_getMonitors(MARU_Context *context,
                                              MARU_MonitorList *out_list) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_retainMonitor(MARU_Monitor *monitor) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_releaseMonitor(MARU_Monitor *monitor) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_getMonitorModes(const MARU_Monitor *monitor,
                                                  MARU_VideoModeList *out_list) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void _maru_validate_setMonitorMode(const MARU_Monitor *monitor,
                                                 MARU_VideoMode mode) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
  _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
  MARU_CONSTRAINT_CHECK(mode.px_size.x > 0 && mode.px_size.y > 0);
}

static inline void _maru_validate_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
  _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
}

static inline void _maru_validate_getWaylandContextHandle(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WAYLAND);
}

static inline void _maru_validate_getWaylandWindowHandle(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
  MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WAYLAND);
}

static inline void _maru_validate_getX11ContextHandle(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_X11);
}

static inline void _maru_validate_getX11WindowHandle(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
  MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_X11);
}

static inline void _maru_validate_getWin32ContextHandle(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WINDOWS);
}

static inline void _maru_validate_getWin32WindowHandle(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
  MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WINDOWS);
}

static inline void _maru_validate_getCocoaContextHandle(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_COCOA);
}

static inline void _maru_validate_getCocoaWindowHandle(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
  MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_COCOA);
}

static inline void _maru_validate_getLinuxContextHandle(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  const MARU_BackendType backend = maru_getContextBackend(context);
  MARU_CONSTRAINT_CHECK(backend == MARU_BACKEND_WAYLAND ||
                        backend == MARU_BACKEND_X11);
}

static inline void _maru_validate_getLinuxWindowHandle(MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
  MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  const MARU_BackendType backend = maru_getContextBackend(context);
  MARU_CONSTRAINT_CHECK(backend == MARU_BACKEND_WAYLAND ||
                        backend == MARU_BACKEND_X11);
}

static inline void _maru_validate_getVkExtensions(const MARU_Context *context,
                                                  MARU_VkExtensionList *out_list) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void
_maru_validate_createVkSurface(MARU_Window *window, VkInstance instance,
                               MARU_VkGetInstanceProcAddrFunc vk_loader,
                               VkSurfaceKHR *out_surface) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  MARU_CONSTRAINT_CHECK(instance != NULL);
  MARU_CONSTRAINT_CHECK(vk_loader != NULL);
  MARU_CONSTRAINT_CHECK(out_surface != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

#define MARU_API_VALIDATE(fn, ...) _maru_validate_##fn(__VA_ARGS__)

static inline void
_maru_validate_live_updateWindow(MARU_Window *window, uint64_t field_mask,
                                 const MARU_WindowAttributes *attributes) {
  (void)field_mask;
  (void)attributes;
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_requestWindowFocus(MARU_Window *window) {
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_requestWindowFrame(MARU_Window *window) {
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void
_maru_validate_live_requestWindowAttention(MARU_Window *window) {
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_resetWindowMetrics(MARU_Window *window) {
  _maru_validate_window_not_lost(window);
}

static inline void
_maru_validate_live_resetControllerMetrics(MARU_Controller *controller) {
  _maru_validate_controller_not_lost(controller);
}

static inline void
_maru_validate_live_getControllerInfo(const MARU_Controller *controller,
                                      MARU_ControllerInfo *out_info) {
  (void)out_info;
  _maru_validate_controller_not_lost(controller);
}

static inline void _maru_validate_live_setControllerHapticLevels(
    MARU_Controller *controller, uint32_t first_haptic, uint32_t count,
    const MARU_Scalar *intensities) {
  (void)first_haptic;
  (void)count;
  (void)intensities;
  _maru_validate_controller_not_lost(controller);
}

static inline void _maru_validate_live_announceData(
    MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types,
    uint32_t count, MARU_DropActionMask allowed_actions) {
  (void)target;
  (void)mime_types;
  (void)count;
  (void)allowed_actions;
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_requestData(
    MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type,
    void *userdata) {
  (void)target;
  (void)mime_type;
  (void)userdata;
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_getAvailableMIMETypes(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
  (void)target;
  (void)out_list;
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_live_getMonitorModes(
    const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  (void)out_list;
  _maru_validate_monitor_not_lost(monitor);
}

static inline void
_maru_validate_live_setMonitorMode(const MARU_Monitor *monitor,
                                   MARU_VideoMode mode) {
  (void)mode;
  _maru_validate_monitor_not_lost(monitor);
}

static inline void
_maru_validate_live_resetMonitorMetrics(MARU_Monitor *monitor) {
  _maru_validate_monitor_not_lost(monitor);
}

static inline void _maru_validate_live_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  (void)instance;
  (void)vk_loader;
  (void)out_surface;
  _maru_validate_window_ready(window);
  _maru_validate_window_not_lost(window);
}

#define MARU_API_VALIDATE_LIVE(fn, ...) _maru_validate_live_##fn(__VA_ARGS__)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#define MARU_API_VALIDATE_LIVE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
