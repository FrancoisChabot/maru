// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_H_INCLUDED
#define MARU_H_INCLUDED

#include "maru/c/contexts.h"
#include "maru/c/core.h"
#include "maru/c/cursors.h"
#include "maru/c/data_exchange.h"
#include "maru/c/events.h"
#include "maru/c/geometry.h"
#include "maru/c/inputs.h"
#include "maru/c/instrumentation.h"
#include "maru/c/metrics.h"
#include "maru/c/monitors.h"
#include "maru/c/convenience.h"
#include "maru/c/tuning.h"
#include "maru/c/windows.h"

#ifdef MARU_ENABLE_CONTROLLERS
#include "maru/c/ext/controllers.h"
#endif

#ifdef MARU_ENABLE_VULKAN
#include "maru/c/ext/vulkan.h"
#endif

#endif
