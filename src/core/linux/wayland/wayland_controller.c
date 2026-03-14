// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <string.h>

MARU_Status maru_getControllers_WL(const MARU_Context *context,
                                   MARU_ControllerList *out_list) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return _maru_linux_common_get_controllers(&ctx->linux_common, out_list);
}

void maru_retainController_WL(MARU_Controller *controller) {
  _maru_linux_common_retain_controller(controller);
}

void maru_releaseController_WL(MARU_Controller *controller) {
  _maru_linux_common_release_controller(controller);
}

MARU_Status maru_setControllerHapticLevels_WL(MARU_Controller *controller,
                                              uint32_t first_haptic,
                                              uint32_t count,
                                              const MARU_Scalar *intensities) {
  MARU_LinuxController *ctrl = (MARU_LinuxController *)controller;
  MARU_Context_WL *ctx = (MARU_Context_WL *)ctrl->base.context;
  return _maru_linux_common_set_haptic_levels(&ctx->linux_common, ctrl, first_haptic, count, intensities);
}
