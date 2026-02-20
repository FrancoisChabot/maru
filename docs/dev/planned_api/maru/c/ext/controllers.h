/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EXT_CONTROLLER_H_INCLUDED
#define MARU_EXT_CONTROLLER_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/inputs.h"
#include "maru/c/metrics.h"

/**
 * @file controllers.h
 * @brief Game controller and gamepad support extension.
 */

#ifdef __cplusplus
extern "C" {
#endif


#ifdef MARU_ENABLE_CONTROLLERS
/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;

/** @brief Opaque handle representing an active controller connection. 
 *
 *  Handles obtained via maru_getControllers() or within events are TRANSIENT 
 *  and only valid until the next call to maru_pumpEvents().
 * 
 *  If you need to hold on to a controller handle longer, use maru_retainController().
 *  Every call to maru_retainController() must be matched by a call to maru_releaseController().
 */
typedef struct MARU_Controller MARU_Controller;

/** @brief Detailed hardware information for a controller. */
typedef struct MARU_ControllerInfo {
  const char *name;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  uint8_t guid[16];
  bool is_standardized;
} MARU_ControllerInfo;

/** @brief Event data for controller hotplugging. */
typedef struct MARU_ControllerConnectionEvent {
  MARU_Controller *controller;
  bool connected;
} MARU_ControllerConnectionEvent;

/** @brief Event data for listening to button events. */
typedef struct MARU_ControllerButtonStateChanged {
  MARU_Controller *controller;
  MARU_ButtonState state;
  uint32_t button_id;
} MARU_ControllerButtonStateChanged;

/** @brief Runtime state flags for a controller. */
enum MARU_ControllerStateFlagBits {
  MARU_CONTROLLER_STATE_LOST = 1ULL << 0,
};

/** @brief Capabilities and properties of an analog input channel. */
typedef struct MARU_AnalogChannelInfo {
  const char *name;
} MARU_AnalogChannelInfo;

/** @brief Capabilities and properties of a digital button. */
typedef struct MARU_ButtonChannelInfo {
  const char *name;
} MARU_ButtonChannelInfo;

/** @brief Capabilities and properties of a haptic feedback channel. */
typedef struct MARU_HapticChannelInfo {
  const char *name;
} MARU_HapticChannelInfo;

/* ----- Passive Accessors (External Synchronization Required) ----- 
 *
 * These functions are essentially zero-cost member accesses. They are safe to 
 * call from any thread, provided the access is synchronized with mutating 
 * operations (like maru_pumpEvents) on the same controller to ensure memory visibility.
 */

/** @brief Retrieves the user-defined data pointer associated with a controller. */
static inline void *maru_getControllerUserdata(const MARU_Controller *controller);

/** @brief Sets the user-defined data pointer associated with a controller. */
static inline void maru_setControllerUserdata(MARU_Controller *controller, void *userdata);

/** @brief Retrieves the context that owns the specified controller. */
static inline MARU_Context *maru_getControllerContext(const MARU_Controller *controller);

/** @brief Checks if the controller has been lost due to disconnection. */
static inline bool maru_isControllerLost(const MARU_Controller *controller);

/** @brief Retrieves the runtime performance metrics for a controller. */
static inline const MARU_ControllerMetrics *maru_getControllerMetrics(const MARU_Controller *controller);

/** @brief Retrieves the number of analog axes supported by the controller. */
static inline uint32_t maru_getControllerAnalogCount(const MARU_Controller *controller);

/** @brief Retrieves hardware info for all analog axes on the controller. */
static inline const MARU_AnalogChannelInfo *maru_getControllerAnalogChannelInfo(const MARU_Controller *controller);

/** @brief Retrieves the current state of all analog axes on the controller. */
static inline const MARU_AnalogInputState *maru_getControllerAnalogStates(const MARU_Controller *controller);

/** @brief Retrieves the number of digital buttons supported by the controller. */
static inline uint32_t maru_getControllerButtonCount(const MARU_Controller *controller);

/** @brief Retrieves hardware info for all digital buttons on the controller. */
static inline const MARU_ButtonChannelInfo *maru_getControllerButtonChannelInfo(const MARU_Controller *controller);

/** @brief Retrieves the current state of all digital buttons on the controller. */
static inline const MARU_ButtonState8 *maru_getControllerButtonStates(const MARU_Controller *controller);

/** @brief Checks if a specific controller button is currently pressed. */
static inline bool maru_isControllerButtonPressed(const MARU_Controller *controller, uint32_t button_id);

/** @brief Retrieves the number of haptic feedback channels on the controller. */
static inline uint32_t maru_getControllerHapticCount(const MARU_Controller *controller);

/** @brief Retrieves info for all haptic feedback channels on the controller. */
static inline const MARU_HapticChannelInfo *maru_getControllerHapticChannelInfo(const MARU_Controller *controller);

/** @brief Standardized analog axis indices. */
typedef enum MARU_ControllerAnalog {
  MARU_CONTROLLER_ANALOG_LEFT_X = 0,
  MARU_CONTROLLER_ANALOG_LEFT_Y = 1,
  MARU_CONTROLLER_ANALOG_RIGHT_X = 2,
  MARU_CONTROLLER_ANALOG_RIGHT_Y = 3,
  MARU_CONTROLLER_ANALOG_LEFT_TRIGGER = 4,
  MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER = 5,
  MARU_CONTROLLER_ANALOG_STANDARD_COUNT = 6
} MARU_ControllerAnalog;

/** @brief Standardized button indices. */
typedef enum MARU_ControllerButton {
  MARU_CONTROLLER_BUTTON_SOUTH = 0,
  MARU_CONTROLLER_BUTTON_EAST = 1,
  MARU_CONTROLLER_BUTTON_WEST = 2,
  MARU_CONTROLLER_BUTTON_NORTH = 3,
  MARU_CONTROLLER_BUTTON_LB = 4,
  MARU_CONTROLLER_BUTTON_RB = 5,
  MARU_CONTROLLER_BUTTON_BACK = 6,
  MARU_CONTROLLER_BUTTON_START = 7,
  MARU_CONTROLLER_BUTTON_GUIDE = 8,
  MARU_CONTROLLER_BUTTON_L_THUMB = 9,
  MARU_CONTROLLER_BUTTON_R_THUMB = 10,
  MARU_CONTROLLER_BUTTON_DPAD_UP = 11,
  MARU_CONTROLLER_BUTTON_DPAD_RIGHT = 12,
  MARU_CONTROLLER_BUTTON_DPAD_DOWN = 13,
  MARU_CONTROLLER_BUTTON_DPAD_LEFT = 14,
  MARU_CONTROLLER_BUTTON_STANDARD_COUNT = 15
} MARU_ControllerButton;

/** @brief Standardized haptic channel indices. */
typedef enum MARU_ControllerHaptic {
  MARU_CONTROLLER_HAPTIC_LOW_FREQ = 0,
  MARU_CONTROLLER_HAPTIC_HIGH_FREQ = 1,
  MARU_CONTROLLER_HAPTIC_LEFT_TRIGGER = 2,
  MARU_CONTROLLER_HAPTIC_RIGHT_TRIGGER = 3,
  MARU_CONTROLLER_HAPTIC_STANDARD_COUNT = 4
} MARU_ControllerHaptic;

/** @brief A transient list of controller handles.
 *
 * The `controllers` pointer is owned by the library and is only valid until the next
 * call to maru_pumpEvents().
 */
typedef struct MARU_ControllerList {
  MARU_Controller *const *controllers;
  uint32_t count;
} MARU_ControllerList;

/** @brief Retrieves the list of currently connected controllers. 

The returned list is valid until the next call to maru_pumpEvents().
*/
MARU_Status maru_getControllers(MARU_Context *context, MARU_ControllerList *out_list);

/** @brief Increments the reference count of a controller handle. 
 *
 * This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 * without external synchronization.
 */
MARU_Status maru_retainController(MARU_Controller *controller);

/** @brief Decrements the reference count of a controller handle. 
 *
 * This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 * without external synchronization.
 */
MARU_Status maru_releaseController(MARU_Controller *controller);

/** @brief Resets the metrics counters attached to a controller handle. */
MARU_Status maru_resetControllerMetrics(MARU_Controller *controller);

/** @brief Retrieves detailed information about a controller. */
MARU_Status maru_getControllerInfo(MARU_Controller *controller,
                                               MARU_ControllerInfo *out_info);

/** @brief Sets haptic feedback levels for a controller. */
MARU_Status maru_setControllerHapticLevels(MARU_Controller *controller,
                                                      uint32_t first_haptic, uint32_t count,
                                                      const MARU_Scalar *intensities);

#include "maru/c/details/controllers.h"

#endif  // MARU_ENABLE_CONTROLLERS

#ifdef __cplusplus
}
#endif

#endif  // MARU_EXT_CONTROLLER_H_INCLUDED
