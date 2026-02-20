#include "event_viewer.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <stdio.h>

struct LoggedEvent {
    uint64_t frame;
    MARU_EventType type;
    std::string description;
};
static std::vector<LoggedEvent> event_log;

static const char* event_type_to_string(MARU_EventType type) {
    if (type == MARU_CLOSE_REQUESTED) return "CLOSE_REQUESTED";
    if (type == MARU_WINDOW_RESIZED) return "WINDOW_RESIZED";
    if (type == MARU_KEY_STATE_CHANGED) return "KEY_STATE_CHANGED";
    if (type == MARU_WINDOW_READY) return "WINDOW_READY";
    if (type == MARU_MOUSE_MOVED) return "MOUSE_MOVED";
    if (type == MARU_MOUSE_BUTTON_STATE_CHANGED) return "MOUSE_BUTTON_STATE_CHANGED";
    if (type == MARU_MOUSE_SCROLLED) return "MOUSE_SCROLLED";
    if (type == MARU_IDLE_STATE_CHANGED) return "IDLE_STATE_CHANGED";
    if (type == MARU_MONITOR_CONNECTION_CHANGED) return "MONITOR_CONNECTION_CHANGED";
    if (type == MARU_MONITOR_MODE_CHANGED) return "MONITOR_MODE_CHANGED";
    if (type == MARU_SYNC_POINT_REACHED) return "SYNC_POINT_REACHED";
    if (type == MARU_TEXT_INPUT_RECEIVED) return "TEXT_INPUT_RECEIVED";
    if (type == MARU_FOCUS_CHANGED) return "FOCUS_CHANGED";
    if (type == MARU_WINDOW_MAXIMIZED) return "WINDOW_MAXIMIZED";
    return "UNKNOWN";
}

void EventViewer_HandleEvent(MARU_EventType type, const MARU_Event* event, uint64_t frame_id) {
    char buf[256];
    buf[0] = '\0';
    if (type == MARU_WINDOW_RESIZED) {
        snprintf(buf, sizeof(buf), "Size: %dx%d", event->resized.geometry.pixel_size.x, event->resized.geometry.pixel_size.y);
    } else if (type == MARU_KEY_STATE_CHANGED) {
        snprintf(buf, sizeof(buf), "Key: %d, State: %d", (int)event->key.raw_key, (int)event->key.state);
    } else if (type == MARU_MOUSE_MOVED) {
        snprintf(buf, sizeof(buf), "Pos: %.1f, %.1f", (double)event->mouse_motion.position.x, (double)event->mouse_motion.position.y);
    } else if (type == MARU_TEXT_INPUT_RECEIVED) {
        snprintf(buf, sizeof(buf), "Text: %s", event->text_input.text);
    }

    event_log.push_back({frame_id, type, buf});
    if (event_log.size() > 500) event_log.erase(event_log.begin());
}

void EventViewer_Render(MARU_Window* window, bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Event Log", p_open)) {
        bool is_fs = maru_isWindowFullscreen(window);
        if (ImGui::Checkbox("Fullscreen", &is_fs)) {
            maru_setWindowFullscreen(window, is_fs);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) event_log.clear();
        
        ImGui::BeginChild("LogScroll");
        for (const auto& entry : event_log) {
            float alpha = (entry.frame % 2) ? 0.1f : 0.2f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, alpha));
            ImGui::BeginChild(std::to_string((uintptr_t)&entry).c_str(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::Text("[%lu] %s %s", entry.frame, event_type_to_string(entry.type), entry.description.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
}
