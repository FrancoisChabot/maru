// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_VULKAN_HPP_INCLUDED
#define MARU_VULKAN_HPP_INCLUDED

#include "maru/vulkan.h"
#include "maru/cpp/context.hpp"
#include "maru/cpp/window.hpp"

namespace maru {

/**
 * @brief Returns the required Vulkan instance extensions for the given context.
 *
 * @param context The maru context.
 * @return std::vector<const char*> A vector of extension names.
 */
inline std::vector<const char*> getVkExtensions(const Context& context) {
    MARU_StringList list = {};
    MARU_Status status = maru_getVkExtensions(context.get(), &list);
    if (status != MARU_SUCCESS || !list.strings || list.count == 0) return std::vector<const char*>();
    return std::vector<const char*>(list.strings, list.strings + list.count);
}

/**
 * @brief Creates a Vulkan surface for the given window.
 *
 * @param window The maru window.
 * @param instance The Vulkan instance.
 * @param vk_loader The Vulkan instance proc addr function.
 * @return expected<VkSurfaceKHR> The created surface or an error status.
 */
inline expected<VkSurfaceKHR> createVkSurface(Window& window, VkInstance instance,
                                              MARU_VkGetInstanceProcAddrFunc vk_loader) {
    VkSurfaceKHR surface = nullptr;
    MARU_Status status = maru_createVkSurface(window.get(), instance, vk_loader, &surface);
    if (status != MARU_SUCCESS) return unexpected<MARU_Status>(status);
    return surface;
}

} // namespace maru

#endif // MARU_VULKAN_HPP_INCLUDED
