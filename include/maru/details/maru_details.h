// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU__INCLUDING_DETAILS_FROM_MARU_H
#error "Do not include maru/details/maru_details.h directly; include maru/maru.h."
#endif

#ifndef MARU_DETAILS_H_INCLUDED
#define MARU_DETAILS_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MARU_CONTEXT_STATE_LOST MARU_BIT(0)
#define MARU_CONTEXT_STATE_READY MARU_BIT(1)

#define MARU_MONITOR_STATE_LOST MARU_BIT(0)

#define MARU_WINDOW_STATE_READY MARU_BIT(1)
#define MARU_WINDOW_STATE_FOCUSED MARU_BIT(2)
#define MARU_WINDOW_STATE_MAXIMIZED MARU_BIT(3)
#define MARU_WINDOW_STATE_FULLSCREEN MARU_BIT(4)
#define MARU_WINDOW_STATE_RESIZABLE MARU_BIT(5)
#define MARU_WINDOW_STATE_DECORATED MARU_BIT(6)
#define MARU_WINDOW_STATE_VISIBLE MARU_BIT(7)
#define MARU_WINDOW_STATE_MINIMIZED MARU_BIT(8)

#define MARU_CONTROLLER_STATE_LOST MARU_BIT(0)

/*
 * This file is not part of Maru's source-level API.
 * Application code must not include it directly, cast opaque handles to these
 * types, or depend on their field names.
 *
 * However, maru.h exposes public inline accessors that rely on the leading
 * layout of some handle storage declared here. Those leading layouts are
 * therefore part of Maru's ABI contract by design.
 */
typedef struct MARU_ContextPrefix {
  void* userdata;
  uint64_t flags;
  MARU_BackendType backend_type;
  const MARU_ButtonState8* mouse_button_state;
  const MARU_ChannelInfo* mouse_button_channels;
  uint32_t mouse_button_count;
  const MARU_ButtonState8* keyboard_state;
  uint32_t keyboard_key_count;
  int32_t mouse_default_button_channels[MARU_MOUSE_DEFAULT_COUNT];
} MARU_ContextPrefix;

typedef struct MARU_ImagePrefix {
  void* userdata;
} MARU_ImagePrefix;

typedef struct MARU_CursorPrefix {
  void* userdata;
  uint64_t flags;
} MARU_CursorPrefix;

typedef struct MARU_MonitorPrefix {
  void* userdata;
  MARU_Context* context;
  uint64_t flags;
  const char* name;
  MARU_Vec2Mm physical_size;
  MARU_VideoMode current_mode;
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_size;
  bool is_primary;
  MARU_Scalar scale;
} MARU_MonitorPrefix;

typedef struct MARU_WindowPrefix {
  void* userdata;
  MARU_Context* context;
  MARU_WindowId window_id;
  uint64_t flags;
  MARU_CursorMode cursor_mode;
  const MARU_Cursor* current_cursor;
  const char* title;
  const MARU_Image* icon;
  MARU_WindowGeometry geometry;
} MARU_WindowPrefix;

typedef struct MARU_ControllerPrefix {
  void* userdata;
  MARU_Context* context;
  uint64_t flags;
  const char* name;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  uint8_t guid[16];
  const MARU_ChannelInfo* analog_channels;
  const MARU_AnalogInputState* analogs;
  uint32_t analog_count;
  const MARU_ChannelInfo* button_channels;
  const MARU_ButtonState8* buttons;
  uint32_t button_count;
  const MARU_ChannelInfo* haptic_channels;
  uint32_t haptic_count;
} MARU_ControllerPrefix;

typedef struct MARU_DropSessionPrefix {
  MARU_DropAction* action;
  void** session_userdata;
} MARU_DropSessionPrefix;

static inline MARU_BackendType maru_getContextBackend(const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->backend_type;
}

static inline void* maru_getContextUserdata(const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->userdata;
}

static inline void maru_setContextUserdata(MARU_Context* context, void* userdata) {
  ((MARU_ContextPrefix*)context)->userdata = userdata;
}

static inline bool maru_isContextLost(const MARU_Context* context) {
  return (((const MARU_ContextPrefix*)context)->flags & MARU_CONTEXT_STATE_LOST) != 0;
}

static inline bool maru_isContextReady(const MARU_Context* context) {
  return (((const MARU_ContextPrefix*)context)->flags & MARU_CONTEXT_STATE_READY) != 0;
}

static inline uint32_t maru_getContextMouseButtonCount(const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->mouse_button_count;
}

static inline const MARU_ButtonState8* maru_getContextMouseButtonStates(
    const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->mouse_button_state;
}

static inline const MARU_ChannelInfo* maru_getContextMouseButtonChannelInfo(
    const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->mouse_button_channels;
}

static inline uint32_t maru_getKeyboardKeyCount(const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->keyboard_key_count;
}

static inline const MARU_ButtonState8* maru_getKeyboardKeyStates(const MARU_Context* context) {
  return ((const MARU_ContextPrefix*)context)->keyboard_state;
}

static inline int32_t maru_getContextMouseDefaultButtonChannel(const MARU_Context* context,
                                                               MARU_MouseDefaultButton which) {
  if ((uint32_t)which >= MARU_MOUSE_DEFAULT_COUNT) {
    return -1;
  }
  return ((const MARU_ContextPrefix*)context)->mouse_default_button_channels[which];
}

static inline bool maru_isContextMouseButtonPressed(const MARU_Context* context,
                                                    uint32_t button_id) {
  uint32_t count = maru_getContextMouseButtonCount(context);
  if (button_id >= count) {
    return false;
  }
  const MARU_ButtonState8* states = maru_getContextMouseButtonStates(context);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline void* maru_getImageUserdata(const MARU_Image* image) {
  return ((const MARU_ImagePrefix*)image)->userdata;
}

static inline void maru_setImageUserdata(MARU_Image* image, void* userdata) {
  ((MARU_ImagePrefix*)image)->userdata = userdata;
}

static inline void* maru_getCursorUserdata(const MARU_Cursor* cursor) {
  return ((const MARU_CursorPrefix*)cursor)->userdata;
}

static inline void maru_setCursorUserdata(MARU_Cursor* cursor, void* userdata) {
  ((MARU_CursorPrefix*)cursor)->userdata = userdata;
}

static inline bool maru_isCursorSystem(const MARU_Cursor* cursor) {
  return (((const MARU_CursorPrefix*)cursor)->flags & MARU_CURSOR_FLAG_SYSTEM) != 0;
}

static inline void* maru_getMonitorUserdata(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->userdata;
}

static inline void maru_setMonitorUserdata(MARU_Monitor* monitor, void* userdata) {
  ((MARU_MonitorPrefix*)monitor)->userdata = userdata;
}

static inline MARU_Context* maru_getMonitorContext(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->context;
}

static inline bool maru_isMonitorLost(const MARU_Monitor* monitor) {
  return (((const MARU_MonitorPrefix*)monitor)->flags & MARU_MONITOR_STATE_LOST) != 0;
}

static inline const char* maru_getMonitorName(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->name;
}

static inline MARU_Vec2Mm maru_getMonitorPhysicalSize(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->physical_size;
}

static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->current_mode;
}

static inline MARU_Vec2Dip maru_getMonitorDipPosition(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->dip_position;
}

static inline MARU_Vec2Dip maru_getMonitorDipSize(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->dip_size;
}

static inline bool maru_isMonitorPrimary(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->is_primary;
}

static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor* monitor) {
  return ((const MARU_MonitorPrefix*)monitor)->scale;
}

static inline void* maru_getWindowUserdata(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->userdata;
}

static inline void maru_setWindowUserdata(MARU_Window* window, void* userdata) {
  ((MARU_WindowPrefix*)window)->userdata = userdata;
}

static inline MARU_Context* maru_getWindowContext(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->context;
}

static inline MARU_WindowId maru_getWindowId(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->window_id;
}

static inline const char* maru_getWindowTitle(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->title;
}

static inline bool maru_isWindowReady(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_READY) != 0;
}

static inline bool maru_isWindowFocused(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_FOCUSED) != 0;
}

static inline bool maru_isWindowMaximized(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
}

static inline bool maru_isWindowFullscreen(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_FULLSCREEN) != 0;
}

static inline bool maru_isWindowVisible(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_VISIBLE) != 0;
}

static inline bool maru_isWindowMinimized(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
}

static inline bool maru_isWindowResizable(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
}

static inline bool maru_isWindowDecorated(const MARU_Window* window) {
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_DECORATED) != 0;
}

static inline MARU_CursorMode maru_getWindowCursorMode(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->cursor_mode;
}

static inline const MARU_Image* maru_getWindowIcon(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->icon;
}

static inline MARU_WindowGeometry maru_getWindowGeometry(const MARU_Window* window) {
  return ((const MARU_WindowPrefix*)window)->geometry;
}

static inline void* maru_getControllerUserdata(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->userdata;
}

static inline void maru_setControllerUserdata(MARU_Controller* controller, void* userdata) {
  ((MARU_ControllerPrefix*)controller)->userdata = userdata;
}

static inline MARU_Context* maru_getControllerContext(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->context;
}

static inline bool maru_isControllerLost(const MARU_Controller* controller) {
  return (((const MARU_ControllerPrefix*)controller)->flags & MARU_CONTROLLER_STATE_LOST) != 0;
}

static inline const char* maru_getControllerName(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->name;
}

static inline uint16_t maru_getControllerVendorId(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->vendor_id;
}

static inline uint16_t maru_getControllerProductId(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->product_id;
}

static inline uint16_t maru_getControllerVersion(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->version;
}

static inline const uint8_t* maru_getControllerGUID(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->guid;
}

static inline uint32_t maru_getControllerAnalogCount(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->analog_count;
}

static inline const MARU_ChannelInfo* maru_getControllerAnalogChannelInfo(
    const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->analog_channels;
}

static inline const MARU_AnalogInputState* maru_getControllerAnalogStates(
    const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->analogs;
}

static inline uint32_t maru_getControllerButtonCount(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->button_count;
}

static inline const MARU_ChannelInfo* maru_getControllerButtonChannelInfo(
    const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->button_channels;
}

static inline const MARU_ButtonState8* maru_getControllerButtonStates(
    const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->buttons;
}

static inline bool maru_isControllerButtonPressed(const MARU_Controller* controller,
                                                  uint32_t button_id) {
  if (button_id >= maru_getControllerButtonCount(controller)) {
    return false;
  }
  const MARU_ButtonState8* states = maru_getControllerButtonStates(controller);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline uint32_t maru_getControllerHapticCount(const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->haptic_count;
}

static inline const MARU_ChannelInfo* maru_getControllerHapticChannelInfo(
    const MARU_Controller* controller) {
  return ((const MARU_ControllerPrefix*)controller)->haptic_channels;
}

static inline void maru_setDropSessionAction(MARU_DropSession* session, MARU_DropAction action) {
  *(((MARU_DropSessionPrefix*)session)->action) = action;
}

static inline MARU_DropAction maru_getDropSessionAction(const MARU_DropSession* session) {
  return *(((const MARU_DropSessionPrefix*)session)->action);
}

static inline void maru_setDropSessionUserdata(MARU_DropSession* session, void* userdata) {
  *(((MARU_DropSessionPrefix*)session)->session_userdata) = userdata;
}

static inline void* maru_getDropSessionUserdata(const MARU_DropSession* session) {
  return *(((const MARU_DropSessionPrefix*)session)->session_userdata);
}

static inline bool maru_isKeyboardKeyPressed(const MARU_Context* context, MARU_Key key) {
  if ((uint32_t)key >= MARU_KEY_COUNT) {
    return false;
  }
  const MARU_ButtonState8* states = maru_getKeyboardKeyStates(context);
  return states ? (states[key] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context* context, bool enabled) {
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.inhibit_idle = enabled;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_INHIBIT_IDLE, &attrs);
}


static inline MARU_Status maru_setContextIdleTimeout(MARU_Context* context, uint32_t timeout_ms) {
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.idle_timeout_ms = timeout_ms;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_IDLE_TIMEOUT, &attrs);
}

static inline MARU_Status maru_setContextDiagnosticCallback(MARU_Context* context, MARU_DiagnosticCallback cb, void* userdata) {
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.diagnostic_cb = cb;
  attrs.diagnostic_userdata = userdata;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_DIAGNOSTICS, &attrs);
}

static inline MARU_Status maru_setWindowViewportDipSize(MARU_Window* window, MARU_Vec2Dip size) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.viewport_dip_size = size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_VIEWPORT_DIP_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowPrimarySelection(MARU_Window* window, bool enabled) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.primary_selection = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_PRIMARY_SELECTION, &attrs);
}

static inline MARU_Status maru_setWindowSurroundingText(MARU_Window* window, const char* text, uint32_t cursor_byte, uint32_t anchor_byte) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.surrounding_text = text;
  attrs.surrounding_cursor_byte = cursor_byte;
  attrs.surrounding_anchor_byte = anchor_byte;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_SURROUNDING_TEXT | MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE | MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE, &attrs);
}

static inline MARU_Status maru_setWindowTitle(MARU_Window* window, const char* title) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.title = title;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE, &attrs);
}

static inline MARU_Status maru_setWindowDipSize(MARU_Window* window, MARU_Vec2Dip size) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_size = size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowDipPosition(MARU_Window* window, MARU_Vec2Dip position) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_position = position;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_POSITION, &attrs);
}

static inline MARU_Status maru_setWindowFullscreen(MARU_Window* window, bool enabled) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.fullscreen = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
}

static inline MARU_Status maru_setWindowMaximized(MARU_Window* window, bool enabled) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.maximized = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
}

static inline MARU_Status maru_setWindowCursorMode(MARU_Window* window, MARU_CursorMode mode) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.cursor_mode = mode;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
}

static inline MARU_Status maru_setWindowCursor(MARU_Window* window, MARU_Cursor* cursor) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.cursor = cursor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
}

static inline MARU_Status maru_setWindowMonitor(MARU_Window* window, MARU_Monitor* monitor) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.monitor = monitor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MONITOR, &attrs);
}

static inline MARU_Status maru_setWindowMinDipSize(MARU_Window* window, MARU_Vec2Dip min_dip_size) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.min_dip_size = min_dip_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MIN_DIP_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowMaxDipSize(MARU_Window* window, MARU_Vec2Dip max_dip_size) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.max_dip_size = max_dip_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MAX_DIP_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowAspectRatio(MARU_Window* window,
                                                    MARU_Fraction aspect_ratio) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.aspect_ratio = aspect_ratio;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
}

static inline MARU_Status maru_setWindowResizable(MARU_Window* window, bool enabled) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.resizable = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
}

static inline MARU_Status maru_setWindowTextInputType(MARU_Window* window,
                                                      MARU_TextInputType type) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.text_input_type = type;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE, &attrs);
}

static inline MARU_Status maru_setWindowTextInputRect(MARU_Window* window, MARU_RectDip rect) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.text_input_rect = rect;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_RECT, &attrs);
}

static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window* window, bool enabled) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.accept_drop = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ACCEPT_DROP, &attrs);
}

static inline MARU_Status maru_setWindowVisible(MARU_Window* window, bool visible) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.visible = visible;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_VISIBLE, &attrs);
}

static inline MARU_Status maru_setWindowMinimized(MARU_Window* window, bool minimized) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.minimized = minimized;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MINIMIZED, &attrs);
}

static inline MARU_Status maru_setWindowIcon(MARU_Window* window, MARU_Image* icon) {
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.icon = icon;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ICON, &attrs);
}

static inline bool maru_applyTextEditCommitUtf8(char* buffer,
                                                uint32_t capacity_bytes,
                                                uint32_t* inout_length,
                                                uint32_t* inout_cursor_byte,
                                                const MARU_TextEditCommittedEvent* commit) {
  if (!buffer || !inout_length || !inout_cursor_byte || !commit) {
    return false;
  }

  const uint32_t length = *inout_length;
  const uint32_t cursor = *inout_cursor_byte;
  if (cursor > length) {
    return false;
  }
  if (commit->delete_before_bytes > cursor) {
    return false;
  }

  const uint32_t delete_start = cursor - commit->delete_before_bytes;
  const uint32_t delete_end = cursor + commit->delete_after_bytes;
  if (delete_end > length || delete_end < delete_start) {
    return false;
  }
  if (commit->committed_length_bytes > 0 && !commit->committed_utf8) {
    return false;
  }

  const uint32_t delete_len = delete_end - delete_start;
  const uint32_t tail_len = length - delete_end;
  const uint32_t new_length = length - delete_len + commit->committed_length_bytes;
  if (new_length >= capacity_bytes) {
    return false;
  }

  if (commit->committed_length_bytes != delete_len) {
    memmove(buffer + delete_start + commit->committed_length_bytes, buffer + delete_end, tail_len);
  }
  if (commit->committed_length_bytes > 0) {
    memcpy(buffer + delete_start, commit->committed_utf8, commit->committed_length_bytes);
  }

  *inout_length = new_length;
  *inout_cursor_byte = delete_start + commit->committed_length_bytes;
  if (new_length < capacity_bytes) {
    buffer[new_length] = '\0';
  }
  return true;
}

static inline const char* maru_getDiagnosticString(MARU_Diagnostic diagnostic) {
  switch (diagnostic) {
    case MARU_DIAGNOSTIC_NONE:
      return "NONE";
    case MARU_DIAGNOSTIC_INFO:
      return "INFO";
    case MARU_DIAGNOSTIC_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE:
      return "RESOURCE_UNAVAILABLE";
    case MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE:
      return "DYNAMIC_LIB_FAILURE";
    case MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED:
      return "FEATURE_UNSUPPORTED";
    case MARU_DIAGNOSTIC_BACKEND_FAILURE:
      return "BACKEND_FAILURE";
    case MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE:
      return "BACKEND_UNAVAILABLE";
    case MARU_DIAGNOSTIC_VULKAN_FAILURE:
      return "VULKAN_FAILURE";
    case MARU_DIAGNOSTIC_UNKNOWN:
      return "UNKNOWN";
    case MARU_DIAGNOSTIC_INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case MARU_DIAGNOSTIC_PRECONDITION_FAILURE:
      return "PRECONDITION_FAILURE";
    case MARU_DIAGNOSTIC_INTERNAL:
      return "INTERNAL";
    default:
      return "UNKNOWN";
  }
}

#ifdef __cplusplus
}
#endif

#endif
