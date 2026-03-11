// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

/**
 * ATTENTION: This file is in the maru details/ directory. This means that
 * it's NOT part of the Maru API, and is just machinery that is required to
 * implement the API.
 *
 * Nothing in here is meant to be stable, or even read by a user of the library.
 */

#ifndef MARU_DETAILS_CURSORS_H_INCLUDED
#define MARU_DETAILS_CURSORS_H_INCLUDED

#include "maru/c/cursors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Internal representation of MARU_Cursor. */
typedef struct MARU_CursorExposed {
  void *userdata;
  uint64_t flags;
  const MARU_CursorMetrics *metrics;
} MARU_CursorExposed;

static inline void *maru_getCursorUserdata(const MARU_Cursor *cursor) {
  return ((const MARU_CursorExposed *)cursor)->userdata;
}

static inline void maru_setCursorUserdata(MARU_Cursor *cursor, void *userdata) {
  ((MARU_CursorExposed *)cursor)->userdata = userdata;
}

static inline bool maru_isCursorSystem(const MARU_Cursor *cursor) {
  return (((const MARU_CursorExposed *)cursor)->flags &
          MARU_CURSOR_FLAG_SYSTEM) != 0;
}

static inline const MARU_CursorMetrics *
maru_getCursorMetrics(const MARU_Cursor *cursor) {
  return ((const MARU_CursorExposed *)cursor)->metrics;
}

#ifdef __cplusplus
}
#endif

#endif // MARU_DETAILS_CURSORS_H_INCLUDED
