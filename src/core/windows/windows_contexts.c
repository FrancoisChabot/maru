// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  (void)create_info;
  (void)out_context;
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_Windows(MARU_Context *context) {
  (void)context;
  return MARU_FAILURE;
}

MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  (void)context;
  (void)field_mask;
  (void)attributes;
  return MARU_FAILURE;
}

void *_maru_getContextNativeHandle_Windows(MARU_Context *context) {
  (void)context;
  return NULL;
}
