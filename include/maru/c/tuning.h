// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_TUNING_H_INCLUDED
#define MARU_TUNING_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"

/**
 * @file tuning.h
 * @brief Backend-specific and internal library tuning structures.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Window decoration strategies for Wayland backends. */
typedef enum MARU_WaylandDecorationMode {
  MARU_WAYLAND_DECORATION_MODE_AUTO = 0,
  MARU_WAYLAND_DECORATION_MODE_SSD = 1,
  MARU_WAYLAND_DECORATION_MODE_CSD = 2,
  MARU_WAYLAND_DECORATION_MODE_NONE = 3,
} MARU_WaylandDecorationMode;

/** @brief Generic function pointer for Vulkan return types. */
typedef void (*MARU_VulkanVoidFunction)(void);

/** @brief Function pointer signature for vkGetInstanceProcAddr. */
typedef MARU_VulkanVoidFunction (*MARU_VkGetInstanceProcAddrFunc)(void *instance,
                                                                  const char *pName);

/** @brief Backend-specific and internal library tuning. */

//N.B. If you are reviewing this API, imagine that this will be extended as needed as the backends are built...
typedef struct MARU_ContextTuning {
  uint32_t user_event_queue_size;

  struct {
    MARU_WaylandDecorationMode decoration_mode;
    uint32_t dnd_post_drop_timeout_ms;
    uint32_t cursor_size;
    bool enforce_client_side_constraints;
  } wayland;

  struct {
    uint32_t idle_poll_interval_ms;
  } x11;
} MARU_ContextTuning;

/** @brief Default tuning parameters. */
#define MARU_CONTEXT_TUNING_DEFAULT                           \
  {                                                            \
    .user_event_queue_size = 256, \
    .wayland = { \
      .decoration_mode = MARU_WAYLAND_DECORATION_MODE_AUTO, \
      .dnd_post_drop_timeout_ms = 1000, \
      .cursor_size = 24, \
      .enforce_client_side_constraints = true \
    }, \
    .x11 = { \
      .idle_poll_interval_ms = 250 \
    }, \
  }

#ifdef __cplusplus
}
#endif

#endif  // MARU_TUNING_H_INCLUDED
