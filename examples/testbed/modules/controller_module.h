// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <map>
#include <vector>

class ControllerModule : public FeatureModule {
public:
    ControllerModule() = default;
    ~ControllerModule() override;

    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;
    void onContextRecreated(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Controllers"; }
    bool& getEnabled() override { return enabled_; }

private:
    struct ControllerState {
        MARU_Controller* controller = nullptr;
        MARU_ControllerInfo info = {};
        std::vector<float> haptic_levels;
    };

    bool enabled_ = false;
    bool first_update_ = true;
    std::map<MARU_Controller*, ControllerState> controllers_;

    void retainController(MARU_Controller* controller);
    void releaseAll();
    void renderController(MARU_Controller* handle, ControllerState& state);
};

