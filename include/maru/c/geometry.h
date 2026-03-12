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
 *
 * This is standardized to `double` to ensure ABI consistency. 
 * If you require `float` for specific constraints, you may modify 
 * this typedef in a private fork, but be aware it will break 
 * binary compatibility with the standard MARU build.
 */
typedef double MARU_Scalar;

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

/** @brief 2D vector in physical millimeter coordinates. */
typedef struct MARU_Vec2Mm {
  MARU_Scalar x;
  MARU_Scalar y;
} MARU_Vec2Mm;

/** @brief 2D vector in dip coordinates. */
typedef struct MARU_Vec2Dip {
  MARU_Scalar x;
  MARU_Scalar y;
} MARU_Vec2Dip;

/** @brief Represents a rectangular area in dip coordinates. */
typedef struct MARU_RectDip {
  MARU_Vec2Dip dip_origin;
  MARU_Vec2Dip dip_size;
} MARU_RectDip;

/** @brief Orientation/flip transform for a window buffer. */
typedef enum MARU_BufferTransform {
  MARU_BUFFER_TRANSFORM_NORMAL = 0,
  MARU_BUFFER_TRANSFORM_90 = 1,
  MARU_BUFFER_TRANSFORM_180 = 2,
  MARU_BUFFER_TRANSFORM_270 = 3,
  MARU_BUFFER_TRANSFORM_FLIPPED = 4,
  MARU_BUFFER_TRANSFORM_FLIPPED_90 = 5,
  MARU_BUFFER_TRANSFORM_FLIPPED_180 = 6,
  MARU_BUFFER_TRANSFORM_FLIPPED_270 = 7,
} MARU_BufferTransform;

/** @brief Snapshot of window geometry state.
 *
 * `dip_origin` is a best-effort dip window dip_position in a global desktop space
 * when the platform exposes one. On platforms where global placement is
 * compositor/OS-controlled or not observable, `dip_origin` is guaranteed to be
 * `{0, 0}`.
 */
typedef struct MARU_WindowGeometry {
  MARU_Vec2Dip dip_origin;
  MARU_Vec2Dip dip_size;
  MARU_Vec2Px px_size;
  MARU_Scalar scale;
  MARU_BufferTransform buffer_transform;
} MARU_WindowGeometry;

#ifdef __cplusplus
}
#endif

#endif  // MARU_GEOMETRY_H_INCLUDED
