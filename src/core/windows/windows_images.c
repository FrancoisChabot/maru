// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

MARU_Status maru_createImage_Windows(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  
  if (create_info->px_size.x <= 0 || create_info->px_size.y <= 0 || !create_info->pixels) {
    return MARU_FAILURE;
  }

  MARU_Image_Windows *img = (MARU_Image_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Image_Windows));
  if (!img) {
    return MARU_FAILURE;
  }
  memset(img, 0, sizeof(MARU_Image_Windows));
  img->base.ctx_base = &ctx->base;
  img->base.width = (uint32_t)create_info->px_size.x;
  img->base.height = (uint32_t)create_info->px_size.y;
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

HICON _maru_windows_create_hicon_from_image(const MARU_Image *image, bool is_icon, int hot_x, int hot_y) {
  const MARU_Image_Windows *img = (const MARU_Image_Windows *)image;
  HBITMAP hbmColor = NULL;
  HBITMAP hbmMask = NULL;
  HICON hIcon = NULL;

  BITMAPV5HEADER bi;
  memset(&bi, 0, sizeof(bi));
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = (LONG)img->base.width;
  bi.bV5Height = -(LONG)img->base.height;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  void *lpBits = NULL;
  HDC hdc = GetDC(NULL);
  hbmColor = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &lpBits, NULL, 0);
  ReleaseDC(NULL, hdc);

  if (!hbmColor || !lpBits) {
    return NULL;
  }

  memcpy(lpBits, img->base.pixels, img->base.width * img->base.height * 4);

  hbmMask = CreateBitmap((int)img->base.width, (int)img->base.height, 1, 1, NULL);

  ICONINFO ii;
  memset(&ii, 0, sizeof(ii));
  ii.fIcon = is_icon;
  ii.xHotspot = (DWORD)hot_x;
  ii.yHotspot = (DWORD)hot_y;
  ii.hbmMask = hbmMask;
  ii.hbmColor = hbmColor;

  hIcon = CreateIconIndirect(&ii);

  DeleteObject(hbmColor);
  DeleteObject(hbmMask);

  return hIcon;
}
