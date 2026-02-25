// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "controller_module.h"
#include "imgui.h"
#include "maru/c/controllers.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <vector>

ControllerModule::~ControllerModule() {
    releaseAll();
}

void ControllerModule::releaseAll() {
    for (auto& pair : controllers_) {
        if (pair.second.controller) {
            maru_releaseController(pair.second.controller);
        }
    }
    controllers_.clear();
}

void ControllerModule::retainController(MARU_Controller* controller) {
    if (!controller) return;
    if (controllers_.find(controller) != controllers_.end()) return;
    if (maru_retainController(controller) != MARU_SUCCESS) return;

    ControllerState state = {};
    state.controller = controller;
    if (maru_getControllerInfo(controller, &state.info) != MARU_SUCCESS) {
        maru_releaseController(controller);
        return;
    }
    state.haptic_levels.resize(maru_getControllerHapticCount(controller), 0.0f);
    controllers_.emplace(controller, std::move(state));
}

void ControllerModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)window;
    if (type == MARU_EVENT_CONTROLLER_CONNECTION_CHANGED) {
        const MARU_ControllerConnectionEvent* ev = &event.controller_connection;
        if (!ev || !ev->controller) return;
        if (ev->connected) {
            retainController(ev->controller);
        } else {
            auto it = controllers_.find(ev->controller);
            if (it != controllers_.end()) {
                maru_releaseController(it->second.controller);
                controllers_.erase(it);
            }
        }
    }
}

void ControllerModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (first_update_) {
        first_update_ = false;
        MARU_ControllerList list = {};
        if (maru_getControllers(ctx, &list) == MARU_SUCCESS) {
            for (uint32_t i = 0; i < list.count; ++i) {
                retainController(list.controllers[i]);
            }
        }
    }

    for (auto it = controllers_.begin(); it != controllers_.end();) {
        if (maru_isControllerLost(it->second.controller)) {
            maru_releaseController(it->second.controller);
            it = controllers_.erase(it);
        } else {
            ++it;
        }
    }
}

void ControllerModule::renderController(MARU_Controller* handle, ControllerState& state) {
    const char* controller_name = state.info.name ? state.info.name : "Unknown Controller";
    char label[160];
    (void)snprintf(label, sizeof(label), "%s###ctrl_%p", controller_name, (void*)handle);

    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("VID: 0x%04X PID: 0x%04X Ver: %u", state.info.vendor_id,
                state.info.product_id, state.info.version);
    ImGui::Text("Standardized: %s", state.info.is_standardized ? "Yes" : "No");

    const uint32_t analog_count = maru_getControllerAnalogCount(state.controller);
    const uint32_t button_count = maru_getControllerButtonCount(state.controller);
    const MARU_AnalogInputState* analogs = maru_getControllerAnalogStates(state.controller);
    const MARU_AnalogChannelInfo* analog_channels = maru_getControllerAnalogChannelInfo(state.controller);
    const MARU_ButtonState8* buttons = maru_getControllerButtonStates(state.controller);
    const MARU_ButtonChannelInfo* button_channels = maru_getControllerButtonChannelInfo(state.controller);

    if (ImGui::BeginTable("CtrlTable", 2, ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextColumn();
        ImGui::Text("Axes (%u)", analog_count);
        for (uint32_t i = 0; i < analog_count; ++i) {
            const float value = analogs ? (float)analogs[i].value : 0.0f;
            const char* axis_name = (analog_channels && analog_channels[i].name)
                                        ? analog_channels[i].name
                                        : "(axis)";
            ImGui::PushID((int)i);
            ImGui::ProgressBar((value + 1.0f) * 0.5f, ImVec2(-FLT_MIN, 0), axis_name);
            ImGui::PopID();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Buttons (%u)", button_count);
        for (uint32_t i = 0; i < button_count; ++i) {
            const bool pressed =
                buttons && buttons[i] == (MARU_ButtonState8)MARU_BUTTON_STATE_PRESSED;
            const char* button_name = (button_channels && button_channels[i].name)
                                          ? button_channels[i].name
                                          : "(button)";
            ImGui::Selectable(button_name, pressed, 0, ImVec2(0, 0));
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    const uint32_t haptic_count = maru_getControllerHapticCount(state.controller);
    const MARU_HapticChannelInfo* haptic_channels =
        maru_getControllerHapticChannelInfo(state.controller);
    ImGui::Text("Haptics / Rumble (%u channels)", haptic_count);
    if (haptic_count > 0) {
        if (state.haptic_levels.size() != haptic_count) {
            state.haptic_levels.resize(haptic_count, 0.0f);
        }

        bool changed = false;
        for (uint32_t i = 0; i < haptic_count; ++i) {
            const char* channel_name = (haptic_channels && haptic_channels[i].name)
                                           ? haptic_channels[i].name
                                           : "(haptic)";
            ImGui::PushID((int)i);
            changed |= ImGui::SliderFloat(channel_name, &state.haptic_levels[i], 0.0f, 1.0f);
            ImGui::PopID();
        }

        if (changed) {
            std::vector<MARU_Scalar> levels(state.haptic_levels.begin(), state.haptic_levels.end());
            (void)maru_setControllerHapticLevels(state.controller, 0, (uint32_t)levels.size(),
                                                 levels.data());
        }
    } else {
        ImGui::TextDisabled("No haptic channels available.");
    }

    if (ImGui::Button("Stop Rumble")) {
        std::fill(state.haptic_levels.begin(), state.haptic_levels.end(), 0.0f);
        std::vector<MARU_Scalar> levels(state.haptic_levels.size(), (MARU_Scalar)0);
        (void)maru_setControllerHapticLevels(state.controller, 0, (uint32_t)levels.size(),
                                             levels.data());
    }
}

void ControllerModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    if (!enabled_) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Controllers", &enabled_)) {
        ImGui::End();
        return;
    }

    if (controllers_.empty()) {
        ImGui::Text("No controllers connected.");
    } else {
        if (ImGui::Button("Close All")) {
            releaseAll();
        }

        for (auto& pair : controllers_) {
            renderController(pair.first, pair.second);
        }
    }

    ImGui::End();
}

void ControllerModule::onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    releaseAll();
    first_update_ = true;
}
