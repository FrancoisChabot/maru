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
  const MARU_ButtonState8 *mouse_button_state;
  const MARU_MouseButtonChannelInfo *mouse_button_channels;
  uint32_t mouse_button_count;
  int32_t mouse_default_button_channels[MARU_MOUSE_DEFAULT_COUNT];
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

static inline uint32_t maru_getContextMouseButtonCount(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->mouse_button_count;
}

static inline const MARU_ButtonState8 *maru_getContextMouseButtonStates(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->mouse_button_state;
}

static inline const MARU_MouseButtonChannelInfo *maru_getContextMouseButtonChannelInfo(const MARU_Context *context) {
  return ((const MARU_ContextExposed *)context)->mouse_button_channels;
}

static inline int32_t maru_getContextMouseDefaultButtonChannel(const MARU_Context *context,
                                                               MARU_MouseDefaultButton which) {
  if ((uint32_t)which >= MARU_MOUSE_DEFAULT_COUNT) {
    return -1;
  }
  return ((const MARU_ContextExposed *)context)->mouse_default_button_channels[which];
}

static inline bool maru_isContextMouseButtonPressed(const MARU_Context *context, uint32_t button_id) {
  uint32_t count = maru_getContextMouseButtonCount(context);
  if (button_id >= count) {
    return false;
  }
  const MARU_ButtonState8 *states = maru_getContextMouseButtonStates(context);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_CONTEXTS_H_INCLUDED
