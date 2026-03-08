// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <string.h>

MARU_Status maru_createImage_WL(MARU_Context *context,
                                const MARU_ImageCreateInfo *create_info,
                                MARU_Image **out_image) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  const uint32_t width = (uint32_t)create_info->size.x;
  const uint32_t height = (uint32_t)create_info->size.y;
  const uint32_t min_stride = width * 4u;
  const uint32_t stride = (create_info->stride_bytes == 0) ? min_stride : create_info->stride_bytes;

  MARU_Image_Base *image = (MARU_Image_Base *)maru_context_alloc(&ctx->base, sizeof(MARU_Image_Base));
  if (!image) {
    return MARU_FAILURE;
  }
  memset(image, 0, sizeof(MARU_Image_Base));

  const size_t packed_stride = (size_t)min_stride;
  const size_t dst_size = packed_stride * (size_t)height;
  image->pixels = (uint8_t *)maru_context_alloc(&ctx->base, dst_size);
  if (!image->pixels) {
    maru_context_free(&ctx->base, image);
    return MARU_FAILURE;
  }

  const uint8_t *src = (const uint8_t *)create_info->pixels;
  for (uint32_t y = 0; y < height; ++y) {
    memcpy(image->pixels + (size_t)y * packed_stride,
           src + (size_t)y * (size_t)stride,
           packed_stride);
  }

  image->ctx_base = &ctx->base;
#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_WL;
  image->backend = &maru_backend_WL;
#endif
  image->pub.userdata = create_info->userdata;
  image->width = width;
  image->height = height;
  image->stride_bytes = min_stride;

  *out_image = (MARU_Image *)image;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyImage_WL(MARU_Image *image_handle) {
  MARU_Image_Base *image = (MARU_Image_Base *)image_handle;
  MARU_Context_Base *ctx_base = image->ctx_base;
  maru_context_free(ctx_base, image->pixels);
  image->pixels = NULL;
  maru_context_free(ctx_base, image);
  return MARU_SUCCESS;
}
