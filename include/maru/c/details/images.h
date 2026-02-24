// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_IMAGES_H_INCLUDED
#define MARU_DETAILS_IMAGES_H_INCLUDED

#include "maru/c/images.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_ImageExposed {
  void *userdata;
} MARU_ImageExposed;

static inline void *maru_getImageUserdata(const MARU_Image *image) {
  return ((const MARU_ImageExposed *)image)->userdata;
}

static inline void maru_setImageUserdata(MARU_Image *image, void *userdata) {
  ((MARU_ImageExposed *)image)->userdata = userdata;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_IMAGES_H_INCLUDED
