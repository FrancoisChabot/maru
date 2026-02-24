// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

class CursorModule : public FeatureModule {
public:
    ~CursorModule() override;
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Cursors"; }
    bool& getEnabled() override { return enabled_; }

    MARU_CursorMode getRequestedMode() const { return requested_mode_; }

private:
    bool enabled_ = true;
    MARU_CursorMode requested_mode_ = MARU_CURSOR_NORMAL;
    MARU_Cursor* custom_cursor_ = nullptr;
    MARU_Cursor* standard_cursor_ = nullptr;
};
