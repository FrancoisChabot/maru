// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_H_INCLUDED
#define MARU_H_INCLUDED

/**
 * @section Threading and Memory Model
 *
 * MARU follows a "Single-Owner, Multiple-Reader" model designed for high
 * performance and predictable state management:
 *
 * 1. OWNER THREAD: The thread that creates a MARU_Context is the only thread
 *    allowed to call owner-thread APIs for that context.
 *
 * 2. OWNER-THREAD APIs: Unless explicitly documented otherwise, functions that
 *    return `MARU_Status` must be called from the owner thread of the target
 *    context (e.g., maru_pumpEvents, maru_createWindow, maru_updateContext).
 *
 * 3. DIRECT-RETURN APIs: Functions that do not return `MARU_Status` and
 *    access context-owned state (e.g., maru_isWindowReady,
 *    maru_getMonitorScale, maru_getWindowGeometry) can be called from any
 *    thread with EXTERNAL SYNCHRONIZATION (e.g., a mutex or memory barrier)
 *    to guarantee memory visibility with the owner thread.
 *
 * 4. GLOBALLY THREAD-SAFE: The following APIs are internally synchronized and
 *    can be called from any thread without external synchronization:
 *    - maru_postEvent()
 *    - maru_wakeContext()
 *    - maru_retain*() / maru_release*()
 *    - maru_getVersion()
 */

/**
 * A note on handles wording:
 *
 * Throughout the headers, we mention "opaque" handles. This is a statement
 * about the API, not the ABI.
 *
 * As far as you are concerned, the handles should be treated as truly opaque.
 * However, as you can see for yourself in the details/ headers, the underlying
 * structure is *partially* (the prefix) exposed. This is used by the various
 * property accessor functions to minimize overhead.
 *
 * DO NOT DOWNCAST THE HANDLES to access the members yourself. Always use the
 * API-provided accessors.
 */

#include "maru/c/contexts.h"
#include "maru/c/controllers.h"
#include "maru/c/convenience.h"
#include "maru/c/core.h"
#include "maru/c/cursors.h"
#include "maru/c/data_exchange.h"
#include "maru/c/events.h"
#include "maru/c/geometry.h"
#include "maru/c/images.h"
#include "maru/c/inputs.h"
#include "maru/c/instrumentation.h"
#include "maru/c/metrics.h"
#include "maru/c/monitors.h"
#include "maru/c/tuning.h"
#include "maru/c/vulkan.h"
#include "maru/c/windows.h"
#include "maru/maru_queue.h"

#endif
