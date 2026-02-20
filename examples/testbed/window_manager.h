// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once
#include "maru/maru.h"
#include <vulkan/vulkan.h>

struct WindowManager_VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t queueFamilyIndex;
};

void WindowManager_Init(MARU_Context* context, MARU_Window* main_window, const WindowManager_VulkanContext& vk_ctx);
void WindowManager_Render(MARU_Context* context, bool* p_open);
void WindowManager_HandleEvent(MARU_EventType type, MARU_Window* window, const MARU_Event* event);
void WindowManager_Update();
void WindowManager_Cleanup();
