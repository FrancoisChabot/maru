// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_METRICS_H_INCLUDED
#define MARU_METRICS_H_INCLUDED

#include <stdint.h>
#include "maru/c/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Runtime performance metrics for a window. */
typedef struct MARU_WindowMetrics {
    uint64_t frame_count;
    // placeholder for more
} MARU_WindowMetrics;

#ifdef __cplusplus
}
#endif

#endif // MARU_METRICS_H_INCLUDED
