// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Status maru_createContext_Cocoa(const MARU_ContextCreateInfo *create_info,
                                      MARU_Context **out_context) {
  return MARU_FAILURE;
}

MARU_Status maru_destroyContext_Cocoa(MARU_Context *context) {
  return MARU_FAILURE;
}

MARU_Status maru_updateContext_Cocoa(MARU_Context *context, uint64_t field_mask,
                                       const MARU_ContextAttributes *attributes) {
  return MARU_FAILURE;
}

MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  return MARU_FAILURE;
}

MARU_Status maru_wakeContext_Cocoa(MARU_Context *context) {
  return MARU_FAILURE;
}

void *_maru_getContextNativeHandle_Cocoa(MARU_Context *context) {
  return NULL;
}
