// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

class ContextModule : public FeatureModule {
public:
    ContextModule();
    ~ContextModule() override;

    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Context"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = true;
    bool inhibit_idle_ = false;
    int idle_timeout_ms_ = 0;
};
