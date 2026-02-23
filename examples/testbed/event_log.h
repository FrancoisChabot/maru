#pragma once

#include "maru/maru.h"
#include <vector>
#include <string>
#include <map>

class EventLog {
public:
    EventLog();
    void onFrameBegin();
    void onEvent(MARU_EventType type, MARU_Window* win, const MARU_Event& e);
    void render();

    bool enabled = true;

private:
    struct LogEntry {
        double timestamp;
        uint32_t frame_id;
        MARU_EventType type;
        std::string details;
        std::string window_title;
        MARU_Window* window_handle;
    };

    std::vector<LogEntry> logs;
    size_t max_logs = 2000;
    size_t next_index = 0;
    uint32_t current_frame_id = 0;
    bool full = false;

    std::map<MARU_Window*, std::string> window_titles;

    bool auto_scroll = true;
    bool show_details = true;

    const char* typeToString(MARU_EventType type);
};
