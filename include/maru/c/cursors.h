// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CURSOR_H_INCLUDED
#define MARU_CURSOR_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/contexts.h"
#include "maru/c/geometry.h"
#include "maru/c/metrics.h"

/**
 * @file cursor.h
 * @brief Hardware cursor creation and management.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing a hardware-accelerated cursor. */
typedef struct MARU_Cursor MARU_Cursor;

/** @brief Runtime state flags for a cursor. */
typedef enum MARU_CursorStateFlagBits {
  MARU_CURSOR_FLAG_SYSTEM = 1ULL << 0,
} MARU_CursorStateFlagBits;




/** @brief Standard OS cursor shapes. */
typedef enum MARU_CursorShape {
  MARU_CURSOR_SHAPE_DEFAULT = 1,
  MARU_CURSOR_SHAPE_HELP = 2,
  MARU_CURSOR_SHAPE_HAND = 3,
  MARU_CURSOR_SHAPE_WAIT = 4,
  MARU_CURSOR_SHAPE_CROSSHAIR = 5,
  MARU_CURSOR_SHAPE_TEXT = 6,
  MARU_CURSOR_SHAPE_MOVE = 7,
  MARU_CURSOR_SHAPE_NOT_ALLOWED = 8,
  MARU_CURSOR_SHAPE_EW_RESIZE = 9,
  MARU_CURSOR_SHAPE_NS_RESIZE = 10,
  MARU_CURSOR_SHAPE_NESW_RESIZE = 11,
  MARU_CURSOR_SHAPE_NWSE_RESIZE = 12,
} MARU_CursorShape;

/* ----- Passive Accessors (External Synchronization Required) ----- 
 *
 * These functions are essentially zero-cost member accesses. They are safe to 
 * call from any thread, provided the access is synchronized with mutating 
 * operations (like maru_pumpEvents) on the cursor's owner context to ensure 
 * memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a cursor. */
static inline void *maru_getCursorUserdata(const MARU_Cursor *cursor);

/** @brief Sets the user-defined data pointer associated with a cursor. */
static inline void maru_setCursorUserdata(MARU_Cursor *cursor, void *userdata);

/** @brief Checks if the cursor is a standard system cursor. */
static inline bool maru_isCursorSystem(const MARU_Cursor *cursor);

/** @brief Retrieves the runtime performance metrics for a cursor. */
static inline const MARU_CursorMetrics *maru_getCursorMetrics(const MARU_Cursor *cursor);

/** @brief Parameters for creating a custom cursor from pixels. */
typedef struct MARU_CursorCreateInfo {
  MARU_Vec2Px size;
  MARU_Vec2Px hot_spot;
  const uint32_t *pixels;
  void *userdata;
} MARU_CursorCreateInfo;

/** @brief Retrieves a handle to a standard OS cursor. */
MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                              MARU_Cursor **out_cursor);

/** @brief Creates a custom hardware cursor. */
MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor);

/** @brief Destroys a custom hardware cursor. */
MARU_Status maru_destroyCursor(MARU_Cursor *cursor);

/** @brief Resets the metrics counters attached to a cursor handle. */
MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor);

#include "maru/c/details/cursors.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_CURSOR_H_INCLUDED
