// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <vector>
#include <string>
#include <map>

class EventLogModule : public FeatureModule {
public:
    EventLogModule();
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;
    void onEvent(MARU_EventId type, MARU_Window* win, const MARU_Event& e) override;

    const char* getName() const override { return "Event Log"; }
    bool& getEnabled() override { return enabled_; }

private:
    struct LogEntry {
        double timestamp;
        uint32_t frame_id;
        MARU_EventId type;
        std::string details;
        std::string window_title;
        MARU_Window* window_handle;
    };

    std::vector<LogEntry> logs_;
    size_t max_logs_ = 2000;
    size_t next_index_ = 0;
    uint32_t current_frame_id_ = 0;
    bool full_ = false;
    bool enabled_ = true;

    std::map<MARU_Window*, std::string> window_titles_;

    bool auto_scroll_ = true;
    bool show_details_ = true;
    bool log_frame_events_ = false;
    bool hide_mouse_motion_ = false;

    const char* typeToString(MARU_EventId type);
};
