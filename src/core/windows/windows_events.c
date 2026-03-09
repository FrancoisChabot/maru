// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  (void)context;
  (void)timeout_ms;
  (void)callback;
  (void)userdata;
  return MARU_FAILURE;
}

MARU_Status maru_wakeContext_Windows(MARU_Context *context) {
  (void)context;
  return MARU_FAILURE;
}
