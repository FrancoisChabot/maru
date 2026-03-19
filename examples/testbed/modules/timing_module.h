// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <vector>

class TimingModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    void recordPumpTime(double timestamp_seconds, double pump_duration_ms);

    const char* getName() const override { return "Timing"; }
    bool& getEnabled() override { return enabled_; }

private:
    struct PumpSample {
        double timestamp_seconds;
        uint32_t frame_id;
        double pump_duration_ms;
    };

    static constexpr size_t kHistorySize = 100;

    std::vector<PumpSample> samples_;
    size_t next_sample_index_ = 0;
    uint32_t current_frame_id_ = 0;
    bool samples_full_ = false;
    bool enabled_ = true;
    bool paused_ = false;
    float auto_pause_threshold_ms_ = 8.0f;

    void clearSamples();
};
