// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_createCursor_Windows(MARU_Context *context,
                                       const MARU_CursorCreateInfo *create_info,
                                       MARU_Cursor **out_cursor) {
  (void)context;
  (void)create_info;
  (void)out_cursor;
  return MARU_FAILURE;
}

MARU_Status maru_destroyCursor_Windows(MARU_Cursor *cursor) {
  (void)cursor;
  return MARU_FAILURE;
}

MARU_Status maru_resetCursorMetrics_Windows(MARU_Cursor *cursor) {
  (void)cursor;
  return MARU_FAILURE;
}
