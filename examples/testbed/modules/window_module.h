// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include "imgui_impl_vulkan.h"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class WindowModule : public FeatureModule {
public:
    WindowModule(MARU_Window* primary_window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t queue_family, VkQueue queue);
    ~WindowModule() override;

    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;
    void onContextRecreated(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Windowing"; }
    bool& getEnabled() override { return enabled_; }

private:
    struct SecondaryWindow {
        MARU_Window* window = nullptr;
        ImGui_ImplVulkanH_Window wd = {};
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        bool vk_window_initialized = false;
        bool should_close = false;
        bool swapchain_rebuild = false;
        std::string title;
        bool is_resizable = true;
        bool mouse_passthrough = false;
        bool primary_selection = true;
        MARU_Vec2Dip min_size = {0, 0};
        MARU_Vec2Dip max_size = {0, 0};
        MARU_Fraction aspect_ratio = {0, 0};
        ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    };

    bool enabled_ = false;
    MARU_Window* primary_window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = 0;
    VkQueue queue_ = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<SecondaryWindow>> secondary_windows_;
    bool handling_window_teardown_ = false;

    bool primary_is_primary_selection_ = true;
    MARU_Vec2Dip primary_min_size_ = {0, 0};
    MARU_Vec2Dip primary_max_size_ = {0, 0};
    MARU_Fraction primary_aspect_ratio_ = {0, 0};
    char primary_title_buf_[256] = "MARU Testbed";
    int primary_size_[2] = {1280, 800};
    int secondary_counter_ = 0;

    struct SecondaryCreateConfig {
        char title[256] = "";
        char app_id[128] = "org.birdsafe.maru.testbed";
        int logical_size[2] = {640, 480};
        bool decorated = true;
        bool transparent = false;
        bool fullscreen = false;
        bool maximized = false;
        bool resizable = true;
        bool mouse_passthrough = false;
        int content_type = 0;
    } secondary_create_;

    void createSecondaryWindow(MARU_Context* ctx);
    void renderSecondaryWindow(SecondaryWindow& sw);
    void cleanupSecondaryWindow(SecondaryWindow& sw);
};
