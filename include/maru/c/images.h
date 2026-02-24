// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_IMAGES_H_INCLUDED
#define MARU_IMAGES_H_INCLUDED

#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/contexts.h"
#include "maru/c/geometry.h"

/**
 * @file images.h
 * @brief Immutable image handles used by cursor and icon APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing an immutable image. */
typedef struct MARU_Image MARU_Image;

/** @brief Parameters for maru_createImage(). */
typedef struct MARU_ImageCreateInfo {
  MARU_Vec2Px size;
  const uint32_t *pixels;
  uint32_t stride_bytes;
  void *userdata;
} MARU_ImageCreateInfo;

/** @brief Creates an immutable image from ARGB8888 pixels. */
MARU_Status maru_createImage(MARU_Context *context,
                             const MARU_ImageCreateInfo *create_info,
                             MARU_Image **out_image);

/** @brief Destroys an image handle. */
MARU_Status maru_destroyImage(MARU_Image *image);

/** @brief Retrieves image user data. */
static inline void *maru_getImageUserdata(const MARU_Image *image);

/** @brief Updates image user data. */
static inline void maru_setImageUserdata(MARU_Image *image, void *userdata);

#include "maru/c/details/images.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_IMAGES_H_INCLUDED
