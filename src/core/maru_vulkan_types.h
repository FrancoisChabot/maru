// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_VULKAN_TYPES_H_INCLUDED
#define MARU_VULKAN_TYPES_H_INCLUDED

#include <stdint.h>

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

typedef void (*MARU_VulkanVoidFunction)(void);
typedef struct VkInstance_T* VkInstance;

#ifndef VK_USE_64_BIT_PTR_DEFINES
#if defined(__LP64__) || defined(_WIN64) ||                             \
    (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) ||     \
    defined(__powerpc64__)
#define VK_USE_64_BIT_PTR_DEFINES 1
#else
#define VK_USE_64_BIT_PTR_DEFINES 0
#endif
#endif

#if (VK_USE_64_BIT_PTR_DEFINES == 1)
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
#else
typedef uint64_t VkSurfaceKHR;
#endif

typedef MARU_VulkanVoidFunction (*MARU_VkGetInstanceProcAddrFunc)(VkInstance instance,
                                                                  const char* pName);

#endif
