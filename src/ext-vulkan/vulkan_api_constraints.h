#ifndef MARU_VULKAN_API_CONSTRAINTS_H_INCLUDED
#define MARU_VULKAN_API_CONSTRAINTS_H_INCLUDED

#include "maru_api_constraints.h"
#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"

#ifdef MARU_VALIDATE_API_CALLS

static inline void _maru_validate_vulkan_getVkExtensions(MARU_Context *context, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
    MARU_CONSTRAINT_CHECK(maru_getExtension(context, MARU_EXT_VULKAN) != NULL);
}

static inline void _maru_validate_vulkan_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 MARU_VkGetInstanceProcAddrFunc vk_loader,
                                 VkSurfaceKHR *out_surface) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(instance != NULL);
    MARU_CONSTRAINT_CHECK(out_surface != NULL);
    MARU_CONSTRAINT_CHECK(maru_getExtension(maru_getWindowContext(window), MARU_EXT_VULKAN) != NULL);
    (void)vk_loader;
}

#define MARU_VULKAN_API_VALIDATE(fn, ...) \
    _maru_validate_vulkan_##fn(__VA_ARGS__)

#else
#define MARU_VULKAN_API_VALIDATE(fn, ...) (void)0
#endif

#endif
