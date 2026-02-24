// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_INPUTS_H_INCLUDED
#define MARU_DETAILS_INPUTS_H_INCLUDED

#include "maru/c/inputs.h"
#include "maru/c/details/windows.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline const MARU_ButtonState8 *maru_getKeyboardButtonStates(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->keyboard_state;
}

static inline uint32_t maru_getKeyboardButtonCount(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->keyboard_key_count;
}

static inline bool maru_isKeyPressed(const MARU_Window *window, MARU_Key key) {
  if ((uint32_t)key >= MARU_KEY_COUNT) {
    return false;
  }
  const MARU_ButtonState8 *states = maru_getKeyboardButtonStates(window);
  return states ? (states[key] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline uint32_t maru_getMouseButtonCount(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->mouse_button_count;
}

static inline const MARU_ButtonState8 *maru_getMouseButtonStates(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->mouse_button_state;
}

static inline const MARU_MouseButtonChannelInfo *maru_getMouseButtonChannelInfo(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->mouse_button_channels;
}

static inline int32_t maru_getMouseDefaultButtonChannel(const MARU_Window *window,
                                                        MARU_MouseDefaultButton which) {
  if ((uint32_t)which >= MARU_MOUSE_DEFAULT_COUNT) {
    return -1;
  }
  return ((const MARU_WindowExposed *)window)->mouse_default_button_channels[which];
}

static inline bool maru_isMouseButtonPressed(const MARU_Window *window, uint32_t button_id) {
  uint32_t count = maru_getMouseButtonCount(window);
  if (button_id >= count) {
    return false;
  }
  const MARU_ButtonState8 *states = maru_getMouseButtonStates(window);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_INPUTS_H_INCLUDED
