// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EVENTS_H_INCLUDED
#define MARU_EVENTS_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t MARU_EventMask;
#define MARU_ALL_EVENTS (0xFFFFFFFFFFFFFFFFULL)

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Event MARU_Event;
typedef struct MARU_Window MARU_Window;

/** @brief Synchronizes the event queue with the OS and dispatches events. */
MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms);

typedef void (*MARU_EventCallback)(MARU_Window *window, const MARU_Event *event, void *userdata);

#ifdef __cplusplus
}
#endif

#endif // MARU_EVENTS_H_INCLUDED
