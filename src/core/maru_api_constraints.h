// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

static inline MARU_Status
_maru_status_if_context_lost(const MARU_Context *context) {
  if (maru_isContextLost(context)) {
    return MARU_CONTEXT_LOST;
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
_maru_status_if_monitor_lost(const MARU_Monitor *monitor) {
  if (maru_isMonitorLost(monitor)) {
    return MARU_FAILURE;
  }
  return MARU_SUCCESS;
}

static inline MARU_Status
_maru_status_if_controller_lost(const MARU_Controller *controller) {
  if (maru_isControllerLost(controller)) {
    return MARU_FAILURE;
  }
  return MARU_SUCCESS;
}

static inline MARU_Status
_maru_status_if_request_context_lost(MARU_DataRequest *request) {
  const MARU_DataRequestHandleBase *handle =
      (const MARU_DataRequestHandleBase *)request;
  return _maru_status_if_context_lost((const MARU_Context *)handle->ctx_base);
}

#define MARU_RETURN_ON_ERROR(expr)                                      \
  do {                                                                         \
    const MARU_Status _maru_status = (expr);                                   \
    if (_maru_status != MARU_SUCCESS) {                                        \
      return _maru_status;                                                     \
    }                                                                          \
  } while (0)

#ifdef MARU_VALIDATE_API_CALLS

#define MARU_CONSTRAINT_CHECK(cond)                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "Maru API contract violation in %s\n", __func__);        \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#include "maru/maru.h"
#include "maru/queue.h"
#ifdef MARU_ENABLE_DIAGNOSTICS
extern void _maru_reportDiagnostic(const MARU_Context *ctx,
                                   MARU_Diagnostic diag, const char *msg);
extern void _maru_reportDiagnosticOn(const MARU_Context *ctx,
                                     MARU_DiagnosticSubjectKind subject_kind,
                                     MARU_DiagnosticSubject subject,
                                     MARU_Diagnostic diag, const char *msg);
#endif
extern MARU_ThreadId _maru_getCurrentThreadId(void);
extern void _maru_validate_queue_creator_thread(const MARU_Queue *queue);

static inline void _maru_validate_thread(const MARU_Context_Base *ctx_base) {
  MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId());
}

static inline void _maru_validate_window_ready(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(maru_isWindowReady(window));
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
  return r.size.x >= 0 && r.size.y >= 0;
}

static inline bool _maru_validate_allocator_complete(MARU_Allocator allocator) {
  const bool any_custom = allocator.alloc_cb != NULL || allocator.realloc_cb != NULL ||
                          allocator.free_cb != NULL;
  if (!any_custom) {
    return true;
  }
  return allocator.alloc_cb != NULL && allocator.realloc_cb != NULL &&
         allocator.free_cb != NULL;
}

static inline bool _maru_validate_optional_power_of_two_u32(uint32_t value) {
  return value == 0u || (value & (value - 1u)) == 0u;
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

static inline bool _maru_is_utf8_continuation_byte(unsigned char byte) {
  return (byte & 0xC0u) == 0x80u;
}

static inline bool _maru_validate_utf8_sequence(const char *text,
                                                uint32_t length_bytes) {
  uint32_t i = 0;
  while (i < length_bytes) {
    const unsigned char lead = (unsigned char)text[i];

    if (lead <= 0x7Fu) {
      ++i;
      continue;
    }

    if (lead >= 0xC2u && lead <= 0xDFu) {
      if (i + 1u >= length_bytes ||
          !_maru_is_utf8_continuation_byte((unsigned char)text[i + 1u])) {
        return false;
      }
      i += 2u;
      continue;
    }

    if (lead >= 0xE0u && lead <= 0xEFu) {
      const unsigned char b1 =
          (i + 1u < length_bytes) ? (unsigned char)text[i + 1u] : 0u;
      const unsigned char b2 =
          (i + 2u < length_bytes) ? (unsigned char)text[i + 2u] : 0u;
      if (i + 2u >= length_bytes ||
          !_maru_is_utf8_continuation_byte(b1) ||
          !_maru_is_utf8_continuation_byte(b2)) {
        return false;
      }
      if ((lead == 0xE0u && b1 < 0xA0u) ||
          (lead == 0xEDu && b1 >= 0xA0u)) {
        return false;
      }
      i += 3u;
      continue;
    }

    if (lead >= 0xF0u && lead <= 0xF4u) {
      const unsigned char b1 =
          (i + 1u < length_bytes) ? (unsigned char)text[i + 1u] : 0u;
      const unsigned char b2 =
          (i + 2u < length_bytes) ? (unsigned char)text[i + 2u] : 0u;
      const unsigned char b3 =
          (i + 3u < length_bytes) ? (unsigned char)text[i + 3u] : 0u;
      if (i + 3u >= length_bytes ||
          !_maru_is_utf8_continuation_byte(b1) ||
          !_maru_is_utf8_continuation_byte(b2) ||
          !_maru_is_utf8_continuation_byte(b3)) {
        return false;
      }
      if ((lead == 0xF0u && b1 < 0x90u) ||
          (lead == 0xF4u && b1 >= 0x90u)) {
        return false;
      }
      i += 4u;
      continue;
    }

    return false;
  }

  return true;
}

static inline bool _maru_validate_utf8_string(const char *text) {
  uint32_t length = 0;
  while (text[length] != '\0') {
    if (length == UINT32_MAX) {
      return false;
    }
    ++length;
  }

  return _maru_validate_utf8_sequence(text, length);
}

static inline bool _maru_validate_utf8_boundary(const char *text,
                                                uint32_t byte_offset) {
  uint32_t i = 0;

  while (true) {
    if (i == byte_offset) {
      return true;
    }
    if (text[i] == '\0' || i > byte_offset) {
      return false;
    }

    const unsigned char lead = (unsigned char)text[i];
    if (lead <= 0x7Fu) {
      ++i;
    } else if (lead >= 0xC2u && lead <= 0xDFu) {
      i += 2u;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
      i += 3u;
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
      i += 4u;
    } else {
      return false;
    }
  }
}

static inline bool
_maru_validate_surrounding_text_tuple(const char *text,
                                      uint32_t cursor_byte,
                                      uint32_t anchor_byte) {
  if (text == NULL) {
    return cursor_byte == 0u && anchor_byte == 0u;
  }

  if (!_maru_validate_utf8_string(text)) {
    return false;
  }

  return _maru_validate_utf8_boundary(text, cursor_byte) &&
         _maru_validate_utf8_boundary(text, anchor_byte);
}

static inline bool
_maru_validate_presentation_state(MARU_WindowPresentationState state) {
  return state >= MARU_WINDOW_PRESENTATION_NORMAL &&
         state <= MARU_WINDOW_PRESENTATION_MINIMIZED;
}

static inline bool
_maru_validate_presentation_visibility(MARU_WindowPresentationState state,
                                        bool visible) {
  if ((state == MARU_WINDOW_PRESENTATION_FULLSCREEN ||
       state == MARU_WINDOW_PRESENTATION_MAXIMIZED) &&
      !visible) {
    return false;
  }
  if (state == MARU_WINDOW_PRESENTATION_MINIMIZED && visible) {
    return false;
  }
  return true;
}

static inline void
_maru_window_attributes_apply_presentation_state(
    MARU_WindowAttributes* attrs, MARU_WindowPresentationState state) {
  MARU_ASSUME(attrs != NULL);
  attrs->presentation_state = state;
}

static inline void
_maru_validate_attributes(const MARU_Context_Base *ctx_base,
                          uint64_t field_mask,
                          const MARU_WindowAttributes *attributes) {
  MARU_CONSTRAINT_CHECK(attributes != NULL);
  const uint64_t known_fields = MARU_WINDOW_ATTR_ALL;
  MARU_CONSTRAINT_CHECK((field_mask & ~known_fields) == 0);

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

  if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->dip_viewport_size));
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_MIN_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->dip_min_size));
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_MAX_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->dip_max_size));
  }

  if ((field_mask & (MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE)) ==
      (MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE)) {
    const bool max_unbounded_x = attributes->dip_max_size.x == 0;
    const bool max_unbounded_y = attributes->dip_max_size.y == 0;
    if (!max_unbounded_x) {
      MARU_CONSTRAINT_CHECK(attributes->dip_max_size.x >= attributes->dip_min_size.x);
    }
    if (!max_unbounded_y) {
      MARU_CONSTRAINT_CHECK(attributes->dip_max_size.y >= attributes->dip_min_size.y);
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_rect(attributes->dip_text_input_rect));
  }

  if (field_mask & MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_non_negative_vec2(attributes->dip_viewport_size));
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

  if (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT) {
    if (attributes->surrounding_text != NULL) {
      MARU_CONSTRAINT_CHECK(
          _maru_validate_utf8_string(attributes->surrounding_text));
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE) {
    MARU_CONSTRAINT_CHECK(
        _maru_validate_presentation_state(attributes->presentation_state));
  }
}

static inline void
_maru_validate_createContext(const MARU_ContextCreateInfo *create_info,
                             MARU_Context **out_context) {
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_context != NULL);
  MARU_CONSTRAINT_CHECK(create_info->backend >= MARU_BACKEND_UNKNOWN &&
                        create_info->backend <= MARU_BACKEND_COCOA);
  MARU_CONSTRAINT_CHECK(
      _maru_validate_allocator_complete(create_info->allocator));
  MARU_CONSTRAINT_CHECK(_maru_validate_optional_power_of_two_u32(
      create_info->tuning.user_event_queue_size));
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

  MARU_CONSTRAINT_CHECK(create_info->content_type >= MARU_CONTENT_TYPE_NONE &&
                        create_info->content_type <= MARU_CONTENT_TYPE_GAME);

  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  _maru_validate_thread(ctx_base);
  _maru_validate_attributes(ctx_base, MARU_WINDOW_ATTR_ALL,
                            &create_info->attributes);
  MARU_CONSTRAINT_CHECK(
      _maru_validate_presentation_state(
          create_info->attributes.presentation_state));
  MARU_CONSTRAINT_CHECK(
      _maru_validate_presentation_visibility(
          create_info->attributes.presentation_state,
          create_info->attributes.visible));
  MARU_CONSTRAINT_CHECK(_maru_validate_surrounding_text_tuple(
      create_info->attributes.surrounding_text,
      create_info->attributes.surrounding_cursor_byte,
      create_info->attributes.surrounding_anchor_byte));
}

static inline void _maru_validate_getWindow(MARU_Context *context,
                                            MARU_WindowId window_id,
                                            MARU_Window **out_window) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(out_window != NULL);
  (void)window_id;
  _maru_validate_thread((const MARU_Context_Base *)context);
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

  if (field_mask & (MARU_WINDOW_ATTR_VISIBLE | MARU_WINDOW_ATTR_PRESENTATION_STATE)) {
    const MARU_WindowAttributes *requested = &win_base->attrs_requested;
    const bool new_visible = (field_mask & MARU_WINDOW_ATTR_VISIBLE)
                                 ? attributes->visible
                                 : requested->visible;
    const MARU_WindowPresentationState new_state =
        (field_mask & MARU_WINDOW_ATTR_PRESENTATION_STATE)
            ? attributes->presentation_state
            : requested->presentation_state;

    MARU_CONSTRAINT_CHECK(_maru_validate_presentation_state(new_state));
    MARU_CONSTRAINT_CHECK(
        _maru_validate_presentation_visibility(new_state, new_visible));
  }

  if ((field_mask & (MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE)) != 0) {
    MARU_Vec2Dip new_min = (field_mask & MARU_WINDOW_ATTR_DIP_MIN_SIZE) ? attributes->dip_min_size : win_base->attrs_requested.dip_min_size;
    MARU_Vec2Dip new_max = (field_mask & MARU_WINDOW_ATTR_DIP_MAX_SIZE) ? attributes->dip_max_size : win_base->attrs_requested.dip_max_size;

    const bool max_unbounded_x = new_max.x == 0;
    const bool max_unbounded_y = new_max.y == 0;
    if (!max_unbounded_x) {
      MARU_CONSTRAINT_CHECK(new_max.x >= new_min.x);
    }
    if (!max_unbounded_y) {
      MARU_CONSTRAINT_CHECK(new_max.y >= new_min.y);
    }
  }

  if ((field_mask & (MARU_WINDOW_ATTR_SURROUNDING_TEXT |
                     MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE |
                     MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE)) != 0) {
    const char *new_text =
        (field_mask & MARU_WINDOW_ATTR_SURROUNDING_TEXT)
            ? attributes->surrounding_text
            : win_base->attrs_requested.surrounding_text;
    const uint32_t new_cursor =
        (field_mask & MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE)
            ? attributes->surrounding_cursor_byte
            : win_base->attrs_requested.surrounding_cursor_byte;
    const uint32_t new_anchor =
        (field_mask & MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE)
            ? attributes->surrounding_anchor_byte
            : win_base->attrs_requested.surrounding_anchor_byte;

    MARU_CONSTRAINT_CHECK(
        _maru_validate_surrounding_text_tuple(new_text, new_cursor, new_anchor));
  }
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

static inline void
_maru_validate_createQueue(const MARU_QueueCreateInfo *create_info,
                           MARU_Queue **out_queue) {
  MARU_CONSTRAINT_CHECK(create_info != NULL);
  MARU_CONSTRAINT_CHECK(out_queue != NULL);
  MARU_CONSTRAINT_CHECK(create_info->capacity > 0u);
  MARU_CONSTRAINT_CHECK(
      _maru_validate_allocator_complete(create_info->allocator));
}

static inline void _maru_validate_destroyQueue(MARU_Queue *queue) {
  MARU_CONSTRAINT_CHECK(queue != NULL);
  _maru_validate_queue_creator_thread(queue);
}

static inline void _maru_validate_pushQueue(MARU_Queue *queue,
                                            MARU_EventId type,
                                            MARU_WindowId window_id,
                                            const MARU_Event *event) {
  MARU_CONSTRAINT_CHECK(queue != NULL);
  _maru_validate_queue_creator_thread(queue);
  MARU_CONSTRAINT_CHECK(event != NULL);
  MARU_CONSTRAINT_CHECK(maru_isKnownEventId(type));
  MARU_CONSTRAINT_CHECK(maru_isQueueSafeEventId(type));
  (void)window_id;
}

static inline void _maru_validate_commitQueue(MARU_Queue *queue) {
  MARU_CONSTRAINT_CHECK(queue != NULL);
  _maru_validate_queue_creator_thread(queue);
}

static inline void _maru_validate_scanQueue(const MARU_Queue *queue,
                                            MARU_EventMask mask,
                                            MARU_QueueEventCallback callback,
                                            void *userdata) {
  MARU_CONSTRAINT_CHECK(queue != NULL);
  if (mask != 0) {
    MARU_CONSTRAINT_CHECK(callback != NULL);
  }
  (void)mask;
  (void)userdata;
}

static inline void _maru_validate_setQueueCoalesceMask(MARU_Queue *queue, MARU_EventMask mask) {
  MARU_CONSTRAINT_CHECK(queue != NULL);
  _maru_validate_queue_creator_thread(queue);
  (void)mask;
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
_maru_validate_getControllers(const MARU_Context *context,
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
    const MARU_ChannelInfo *channels =
        maru_getControllerHapticChannelInfo(controller);
    MARU_CONSTRAINT_CHECK(channels != NULL);
    for (uint32_t i = 0; i < count; ++i) {
      const MARU_Scalar value = intensities[i];
      const MARU_ChannelInfo *channel = &channels[first_haptic + i];
      MARU_CONSTRAINT_CHECK(value >= channel->min_value);
      MARU_CONSTRAINT_CHECK(value <= channel->max_value);
    }
  }
}

static inline void
_maru_validate_announceData(MARU_Window *window, MARU_DataExchangeTarget target,
                            MARU_StringList mime_types,
                            MARU_DropActionMask allowed_actions) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  const MARU_DropActionMask known_actions = (MARU_DropActionMask)(
      MARU_DROP_ACTION_COPY | MARU_DROP_ACTION_MOVE | MARU_DROP_ACTION_LINK);
  MARU_CONSTRAINT_CHECK((allowed_actions & ~known_actions) == 0);
  if (mime_types.count > 0) {
    MARU_CONSTRAINT_CHECK(mime_types.strings != NULL);
    for (uint32_t i = 0; i < mime_types.count; ++i) {
      MARU_CONSTRAINT_CHECK(mime_types.strings[i] != NULL);
      MARU_CONSTRAINT_CHECK(mime_types.strings[i][0] != '\0');
    }
  }
}

static inline void
_maru_validate_announceClipboardData(MARU_Context *context,
                                     MARU_StringList mime_types) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
  if (mime_types.count > 0) {
    MARU_CONSTRAINT_CHECK(mime_types.strings != NULL);
    for (uint32_t i = 0; i < mime_types.count; ++i) {
      MARU_CONSTRAINT_CHECK(mime_types.strings[i] != NULL);
      MARU_CONSTRAINT_CHECK(mime_types.strings[i][0] != '\0');
    }
  }
}

static inline void
_maru_validate_announceDragData(MARU_Window *window, MARU_StringList mime_types,
                                MARU_DropActionMask allowed_actions) {
  _maru_validate_announceData(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                              mime_types, allowed_actions);
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

static inline void
_maru_validate_provideClipboardData(MARU_DataRequest *request,
                                    const void *data, size_t size,
                                    MARU_DataProvideFlags flags) {
  _maru_validate_provideData(request, data, size, flags);
  MARU_CONSTRAINT_CHECK(
      ((const MARU_DataRequestHandleBase *)request)->target ==
      MARU_DATA_EXCHANGE_TARGET_CLIPBOARD);
}

static inline void
_maru_validate_provideDropData(MARU_DataRequest *request, const void *data,
                               size_t size, MARU_DataProvideFlags flags) {
  _maru_validate_provideData(request, data, size, flags);
  MARU_CONSTRAINT_CHECK(
      ((const MARU_DataRequestHandleBase *)request)->target ==
      MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
}

static inline void _maru_validate_requestData(MARU_Window *window,
                                              MARU_DataExchangeTarget target,
                                              const char *mime_type,
                                              void *userdata) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  MARU_CONSTRAINT_CHECK(mime_type != NULL);
  MARU_CONSTRAINT_CHECK(mime_type[0] != '\0');
  (void)userdata;
}

static inline void
_maru_validate_requestClipboardData(MARU_Context *context, const char *mime_type,
                                    void *userdata) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
  MARU_CONSTRAINT_CHECK(mime_type != NULL);
  MARU_CONSTRAINT_CHECK(mime_type[0] != '\0');
  (void)userdata;
}

static inline void _maru_validate_requestDropData(MARU_Window *window,
                                                  const char *mime_type,
                                                  void *userdata) {
  _maru_validate_requestData(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                             mime_type, userdata);
}

static inline void
_maru_validate_getAvailableMIMETypes(const MARU_Window *window,
                                     MARU_DataExchangeTarget target,
                                     MARU_StringList *out_list) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
  MARU_CONSTRAINT_CHECK(target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void
_maru_validate_getAvailableClipboardMIMETypes(const MARU_Context *context,
                                              MARU_StringList *out_list) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void
_maru_validate_getAvailableDropMIMETypes(const MARU_Window *window,
                                         MARU_StringList *out_list) {
  _maru_validate_getAvailableMIMETypes(
      window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, out_list);
}

static inline void _maru_validate_wakeContext(MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void _maru_validate_postEvent(MARU_Context *context,
                                            MARU_EventId type,
                                            MARU_UserDefinedEvent evt) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_isUserEventId(type));
  (void)type;
  (void)evt;
}

static inline void _maru_validate_getMonitors(const MARU_Context *context,
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
  _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
}

static inline void _maru_validate_setMonitorMode(MARU_Monitor *monitor,
                                                 MARU_VideoMode mode) {
  MARU_CONSTRAINT_CHECK(monitor != NULL);
  _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
  MARU_CONSTRAINT_CHECK(mode.px_size.x > 0 && mode.px_size.y > 0);
}

static inline void
_maru_validate_getWaylandContextHandle(const MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WAYLAND);
}

static inline void
_maru_validate_getWaylandWindowHandle(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  const MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WAYLAND);
}

static inline void
_maru_validate_getX11ContextHandle(const MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_X11);
}

static inline void
_maru_validate_getX11WindowHandle(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  const MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_X11);
}

static inline void
_maru_validate_getWin32ContextHandle(const MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WINDOWS);
}

static inline void
_maru_validate_getWin32WindowHandle(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  const MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_WINDOWS);
}

static inline void
_maru_validate_getCocoaContextHandle(const MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_COCOA);
}

static inline void
_maru_validate_getCocoaWindowHandle(const MARU_Window *window) {
  MARU_CONSTRAINT_CHECK(window != NULL);
  _maru_validate_window_ready(window);
  const MARU_Context *context = maru_getWindowContext(window);
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(maru_getContextBackend(context) == MARU_BACKEND_COCOA);
}

static inline void
_maru_validate_getKeyboardKeyStates(const MARU_Context *context) {
  MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void
_maru_validate_isKeyboardKeyPressed(const MARU_Context *context, MARU_Key key) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(key < MARU_KEY_COUNT);
}

static inline void _maru_validate_getVkExtensions(const MARU_Context *context,
                                                  MARU_StringList *out_list) {
  MARU_CONSTRAINT_CHECK(context != NULL);
  MARU_CONSTRAINT_CHECK(out_list != NULL);
  _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void
_maru_validate_createVkSurface(const MARU_Window *window, VkInstance instance,
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
}

static inline void _maru_validate_live_requestWindowFocus(MARU_Window *window) {
  _maru_validate_window_ready(window);
}

static inline void _maru_validate_live_requestWindowFrame(MARU_Window *window) {
  _maru_validate_window_ready(window);
}

static inline void
_maru_validate_live_requestWindowAttention(MARU_Window *window) {
  _maru_validate_window_ready(window);
}

static inline void _maru_validate_live_setControllerHapticLevels(
    MARU_Controller *controller, uint32_t first_haptic, uint32_t count,
    const MARU_Scalar *intensities) {
  (void)controller;
  (void)first_haptic;
  (void)count;
  (void)intensities;
}

static inline void _maru_validate_live_announceData(
    MARU_Window *window, MARU_DataExchangeTarget target, MARU_StringList mime_types,
    MARU_DropActionMask allowed_actions) {
  (void)target;
  (void)mime_types;
  (void)allowed_actions;
  _maru_validate_window_ready(window);
}

static inline void
_maru_validate_live_announceClipboardData(MARU_Context *context,
                                          MARU_StringList mime_types) {
  (void)context;
  (void)mime_types;
}

static inline void
_maru_validate_live_announceDragData(MARU_Window *window,
                                     MARU_StringList mime_types,
                                     MARU_DropActionMask allowed_actions) {
  _maru_validate_live_announceData(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                                   mime_types, allowed_actions);
}

static inline void _maru_validate_live_requestData(
    MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type,
    void *userdata) {
  (void)target;
  (void)mime_type;
  (void)userdata;
  _maru_validate_window_ready(window);
}

static inline void
_maru_validate_live_requestClipboardData(MARU_Context *context,
                                         const char *mime_type,
                                         void *userdata) {
  (void)context;
  (void)mime_type;
  (void)userdata;
}

static inline void _maru_validate_live_requestDropData(MARU_Window *window,
                                                       const char *mime_type,
                                                       void *userdata) {
  _maru_validate_live_requestData(window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                                  mime_type, userdata);
}

static inline void _maru_validate_live_getAvailableMIMETypes(
    const MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_StringList *out_list) {
  (void)target;
  (void)out_list;
  _maru_validate_window_ready(window);
}

static inline void
_maru_validate_live_getAvailableClipboardMIMETypes(const MARU_Context *context,
                                                   MARU_StringList *out_list) {
  (void)context;
  (void)out_list;
}

static inline void
_maru_validate_live_getAvailableDropMIMETypes(const MARU_Window *window,
                                              MARU_StringList *out_list) {
  _maru_validate_live_getAvailableMIMETypes(
      window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, out_list);
}

static inline void _maru_validate_live_getMonitorModes(
    const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  (void)monitor;
  (void)out_list;
}

static inline void
_maru_validate_live_setMonitorMode(MARU_Monitor *monitor,
                                   MARU_VideoMode mode) {
  (void)monitor;
  (void)mode;
}
static inline void
_maru_validate_live_createVkSurface(const MARU_Window *window, VkInstance instance,
                                    MARU_VkGetInstanceProcAddrFunc vk_loader,
                                    VkSurfaceKHR *out_surface) {
  (void)instance;
  (void)vk_loader;
  (void)out_surface;
  _maru_validate_window_ready(window);
}

#define MARU_API_VALIDATE_LIVE(fn, ...) _maru_validate_live_##fn(__VA_ARGS__)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#define MARU_API_VALIDATE_LIVE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
