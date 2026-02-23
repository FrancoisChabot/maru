// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_CONTEXTS_H_INCLUDED
#define MARU_DETAILS_CONTEXTS_H_INCLUDED

#include "maru/c/contexts.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Internal representation of MARU_Context. 
    CHANGING THIS REQUIRES A MAJOR VERSION BUMP
*/
typedef struct MARU_ContextExposed {
  void *userdata;
  uint64_t flags;
  MARU_BackendType backend_type;
  const MARU_ContextMetrics *metrics;
  void *extensions[MARU_EXT_COUNT];
} MARU_ContextExposed;

static inline MARU_BackendType maru_getContextBackend(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->backend_type;
}

static inline void *maru_getContextUserdata(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->userdata;
}

static inline void maru_setContextUserdata(MARU_Context *context, void *userdata) {
  ((MARU_ContextExposed *)context)->userdata = userdata;
}

static inline bool maru_isContextLost(const MARU_Context *context) {
  return (((const MARU_ContextExposed *)context)->flags & MARU_CONTEXT_STATE_LOST) != 0;
}

static inline bool maru_isContextReady(const MARU_Context *context) {
  return (((const MARU_ContextExposed *)context)->flags & MARU_CONTEXT_STATE_READY) != 0;
}

static inline const MARU_ContextMetrics *maru_getContextMetrics(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->metrics;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_CONTEXTS_H_INCLUDED
