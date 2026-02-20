/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_INSTRUMENTATION_H_INCLUDED
#define MARU_INSTRUMENTATION_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/contexts.h"
#include "maru/c/core.h"

/**
 * @file instrumentation.h
 * @brief Error reporting APIs (metrics live on handles via `maru/metrics.h`).
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Categorization of library-side errors and warnings. */
typedef enum MARU_Diagnostic {
  MARU_DIAGNOSTIC_NONE = 0,
  MARU_DIAGNOSTIC_INFO,
  MARU_DIAGNOSTIC_OUT_OF_MEMORY,
  MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
  MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE,
  MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
  MARU_DIAGNOSTIC_BACKEND_FAILURE,
  MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE,
  MARU_DIAGNOSTIC_VULKAN_FAILURE,
  MARU_DIAGNOSTIC_UNKNOWN,

  MARU_DIAGNOSTIC_INVALID_ARGUMENT,
  MARU_DIAGNOSTIC_PRECONDITION_FAILURE,
  MARU_DIAGNOSTIC_INTERNAL,
} MARU_Diagnostic;

/** @brief Detailed information about a diagnostic event. */
typedef struct MARU_DiagnosticInfo {
  MARU_Diagnostic diagnostic;
  const char *message;
  MARU_Context *context;
  MARU_Window *window;
} MARU_DiagnosticInfo;

#ifdef __cplusplus
}
#endif

#endif // MARU_INSTRUMENTATION_H_INCLUDED
