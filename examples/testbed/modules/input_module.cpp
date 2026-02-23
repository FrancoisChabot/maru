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
    }
}

void InputModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    if (ImGui::Begin("Input", &enabled_)) {
        if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint32_t key_count = maru_getKeyboardButtonCount(window);
            const MARU_ButtonState8* states = maru_getKeyboardButtonStates(window);
            
            ImGui::Text("Key Count: %u", key_count);
            
            // Display pressed keys
            ImGui::Text("Pressed Keys:");
            ImGui::BeginChild("PressedKeys", ImVec2(0, 100), true);
            for (uint32_t i = 0; i < key_count; ++i) {
                if (states[i] == MARU_BUTTON_STATE_PRESSED) {
                    ImGui::Text("Key %u", i); // TODO: map to string if possible
                }
            }
            ImGui::EndChild();
        }

        if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Mouse Position: (%.1f, %.1f)", mouse_pos_.x, mouse_pos_.y);
            
            // Buttons
            ImGui::Text("Buttons:");
            for (int i = 0; i < 8; ++i) {
                // MARU doesn't seem to have a maru_isMouseButtonPressed accessor yet, 
                // but we can see it in geometry if it was there. 
                // Actually, I don't see it in geometry.
            }
        }
    }
    ImGui::End();
}
