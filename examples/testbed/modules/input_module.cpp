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

        auto render_default_map = [](const char* label, const int32_t* defaults) {
            static const char* kDefaultNames[] = {
                "Left", "Right", "Middle", "Back", "Forward"
            };
            ImGui::Text("%s", label);
            for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
                ImGui::BulletText("%s -> %d", kDefaultNames[i], defaults[i]);
            }
        };

        auto render_mouse_channels = [](const char* child_id,
                                        uint32_t count,
                                        const MARU_ButtonState8* states,
                                        const MARU_MouseButtonChannelInfo* channels) {
            ImGui::BeginChild(child_id, ImVec2(0, 160), true);
            if (ImGui::BeginTable("MouseChannelsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Pressed");
                ImGui::TableSetupColumn("Default");
                ImGui::TableSetupColumn("Native");
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();

                for (uint32_t i = 0; i < count; ++i) {
                    const bool pressed = states && states[i] == MARU_BUTTON_STATE_PRESSED;
                    const bool is_default = channels && channels[i].is_default;
                    const uint32_t native_code = channels ? channels[i].native_code : 0;
                    const char* name = (channels && channels[i].name) ? channels[i].name : "(none)";

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u", i);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(pressed ? "yes" : "no");
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(is_default ? "yes" : "no");
                    ImGui::TableNextColumn(); ImGui::Text("%u", native_code);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        };

        if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: (%.1f, %.1f)", mouse_pos_.x, mouse_pos_.y);
            ImGui::Text("Delta: (%.3f, %.3f)", mouse_delta_.x, mouse_delta_.y);
            ImGui::Text("Raw Delta: (%.3f, %.3f)", mouse_raw_delta_.x, mouse_raw_delta_.y);
            
            if (ImGui::Button("Reset Deltas")) {
                mouse_delta_ = {0, 0};
                mouse_raw_delta_ = {0, 0};
            }

            const uint32_t win_btn_count = maru_getMouseButtonCount(window);
            const MARU_ButtonState8* win_btn_states = maru_getMouseButtonStates(window);
            const MARU_MouseButtonChannelInfo* win_btn_channels = maru_getMouseButtonChannelInfo(window);

            const uint32_t ctx_btn_count = maru_getContextMouseButtonCount(ctx);
            const MARU_ButtonState8* ctx_btn_states = maru_getContextMouseButtonStates(ctx);
            const MARU_MouseButtonChannelInfo* ctx_btn_channels = maru_getContextMouseButtonChannelInfo(ctx);

            ImGui::Separator();
            ImGui::Text("Window Mouse Channels: %u", win_btn_count);
            render_mouse_channels("WindowMouseChannels", win_btn_count, win_btn_states, win_btn_channels);
            int32_t win_defaults[MARU_MOUSE_DEFAULT_COUNT];
            for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
                win_defaults[i] = maru_getMouseDefaultButtonChannel(window, (MARU_MouseDefaultButton)i);
            }
            render_default_map("Window Default Mapping", win_defaults);

            ImGui::Separator();
            ImGui::Text("Context Mouse Channels: %u", ctx_btn_count);
            render_mouse_channels("ContextMouseChannels", ctx_btn_count, ctx_btn_states, ctx_btn_channels);
            int32_t ctx_defaults[MARU_MOUSE_DEFAULT_COUNT];
            for (uint32_t i = 0; i < MARU_MOUSE_DEFAULT_COUNT; ++i) {
                ctx_defaults[i] = maru_getContextMouseDefaultButtonChannel(ctx, (MARU_MouseDefaultButton)i);
            }
            render_default_map("Context Default Mapping", ctx_defaults);

            ImGui::Text("Pressed Context Channels:");
            ImGui::BeginChild("PressedContextChannels", ImVec2(0, 80), true);
            for (uint32_t i = 0; i < ctx_btn_count; ++i) {
                if (maru_isContextMouseButtonPressed(ctx, i)) {
                    const char* name = (ctx_btn_channels && ctx_btn_channels[i].name) ? ctx_btn_channels[i].name : "(unnamed)";
                    ImGui::BulletText("%u (%s)", i, name);
                }
            }
            ImGui::EndChild();
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
