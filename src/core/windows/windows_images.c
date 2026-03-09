// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

MARU_Status maru_createImage_Windows(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  
  if (create_info->size.x <= 0 || create_info->size.y <= 0 || !create_info->pixels) {
    return MARU_FAILURE;
  }

  MARU_Image_Windows *img = (MARU_Image_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Image_Windows));
  if (!img) {
    return MARU_FAILURE;
  }
  memset(img, 0, sizeof(MARU_Image_Windows));
  img->base.ctx_base = &ctx->base;
  img->base.width = (uint32_t)create_info->size.x;
  img->base.height = (uint32_t)create_info->size.y;
  img->base.stride_bytes = img->base.width * 4;
  img->base.pub.userdata = create_info->userdata;

  size_t pixel_data_size = img->base.stride_bytes * img->base.height;
  img->base.pixels = (uint8_t *)maru_context_alloc(&ctx->base, pixel_data_size);
  if (!img->base.pixels) {
    maru_context_free(&ctx->base, img);
    return MARU_FAILURE;
  }

  if (create_info->stride_bytes == img->base.stride_bytes || create_info->stride_bytes == 0) {
    memcpy(img->base.pixels, create_info->pixels, pixel_data_size);
  } else {
    for (uint32_t y = 0; y < img->base.height; ++y) {
      memcpy(img->base.pixels + y * img->base.stride_bytes,
             (const uint8_t *)create_info->pixels + y * create_info->stride_bytes,
             img->base.stride_bytes);
    }
  }

  *out_image = (MARU_Image *)img;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyImage_Windows(MARU_Image *image) {
  MARU_Image_Windows *img = (MARU_Image_Windows *)image;
  if (!img) return MARU_SUCCESS;

  MARU_Context_Windows *ctx = (MARU_Context_Windows *)img->base.ctx_base;
  if (img->base.pixels) {
    maru_context_free(&ctx->base, img->base.pixels);
  }
  maru_context_free(&ctx->base, img);
  return MARU_SUCCESS;
}
