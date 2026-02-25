// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_H_INCLUDED
#define MARU_H_INCLUDED

/**
 * @section Threading and Memory Model
 *
 * MARU follows a "Single-Owner, Multiple-Reader" model designed for high performance
 * and predictable state management:
 *
 * 1. OWNER THREAD: The thread that creates a MARU_Context is the only thread
 *    allowed to call Mutating Functions.
 *
 * 2. MUTATING FUNCTIONS: Identified by a `MARU_Status` return type (e.g., maru_pumpEvents,
 *    maru_createWindow, maru_updateContext). These functions interact with the OS
 *    display server or modify internal library state.
 *
 * 3. PASSIVE ACCESSORS: Functions that return values directly (e.g., maru_isWindowReady,
 *    maru_getMonitorScale, maru_getWindowUserdata). These are zero-cost reads from
 *    cached state. They are safe to call from any thread provided the user ensures
 *    EXTERNAL SYNCHRONIZATION (e.g., a mutex or memory barrier) with the Owner Thread
 *    to guarantee memory visibility.
 *
 * 4. GLOBALLY THREAD-SAFE: A small set of functions are internally synchronized and
 *    can be called from any thread at any time:
 *    - maru_postEvent()
 *    - maru_wakeContext()
 *    - maru_retain*() / maru_release*()
 */

#include "maru/c/core.h"
#include "maru/c/contexts.h"
#include "maru/c/controllers.h"
#include "maru/c/cursors.h"
#include "maru/c/data_exchange.h"
#include "maru/c/events.h"
#include "maru/c/geometry.h"
#include "maru/c/images.h"
#include "maru/c/inputs.h"
#include "maru/c/instrumentation.h"
#include "maru/c/metrics.h"
#include "maru/c/monitors.h"
#include "maru/c/convenience.h"
#include "maru/c/tuning.h"
#include "maru/c/vulkan.h"
#include "maru/c/windows.h"

#endif
