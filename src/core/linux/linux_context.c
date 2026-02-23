#include "maru_api_constraints.h"

// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot





extern MARU_Status
maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                      MARU_Context **out_context);
extern MARU_Status
maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                       MARU_Context **out_context);

MARU_Status maru_createContext_Linux(const MARU_ContextCreateInfo *create_info,
                                     MARU_Context **out_context) {
  MARU_BackendType backend = create_info->backend;

  if (backend == MARU_BACKEND_WAYLAND || backend == MARU_BACKEND_UNKNOWN) {
    if (maru_createContext_WL(create_info, out_context) == MARU_SUCCESS) {
      return MARU_SUCCESS;
    }
  }

  if (backend == MARU_BACKEND_X11 || backend == MARU_BACKEND_UNKNOWN) {
    if (maru_createContext_X11(create_info, out_context) == MARU_SUCCESS) {
      return MARU_SUCCESS;
    }
  }

  return MARU_FAILURE;
}
