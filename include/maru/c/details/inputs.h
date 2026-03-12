// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

/**
 * ATTENTION: This file is in the maru details/ directory. This means that
 * it's NOT part of the Maru API, and is just machinery that is required to
 * implement the API.
 *
 * Nothing in here is meant to be stable, or even read by a user of the library.
 */

#ifndef MARU_DETAILS_INPUTS_H_INCLUDED
#define MARU_DETAILS_INPUTS_H_INCLUDED

#include "maru/c/details/windows.h"
#include "maru/c/inputs.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline bool maru_isKeyboardKeyPressed(const MARU_Context *context,
                                             MARU_Key key) {
  if ((uint32_t)key >= MARU_KEY_COUNT) {
    return false;
  }
  const MARU_ButtonState8 *states = maru_getKeyboardKeyStates(context);
  return states ? (states[key] == MARU_BUTTON_STATE_PRESSED) : false;
}

#ifdef __cplusplus
}
#endif

#endif // MARU_DETAILS_INPUTS_H_INCLUDED
