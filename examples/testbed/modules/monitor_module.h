// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

class MonitorModule : public FeatureModule {
public:
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Monitors"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = false;
    bool refresh_requested_ = true;
    bool auto_refresh_on_events_ = true;
    bool has_mode_set_result_ = false;
    MARU_Status last_mode_set_status_ = MARU_FAILURE;
};
