// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <string>

class WindowModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Window"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = true;
    char title_buf_[256] = "MARU Testbed";
};
