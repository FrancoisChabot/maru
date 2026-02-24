// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_INPUT_H_INCLUDED
#define MARU_INPUT_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/geometry.h"

/**
 * @file inputs.h
 * @brief Keyboard, mouse, and analog input definitions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bitmask for active keyboard modifiers. */
typedef uint64_t MARU_ModifierFlags;


/** @brief Tracks whether a button is currently pressed or released. */
typedef enum MARU_ButtonState {
  MARU_BUTTON_STATE_RELEASED = 0,
  MARU_BUTTON_STATE_PRESSED = 1,
} MARU_ButtonState;

/** @brief 1-byte version of MARU_ButtonState for efficient array storage. */
typedef uint8_t MARU_ButtonState8;

/** @brief Identifiers for physical keyboard keys. */
typedef enum MARU_Key {
  MARU_KEY_UNKNOWN = 0,
  MARU_KEY_SPACE,
  MARU_KEY_APOSTROPHE,
  MARU_KEY_COMMA,
  MARU_KEY_MINUS,
  MARU_KEY_PERIOD,
  MARU_KEY_SLASH,
  MARU_KEY_0,
  MARU_KEY_1,
  MARU_KEY_2,
  MARU_KEY_3,
  MARU_KEY_4,
  MARU_KEY_5,
  MARU_KEY_6,
  MARU_KEY_7,
  MARU_KEY_8,
  MARU_KEY_9,
  MARU_KEY_SEMICOLON,
  MARU_KEY_EQUAL,
  MARU_KEY_A,
  MARU_KEY_B,
  MARU_KEY_C,
  MARU_KEY_D,
  MARU_KEY_E,
  MARU_KEY_F,
  MARU_KEY_G,
  MARU_KEY_H,
  MARU_KEY_I,
  MARU_KEY_J,
  MARU_KEY_K,
  MARU_KEY_L,
  MARU_KEY_M,
  MARU_KEY_N,
  MARU_KEY_O,
  MARU_KEY_P,
  MARU_KEY_Q,
  MARU_KEY_R,
  MARU_KEY_S,
  MARU_KEY_T,
  MARU_KEY_U,
  MARU_KEY_V,
  MARU_KEY_W,
  MARU_KEY_X,
  MARU_KEY_Y,
  MARU_KEY_Z,
  MARU_KEY_LEFT_BRACKET,
  MARU_KEY_BACKSLASH,
  MARU_KEY_RIGHT_BRACKET,
  MARU_KEY_GRAVE_ACCENT,
  MARU_KEY_ESCAPE,
  MARU_KEY_ENTER,
  MARU_KEY_TAB,
  MARU_KEY_BACKSPACE,
  MARU_KEY_INSERT,
  MARU_KEY_DELETE,
  MARU_KEY_RIGHT,
  MARU_KEY_LEFT,
  MARU_KEY_DOWN,
  MARU_KEY_UP,
  MARU_KEY_PAGE_UP,
  MARU_KEY_PAGE_DOWN,
  MARU_KEY_HOME,
  MARU_KEY_END,
  MARU_KEY_CAPS_LOCK,
  MARU_KEY_SCROLL_LOCK,
  MARU_KEY_NUM_LOCK,
  MARU_KEY_PRINT_SCREEN,
  MARU_KEY_PAUSE,
  MARU_KEY_F1,
  MARU_KEY_F2,
  MARU_KEY_F3,
  MARU_KEY_F4,
  MARU_KEY_F5,
  MARU_KEY_F6,
  MARU_KEY_F7,
  MARU_KEY_F8,
  MARU_KEY_F9,
  MARU_KEY_F10,
  MARU_KEY_F11,
  MARU_KEY_F12,
  MARU_KEY_KP_0,
  MARU_KEY_KP_1,
  MARU_KEY_KP_2,
  MARU_KEY_KP_3,
  MARU_KEY_KP_4,
  MARU_KEY_KP_5,
  MARU_KEY_KP_6,
  MARU_KEY_KP_7,
  MARU_KEY_KP_8,
  MARU_KEY_KP_9,
  MARU_KEY_KP_DECIMAL,
  MARU_KEY_KP_DIVIDE,
  MARU_KEY_KP_MULTIPLY,
  MARU_KEY_KP_SUBTRACT,
  MARU_KEY_KP_ADD,
  MARU_KEY_KP_ENTER,
  MARU_KEY_KP_EQUAL,
  MARU_KEY_LEFT_SHIFT,
  MARU_KEY_LEFT_CONTROL,
  MARU_KEY_LEFT_ALT,
  MARU_KEY_LEFT_META,
  MARU_KEY_RIGHT_SHIFT,
  MARU_KEY_RIGHT_CONTROL,
  MARU_KEY_RIGHT_ALT,
  MARU_KEY_RIGHT_META,
  MARU_KEY_MENU,
  MARU_KEY_COUNT
} MARU_Key;

/** @brief Bitmask for active modifiers. */
typedef enum MARU_ModifierFlagBits {
  MARU_MODIFIER_SHIFT = 1ULL << 0,
  MARU_MODIFIER_CONTROL = 1ULL << 1,
  MARU_MODIFIER_ALT = 1ULL << 2,
  MARU_MODIFIER_META = 1ULL << 3,
  MARU_MODIFIER_CAPS_LOCK = 1ULL << 4,
  MARU_MODIFIER_NUM_LOCK = 1ULL << 5,
} MARU_ModifierFlagBits;

/** @brief Named default mouse button roles. */
typedef enum MARU_MouseDefaultButton {
  MARU_MOUSE_DEFAULT_LEFT = 0,
  MARU_MOUSE_DEFAULT_RIGHT = 1,
  MARU_MOUSE_DEFAULT_MIDDLE = 2,
  MARU_MOUSE_DEFAULT_BACK = 3,
  MARU_MOUSE_DEFAULT_FORWARD = 4,
  MARU_MOUSE_DEFAULT_COUNT = 5,
} MARU_MouseDefaultButton;

/** @brief Metadata for a mouse button channel. */
typedef struct MARU_MouseButtonChannelInfo {
  const char *name;
  uint32_t native_code;
  bool is_default;
} MARU_MouseButtonChannelInfo;

/** @brief State for a single continuous input axis. */
typedef struct MARU_AnalogInputState {
  MARU_Scalar value;
} MARU_AnalogInputState;

/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;

/* ----- Passive Accessors (External Synchronization Required) ----- 
 *
 * These functions are essentially zero-cost member accesses. They are safe to 
 * call from any thread, provided the access is synchronized with mutating 
 * operations (like maru_pumpEvents) on the window's owner context to ensure 
 * memory visibility.
 */

/** @brief Retrieves the number of keyboard keys supported for a window. */
static inline uint32_t maru_getKeyboardButtonCount(const MARU_Window *window);

/** @brief Retrieves the current state of all keyboard keys for a window. */
static inline const MARU_ButtonState8 *maru_getKeyboardButtonStates(const MARU_Window *window);

/** @brief Checks if a specific keyboard key is currently pressed for a window. */
static inline bool maru_isKeyPressed(const MARU_Window *window, MARU_Key key);

/** @brief Retrieves the number of mouse buttons/channels for a window. */
static inline uint32_t maru_getMouseButtonCount(const MARU_Window *window);

/** @brief Retrieves the current state of all mouse buttons/channels for a window. */
static inline const MARU_ButtonState8 *maru_getMouseButtonStates(const MARU_Window *window);

/** @brief Retrieves metadata for all mouse buttons/channels for a window. */
static inline const MARU_MouseButtonChannelInfo *maru_getMouseButtonChannelInfo(const MARU_Window *window);

/** @brief Retrieves the channel index for a named default mouse button role. */
static inline int32_t maru_getMouseDefaultButtonChannel(const MARU_Window *window, MARU_MouseDefaultButton which);

/** @brief Checks if a specific mouse button/channel is currently pressed for a window. */
static inline bool maru_isMouseButtonPressed(const MARU_Window *window, uint32_t button_id);

#ifdef __cplusplus
}
#endif

#include "maru/c/details/inputs.h"

#endif  // MARU_INPUT_H_INCLUDED
