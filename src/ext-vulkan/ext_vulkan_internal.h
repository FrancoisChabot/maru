#ifndef MARU_EXT_VULKAN_INTERNAL_H_INCLUDED
#define MARU_EXT_VULKAN_INTERNAL_H_INCLUDED

#include "maru/c/ext/vulkan.h"

typedef struct MARU_ExtVulkanVTable {
  const char **(*getVkExtensions)(const MARU_Context *context,
                                  uint32_t *out_count);
  MARU_Status (*createVkSurface)(MARU_Window *window, VkInstance instance,
                                 MARU_VkGetInstanceProcAddrFunc vk_loader,
                                 VkSurfaceKHR *out_surface);
} MARU_ExtVulkanVTable;

#endif
