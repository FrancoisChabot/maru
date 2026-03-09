// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru/c/vulkan.h"

const char **maru_getVkExtensions_Windows(const MARU_Context *context, uint32_t *out_count) {
  (void)context;
  if (out_count) {
    *out_count = 0;
  }
  return NULL;
}

MARU_Status maru_createVkSurface_Windows(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  (void)window;
  (void)instance;
  (void)vk_loader;
  (void)out_surface;
  return MARU_FAILURE;
}
