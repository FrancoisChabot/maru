// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_createImage_Windows(MARU_Context *context,
                                     const MARU_ImageCreateInfo *create_info,
                                     MARU_Image **out_image) {
  (void)context;
  (void)create_info;
  (void)out_image;
  return MARU_FAILURE;
}

MARU_Status maru_destroyImage_Windows(MARU_Image *image) {
  (void)image;
  return MARU_FAILURE;
}
