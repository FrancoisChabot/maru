#include "maru/c/core.h"
#include "wayland_internal.h"
#include "linux_controllers.h"

MARU_Status maru_getControllers_WL(MARU_Context *context,
                                   MARU_ControllerList *out_list) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  return MARU_FAILURE;
}

MARU_Status maru_retainController_WL(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_releaseController_WL(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_resetControllerMetrics_WL(MARU_Controller *controller) {
  return MARU_FAILURE;
}

MARU_Status maru_getControllerInfo_WL(MARU_Controller *controller,
                                      MARU_ControllerInfo *out_info) {
  return MARU_FAILURE;
}

MARU_Status maru_setControllerHapticLevels_WL(MARU_Controller *controller,
                                              uint32_t first_haptic,
                                              uint32_t count,
                                              const MARU_Scalar *intensities) {
  return MARU_FAILURE;
}
