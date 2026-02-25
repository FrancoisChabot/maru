// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"

#include <string>
#include <vector>

class DataExchangeModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) override;
    void onContextRecreated(MARU_Context* ctx, MARU_Window* window) override;

    const char* getName() const override { return "Data Exchange"; }
    bool& getEnabled() override { return enabled_; }

private:
    bool enabled_ = false;
    bool owns_clipboard_ = false;
    bool owns_primary_selection_ = false;
    MARU_Status last_status_ = MARU_SUCCESS;
    MARU_DataExchangeTarget last_received_target_ = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
    bool has_received_target_ = false;
    char input_buffer_[2048] = "";
    std::vector<std::string> clipboard_mime_types_;
    std::vector<std::string> primary_mime_types_;
};
