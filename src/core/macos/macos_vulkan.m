// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

const char **maru_getVkExtensions_Cocoa(const MARU_Context *context, uint32_t *out_count) {
  if (out_count) *out_count = 0;
  return NULL;
}

MARU_Status maru_createVkSurface_Cocoa(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  return MARU_FAILURE;
}
