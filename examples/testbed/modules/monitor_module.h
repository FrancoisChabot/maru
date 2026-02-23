// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

class MonitorModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Monitors"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = true;
};
