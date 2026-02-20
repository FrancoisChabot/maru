#ifndef MARU_EXT_VULKAN_INTERNAL_H_INCLUDED
#define MARU_EXT_VULKAN_INTERNAL_H_INCLUDED

#include "maru/c/ext/vulkan.h"

typedef struct MARU_ExtVulkanVTable {
  __typeof__(maru_vulkan_getVkExtensions) *getVkExtensions;
  __typeof__(maru_vulkan_createVkSurface) *createVkSurface;
} MARU_ExtVulkanVTable;

#endif
