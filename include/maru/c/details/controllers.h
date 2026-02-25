// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_CONTROLLERS_H_INCLUDED
#define MARU_DETAILS_CONTROLLERS_H_INCLUDED

#include "maru/c/controllers.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Internal representation of MARU_Controller. 
    CHANGING THIS REQUIRES A MAJOR VERSION BUMP
*/
typedef struct MARU_ControllerExposed {
  void *userdata;
  MARU_Context *context;
  uint64_t flags;
  const MARU_ControllerMetrics *metrics;

  const MARU_AnalogChannelInfo *analog_channels;
  const MARU_AnalogInputState *analogs;
  uint32_t analog_count;

  const MARU_ButtonChannelInfo *button_channels;
  const MARU_ButtonState8 *buttons;
  uint32_t button_count;

  const MARU_HapticChannelInfo *haptic_channels;
  uint32_t haptic_count;
} MARU_ControllerExposed;

static inline void *maru_getControllerUserdata(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->userdata;
}

static inline void maru_setControllerUserdata(MARU_Controller *controller, void *userdata) {
  ((MARU_ControllerExposed *)controller)->userdata = userdata;
}

static inline MARU_Context *maru_getControllerContext(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->context;
}

static inline bool maru_isControllerLost(const MARU_Controller *controller) {
  return (((const MARU_ControllerExposed *)controller)->flags & MARU_CONTROLLER_STATE_LOST) != 0;
}

static inline const MARU_ControllerMetrics *maru_getControllerMetrics(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->metrics;
}

static inline uint32_t maru_getControllerAnalogCount(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->analog_count;
}

static inline const MARU_AnalogChannelInfo *maru_getControllerAnalogChannelInfo(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->analog_channels;
}

static inline const MARU_AnalogInputState *maru_getControllerAnalogStates(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->analogs;
}

static inline uint32_t maru_getControllerButtonCount(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->button_count;
}

static inline const MARU_ButtonChannelInfo *maru_getControllerButtonChannelInfo(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->button_channels;
}

static inline const MARU_ButtonState8 *maru_getControllerButtonStates(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->buttons;
}

static inline bool maru_isControllerButtonPressed(const MARU_Controller *controller, uint32_t button_id) {
  if (button_id >= maru_getControllerButtonCount(controller)) {
    return false;
  }
  const MARU_ButtonState8 *states = maru_getControllerButtonStates(controller);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline uint32_t maru_getControllerHapticCount(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->haptic_count;
}

static inline const MARU_HapticChannelInfo *maru_getControllerHapticChannelInfo(const MARU_Controller *controller) {
  return ((const MARU_ControllerExposed *)controller)->haptic_channels;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_CONTROLLERS_H_INCLUDED
