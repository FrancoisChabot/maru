// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "maru/maru.h"
#include "feature_module.h"
#include "imgui.h"
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>

enum class AppStatus {
    KEEP_GOING,
    EXIT,
    RECREATE_CONTEXT
};

class App {
public:
    App(MARU_Window* window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t queue_family, VkQueue queue, VkDescriptorPool descriptor_pool);
    ~App();

    void onEvent(MARU_EventType type, MARU_Window* window, const MARU_Event& event);
    void onDiagnostic(const MARU_DiagnosticInfo* info);
    AppStatus update(MARU_Context* ctx, MARU_Window* window);
    void renderUi(MARU_Context* ctx, MARU_Window* window);
    void onContextRecreated(MARU_Context* ctx, MARU_Window* window);

    ImVec4 getClearColor() const { return clear_color_; }

private:
    MARU_Window* primary_window_handle_;
    bool exit_requested_ = false;
    ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::vector<std::unique_ptr<FeatureModule>> modules_;
    class InstrumentationModule* inst_module_ = nullptr;
};
