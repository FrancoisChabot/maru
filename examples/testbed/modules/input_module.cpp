// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "input_module.h"
#include "imgui.h"
#include <cstdio>
#include <vector>

void InputModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void InputModule::onEvent(MARU_EventType type, MARU_Window* window, const MARU_Event& event) {
    (void)window;
    if (type == MARU_MOUSE_MOVED) {
        mouse_pos_ = event.mouse_motion.position;
        mouse_delta_ = event.mouse_motion.delta;
        mouse_raw_delta_ = event.mouse_motion.raw_delta;
    }
}

void InputModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    if (ImGui::Begin("Input", &enabled_)) {
        MARU_WindowGeometry geometry;
        maru_getWindowGeometry(window, &geometry);

        if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: (%.1f, %.1f)", mouse_pos_.x, mouse_pos_.y);
            ImGui::Text("Delta: (%.3f, %.3f)", mouse_delta_.x, mouse_delta_.y);
            ImGui::Text("Raw Delta: (%.3f, %.3f)", mouse_raw_delta_.x, mouse_raw_delta_.y);
            
            if (ImGui::Button("Reset Deltas")) {
                mouse_delta_ = {0, 0};
                mouse_raw_delta_ = {0, 0};
            }
        }

        if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint32_t key_count = maru_getKeyboardButtonCount(window);
            const MARU_ButtonState8* states = maru_getKeyboardButtonStates(window);
            
            ImGui::Text("Key Count: %u", key_count);
            
            ImGui::Text("Pressed Keys:");
            ImGui::BeginChild("PressedKeys", ImVec2(0, 100), true);
            for (uint32_t i = 0; i < key_count; ++i) {
                if (states[i] == MARU_BUTTON_STATE_PRESSED) {
                    ImGui::Text("Key %u", i);
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}
