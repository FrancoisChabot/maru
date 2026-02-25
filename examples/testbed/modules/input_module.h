// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

class InputModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;

    const char* getName() const override { return "Input"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = false;
    MARU_Vec2Dip mouse_pos_ = {0, 0};
    MARU_Vec2Dip mouse_delta_ = {0, 0};
    MARU_Vec2Dip mouse_raw_delta_ = {0, 0};
    MARU_CursorMode cursor_mode_ = MARU_CURSOR_NORMAL;
};
