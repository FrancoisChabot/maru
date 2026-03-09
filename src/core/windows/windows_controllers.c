// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_getControllers_Windows(MARU_Context *context, MARU_ControllerList *out_list) {
  (void)context;
  if (out_list) {
    out_list->controllers = NULL;
    out_list->count = 0;
  }
  return MARU_FAILURE;
}

MARU_Status maru_retainController_Windows(MARU_Controller *controller) {
  (void)controller;
  return MARU_FAILURE;
}

MARU_Status maru_releaseController_Windows(MARU_Controller *controller) {
  (void)controller;
  return MARU_FAILURE;
}

MARU_Status maru_resetControllerMetrics_Windows(MARU_Controller *controller) {
  (void)controller;
  return MARU_FAILURE;
}

MARU_Status maru_getControllerInfo_Windows(MARU_Controller *controller, MARU_ControllerInfo *out_info) {
  (void)controller;
  (void)out_info;
  return MARU_FAILURE;
}

MARU_Status maru_setControllerHapticLevels_Windows(MARU_Controller *controller, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
  (void)controller;
  (void)first_haptic;
  (void)count;
  (void)intensities;
  return MARU_FAILURE;
}

void _maru_controller_free(MARU_Controller_Base *controller) {
  (void)controller;
}
