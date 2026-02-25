// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <string>
#include <vector>

class CompositionModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;

    const char* getName() const override { return "Composition"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = false;
    
    char multiline_buffer_[4096] = "Type here to test IME composition...";
    
    struct SessionState {
        uint64_t id;
        std::string preedit;
        uint32_t caret;
        uint32_t sel_start;
        uint32_t sel_len;
        bool active;
    };
    
    SessionState current_session_ = {0, "", 0, 0, 0, false};
    std::vector<std::string> commit_history_;
};
