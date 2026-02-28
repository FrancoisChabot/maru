// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Status maru_getControllers_Cocoa(MARU_Context *context,
                                        MARU_ControllerList *out_list) {
  return MARU_FAILURE;
}

MARU_Status maru_retainController_Cocoa(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_releaseController_Cocoa(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_resetControllerMetrics_Cocoa(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_getControllerInfo_Cocoa(MARU_Controller *controller,
                                           MARU_ControllerInfo *out_info) {
  return MARU_FAILURE;
}

MARU_Status maru_setControllerHapticLevels_Cocoa(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
  return MARU_FAILURE;
}
