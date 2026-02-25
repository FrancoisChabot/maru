// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_CONTROLLERS_H_INCLUDED
#define MARU_LINUX_CONTROLLERS_H_INCLUDED

#include "linux_internal.h"

void _maru_linux_controllers_init(MARU_Context_Base *ctx_base,
                                  MARU_ControllerContext_Linux *ctrl_ctx);
void _maru_linux_controllers_poll(MARU_Context_Base *ctx_base,
                                  MARU_ControllerContext_Linux *ctrl_ctx,
                                  bool handle_hotplug);
void _maru_linux_controllers_get_pollfds(
    MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
    const struct pollfd **out_fds, uint32_t *out_count);
void _maru_linux_controllers_cleanup(MARU_Context_Base *ctx_base,
                                     MARU_ControllerContext_Linux *ctrl_ctx);

MARU_Status _maru_linux_getControllers(MARU_Context_Base *ctx_base,
                                       MARU_ControllerContext_Linux *ctrl_ctx,
                                       MARU_ControllerList *out_list);
MARU_Status _maru_linux_retainController(MARU_Controller *controller);
MARU_Status _maru_linux_releaseController(MARU_Controller *controller);
MARU_Status _maru_linux_resetControllerMetrics(MARU_Controller *controller);
MARU_Status _maru_linux_getControllerInfo(MARU_Controller *controller,
                                          MARU_ControllerInfo *out_info);
MARU_Status _maru_linux_setControllerHapticLevels(
    MARU_Controller *controller, uint32_t first_haptic, uint32_t count,
    const MARU_Scalar *intensities);

#endif // MARU_LINUX_CONTROLLERS_H_INCLUDED
