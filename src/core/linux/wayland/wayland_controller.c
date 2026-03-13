// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <string.h>

MARU_Status maru_getControllers_WL(MARU_Context *context,
                                   MARU_ControllerList *out_list) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  MARU_Context_Linux_Common *common = &ctx->linux_common;

  if (common->controller_count > ctx->controller_list_capacity) {
    uint32_t new_capacity = common->controller_count;
    if (new_capacity < 8) new_capacity = 8;
    MARU_Controller **new_list = (MARU_Controller **)maru_context_realloc(
        &ctx->base, ctx->controller_list_storage,
        ctx->controller_list_capacity * sizeof(MARU_Controller *),
        new_capacity * sizeof(MARU_Controller *));
    if (!new_list) return MARU_FAILURE;
    ctx->controller_list_storage = new_list;
    ctx->controller_list_capacity = new_capacity;
  }

  uint32_t i = 0;
  for (MARU_LinuxController *it = common->controllers; it; it = it->next) {
    ctx->controller_list_storage[i++] = (MARU_Controller *)it;
  }

  out_list->controllers = ctx->controller_list_storage;
  out_list->count = common->controller_count;
  return MARU_SUCCESS;
}

void maru_retainController_WL(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
}

void maru_releaseController_WL(MARU_Controller *controller) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u) {
        MARU_Context_WL *ctx = (MARU_Context_WL *)ctrl->base.context;
        _maru_linux_controller_destroy(&ctx->base, ctrl);
      }
      break;
    }
  }
}

MARU_Status maru_setControllerHapticLevels_WL(MARU_Controller *controller,
                                              uint32_t first_haptic,
                                              uint32_t count,
                                              const MARU_Scalar *intensities) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  MARU_Context_WL *ctx = (MARU_Context_WL *)ctrl->base.context;
  return _maru_linux_common_set_haptic_levels(&ctx->linux_common, ctrl, first_haptic, count, intensities);
}
