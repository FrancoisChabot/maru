// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WINDOW_H_INCLUDED
#define MARU_WINDOW_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/contexts.h"
#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/metrics.h"

/**
 * @file windows.h
 * @brief Window management, lifecycle, attributes, and core state.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server
 * connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;
/** @brief Opaque handle representing a hardware-accelerated cursor. */
typedef struct MARU_Cursor MARU_Cursor;
/** @brief Opaque handle representing an immutable image. */
typedef struct MARU_Image MARU_Image;
/** @brief Persistent handle representing a physical monitor. */
typedef struct MARU_Monitor MARU_Monitor;

/** @brief Runtime state flags for a window. */
#define MARU_WINDOW_STATE_LOST ((MARU_Flags)1 << 0)
#define MARU_WINDOW_STATE_READY ((MARU_Flags)1 << 1)
#define MARU_WINDOW_STATE_FOCUSED ((MARU_Flags)1 << 2)
#define MARU_WINDOW_STATE_MAXIMIZED ((MARU_Flags)1 << 3)
#define MARU_WINDOW_STATE_FULLSCREEN ((MARU_Flags)1 << 4)
#define MARU_WINDOW_STATE_MOUSE_PASSTHROUGH ((MARU_Flags)1 << 5)
#define MARU_WINDOW_STATE_RESIZABLE ((MARU_Flags)1 << 6)
#define MARU_WINDOW_STATE_DECORATED ((MARU_Flags)1 << 7)
#define MARU_WINDOW_STATE_VISIBLE ((MARU_Flags)1 << 8)
#define MARU_WINDOW_STATE_MINIMIZED ((MARU_Flags)1 << 9)

/** @brief Cursor visibility and constraint modes. */
typedef enum MARU_CursorMode {
  MARU_CURSOR_NORMAL = 0,
  MARU_CURSOR_HIDDEN = 1,
  MARU_CURSOR_LOCKED = 2,
} MARU_CursorMode;

/* ----- Passive Accessors (External Synchronization Required) -----
 *
 * These functions are essentially zero-cost member accesses. They are safe to
 * call from any thread, provided the access is synchronized with mutating
 * operations (like maru_pumpEvents or maru_updateWindow) on the same window
 * to ensure memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a window. */
static inline void *maru_getWindowUserdata(const MARU_Window *window);

/** @brief Sets the user-defined data pointer associated with a window. */
static inline void maru_setWindowUserdata(MARU_Window *window, void *userdata);

/** @brief Retrieves the context that owns the specified window. */
static inline MARU_Context *maru_getWindowContext(const MARU_Window *window);

/** @brief Retrieves the current title of a window. */
static inline const char *maru_getWindowTitle(const MARU_Window *window);

/** @brief Checks if the window has been lost due to an unrecoverable error. */
static inline bool maru_isWindowLost(const MARU_Window *window);

/** @brief Checks if the window is fully initialized and ready for use. */
static inline bool maru_isWindowReady(const MARU_Window *window);

/** @brief Checks if the window currently has input focus. */
static inline bool maru_isWindowFocused(const MARU_Window *window);

/** @brief Checks if the window is currently maximized. */
static inline bool maru_isWindowMaximized(const MARU_Window *window);

/** @brief Checks if the window is currently in fullscreen mode. */
static inline bool maru_isWindowFullscreen(const MARU_Window *window);

/** @brief Checks if the window currently has mouse passthrough enabled. */
static inline bool maru_isWindowMousePassthrough(const MARU_Window *window);
/** @brief Checks if the window is currently visible. */
static inline bool maru_isWindowVisible(const MARU_Window *window);
/** @brief Checks if the window is currently minimized. */
static inline bool maru_isWindowMinimized(const MARU_Window *window);

/** @brief Retrieves the current event mask for a window. */
static inline MARU_EventMask maru_getWindowEventMask(const MARU_Window *window);

/** @brief Retrieves the runtime performance metrics for a window. */
static inline const MARU_WindowMetrics *
maru_getWindowMetrics(const MARU_Window *window);

/** @brief Semantic hints for compositor optimization. */
typedef enum MARU_ContentType {
  MARU_CONTENT_TYPE_NONE = 0,
  MARU_CONTENT_TYPE_PHOTO = 1,
  MARU_CONTENT_TYPE_VIDEO = 2,
  MARU_CONTENT_TYPE_GAME = 3,
} MARU_ContentType;

/** @brief Semantic hints for the Input Method Editor. */
typedef enum MARU_TextInputType {
  MARU_TEXT_INPUT_TYPE_NONE = 0,
  MARU_TEXT_INPUT_TYPE_TEXT,
  MARU_TEXT_INPUT_TYPE_PASSWORD,
  MARU_TEXT_INPUT_TYPE_EMAIL,
  MARU_TEXT_INPUT_TYPE_NUMERIC,
} MARU_TextInputType;

/** @brief Bitmask for selecting which attributes to update in
 * maru_updateWindow(). */
typedef uint32_t MARU_WindowAttributesField;
#define MARU_WINDOW_ATTR_TITLE (1u << 0)
#define MARU_WINDOW_ATTR_LOGICAL_SIZE (1u << 1)
#define MARU_WINDOW_ATTR_FULLSCREEN (1u << 2)
#define MARU_WINDOW_ATTR_CURSOR_MODE (1u << 3)
#define MARU_WINDOW_ATTR_CURSOR (1u << 4)
#define MARU_WINDOW_ATTR_MONITOR (1u << 5)
#define MARU_WINDOW_ATTR_MAXIMIZED (1u << 6)
#define MARU_WINDOW_ATTR_MIN_SIZE (1u << 7)
#define MARU_WINDOW_ATTR_MAX_SIZE (1u << 8)
#define MARU_WINDOW_ATTR_POSITION (1u << 9)
#define MARU_WINDOW_ATTR_ASPECT_RATIO (1u << 10)
#define MARU_WINDOW_ATTR_RESIZABLE (1u << 11)
#define MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH (1u << 12)
#define MARU_WINDOW_ATTR_ACCEPT_DROP (1u << 13)
#define MARU_WINDOW_ATTR_TEXT_INPUT_TYPE (1u << 14)
#define MARU_WINDOW_ATTR_TEXT_INPUT_RECT (1u << 15)
#define MARU_WINDOW_ATTR_PRIMARY_SELECTION (1u << 16)
#define MARU_WINDOW_ATTR_EVENT_MASK (1u << 17)
#define MARU_WINDOW_ATTR_VIEWPORT_SIZE (1u << 18)
#define MARU_WINDOW_ATTR_SURROUNDING_TEXT (1u << 19)
#define MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET (1u << 20)
#define MARU_WINDOW_ATTR_VISIBLE (1u << 21)
#define MARU_WINDOW_ATTR_MINIMIZED (1u << 22)
#define MARU_WINDOW_ATTR_ICON (1u << 23)

#define MARU_WINDOW_ATTR_ALL 0xFFFFFFFFu

/** @brief Live-updatable window properties. */
typedef struct MARU_WindowAttributes {
  const char *title;
  MARU_Vec2Dip logical_size;
  bool fullscreen;
  bool maximized;
  MARU_CursorMode cursor_mode;
  MARU_Cursor *cursor;
  MARU_Monitor *monitor;
  MARU_Vec2Dip min_size;
  MARU_Vec2Dip max_size;
  MARU_Vec2Dip position;
  MARU_Fraction aspect_ratio;
  bool resizable;
  bool mouse_passthrough;
  bool accept_drop;
  MARU_TextInputType text_input_type;
  MARU_RectDip text_input_rect;
  bool primary_selection;
  MARU_EventMask event_mask;
  MARU_Vec2Dip viewport_size;
  const char *surrounding_text;
  uint32_t surrounding_cursor_offset;
  uint32_t surrounding_anchor_offset;
  bool visible;
  bool minimized;
  MARU_Image *icon;
} MARU_WindowAttributes;

/** @brief Parameters for maru_createWindow(). */
typedef struct MARU_WindowCreateInfo {
  MARU_WindowAttributes attributes;
  const char *app_id;
  MARU_ContentType content_type;
  bool decorated;
  bool transparent;
  void *userdata;
} MARU_WindowCreateInfo;

/** @brief Default initialization macro for MARU_WindowCreateInfo. */
#define MARU_WINDOW_CREATE_INFO_DEFAULT                                        \
  {.attributes = {.title = "MARU Window",                                      \
                  .logical_size = {800, 600},                                  \
                  .fullscreen = false,                                         \
                  .maximized = false,                                          \
                  .cursor_mode = MARU_CURSOR_NORMAL,                           \
                  .cursor = NULL,                                              \
                  .monitor = NULL,                                             \
                  .min_size = {0, 0},                                          \
                  .max_size = {0, 0},                                          \
                  .position = {0, 0},                                          \
                  .aspect_ratio = {0, 0},                                      \
                  .resizable = true,                                           \
                  .mouse_passthrough = false,                                  \
                  .accept_drop = false,                                        \
                  .text_input_type = MARU_TEXT_INPUT_TYPE_NONE,                \
                  .text_input_rect = {{0, 0}, {0, 0}},                         \
                  .primary_selection = true,                                   \
                  .event_mask = MARU_ALL_EVENTS,                               \
                  .viewport_size = {0, 0},                                     \
                  .surrounding_text = NULL,                                    \
                  .surrounding_cursor_offset = 0,                              \
                  .surrounding_anchor_offset = 0,                              \
                  .visible = true,                                             \
                  .minimized = false,                                          \
                  .icon = NULL},                                               \
   .app_id = "maru.app",                                                       \
   .content_type = MARU_CONTENT_TYPE_NONE,                                     \
   .decorated = true,                                                          \
   .transparent = false,                                                       \
   .userdata = NULL}

/** @brief Creates a new window. */
MARU_Status maru_createWindow(MARU_Context *context,
                              const MARU_WindowCreateInfo *create_info,
                              MARU_Window **out_window);

/** @brief Destroys a window. */
MARU_Status maru_destroyWindow(MARU_Window *window);

/** @brief Updates window attributes. */
MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                              const MARU_WindowAttributes *attributes);

/** @brief Returns a snapshot of window geometry.
 *
 * The `origin` member follows `MARU_WindowGeometry` semantics and may be
 * `{0, 0}` on platforms that do not expose global window position.
 */
void maru_getWindowGeometry(MARU_Window *window,
                            MARU_WindowGeometry *out_geometry);

/** @brief Requests input focus for a window. */
MARU_Status maru_requestWindowFocus(MARU_Window *window);

/** @brief Requests a frame callback for the window. */
MARU_Status maru_requestWindowFrame(MARU_Window *window);
/** @brief Requests user attention for the specified window (best effort). */
MARU_Status maru_requestWindowAttention(MARU_Window *window);

/** @brief Resets the metrics counters attached to a window handle. */
MARU_Status maru_resetWindowMetrics(MARU_Window *window);

#include "maru/c/details/windows.h"

#ifdef __cplusplus
}
#endif

#endif // MARU_WINDOW_H_INCLUDED
