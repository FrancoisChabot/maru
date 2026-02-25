#include "wayland_internal.h"
#include "linux_controllers.h"

MARU_Status maru_getControllers_WL(MARU_Context *context,
                                   MARU_ControllerList *out_list) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  return _maru_linux_getControllers(&ctx->base, &ctx->linux_common.controller,
                                    out_list);
}

MARU_Status maru_retainController_WL(MARU_Controller *controller) {
  return _maru_linux_retainController(controller);
}

MARU_Status maru_releaseController_WL(MARU_Controller *controller) {
  return _maru_linux_releaseController(controller);
}

MARU_Status maru_resetControllerMetrics_WL(MARU_Controller *controller) {
  return _maru_linux_resetControllerMetrics(controller);
}

MARU_Status maru_getControllerInfo_WL(MARU_Controller *controller,
                                      MARU_ControllerInfo *out_info) {
  return _maru_linux_getControllerInfo(controller, out_info);
}

MARU_Status maru_setControllerHapticLevels_WL(MARU_Controller *controller,
                                              uint32_t first_haptic,
                                              uint32_t count,
                                              const MARU_Scalar *intensities) {
  return _maru_linux_setControllerHapticLevels(controller, first_haptic, count,
                                               intensities);
}
