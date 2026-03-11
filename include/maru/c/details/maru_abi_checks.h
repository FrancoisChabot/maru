// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_ABI_CHECKS_H_INCLUDED
#define MARU_DETAILS_ABI_CHECKS_H_INCLUDED

#include "maru/c/events.h"

/*
 * This file performs compile-time checks to ensure that the MARU library ABI
 * matches the expectations of the configuration (Float vs Double).
 * These checks require C11 (_Static_assert) or C++11 (static_assert).
 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define MARU_STATIC_ASSERT _Static_assert
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define MARU_STATIC_ASSERT static_assert
#else
#define MARU_STATIC_ASSERT(cond, msg)
#endif

/* Ensure MARU_Event fits within the guaranteed cache-line friendly size limits.
 */
#ifdef MARU_STATIC_ASSERT

#define MARU_EXPECTED_EVENT_SIZE 64

MARU_STATIC_ASSERT(
    sizeof(MARU_Event) <= MARU_EXPECTED_EVENT_SIZE,
    "MARU_Event ABI Violation: Size exceeds guaranteed threshold. ");

/* Verify user event payload size */
MARU_STATIC_ASSERT(sizeof(MARU_UserDefinedEvent) == 64,
                   "MARU_UserDefinedEvent ABI Violation: Expected 64 bytes.");

#endif

#endif /* MARU_DETAILS_ABI_CHECKS_H_INCLUDED */
