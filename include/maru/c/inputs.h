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

/** @brief Identifiers for mouse buttons. */
typedef enum MARU_MouseButton {
  MARU_MOUSE_BUTTON_LEFT = 0,
  MARU_MOUSE_BUTTON_RIGHT = 1,
  MARU_MOUSE_BUTTON_MIDDLE = 2,
  MARU_MOUSE_BUTTON_4 = 3,
  MARU_MOUSE_BUTTON_5 = 4,
  MARU_MOUSE_BUTTON_6 = 5,
  MARU_MOUSE_BUTTON_7 = 6,
  MARU_MOUSE_BUTTON_8 = 7,
} MARU_MouseButton;

#ifdef __cplusplus
}
#endif

#include "maru/c/details/inputs.h"

#endif // MARU_INPUT_H_INCLUDED
