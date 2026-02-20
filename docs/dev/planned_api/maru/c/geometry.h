/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_GEOMETRY_H_INCLUDED
#define MARU_GEOMETRY_H_INCLUDED

#include <stdint.h>

#include "maru/c/details/maru_config.h"

/**
 * @file geometry.h
 * @brief Vectors, rectangles, and fractional types.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Generic floating point scalar type. 
WARNING! If you use MARU_USE_FLOAT, make certain that you are linking against maru_f. 
*/
#ifdef MARU_USE_FLOAT
typedef float MARU_Scalar;
#else
typedef double MARU_Scalar;
#endif

/** @brief Represents a simple fraction or aspect ratio. */
typedef struct MARU_Fraction {
  int32_t num;
  int32_t denom;
} MARU_Fraction;

/** @brief 2D vector in physical pixel coordinates. */
typedef struct MARU_Vec2Px {
  int32_t x;
  int32_t y;
} MARU_Vec2Px;

/** @brief 2D vector in logical coordinates. */
typedef struct MARU_Vec2Dip {
  MARU_Scalar x;
  MARU_Scalar y;
} MARU_Vec2Dip;

/** @brief Represents a rectangular area in logical coordinates. */
typedef struct MARU_RectDip {
  MARU_Vec2Dip origin;
  MARU_Vec2Dip size;
} MARU_RectDip;

/** @brief Snapshot of window dimensions and position. */
typedef struct MARU_WindowGeometry {
  MARU_Vec2Dip origin;
  MARU_Vec2Dip logical_size;
  MARU_Vec2Px pixel_size;
  MARU_Scalar scale;
} MARU_WindowGeometry;

#ifdef __cplusplus
}
#endif

#endif  // MARU_GEOMETRY_H_INCLUDED
