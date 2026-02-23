#include "event_log.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>

EventLog::EventLog() {
    logs.resize(max_logs);
}

void EventLog::onFrameBegin() {
    current_frame_id++;
}

static std::string formatModifiers(MARU_ModifierFlags mods) {
    std::string s;
    if (mods & MARU_MODIFIER_SHIFT) s += "Shift ";
    if (mods & MARU_MODIFIER_CONTROL) s += "Ctrl ";
    if (mods & MARU_MODIFIER_ALT) s += "Alt ";
    if (mods & MARU_MODIFIER_META) s += "Super ";
    if (mods & MARU_MODIFIER_CAPS_LOCK) s += "Caps ";
    if (mods & MARU_MODIFIER_NUM_LOCK) s += "Num ";
    return s;
}

void EventLog::onEvent(MARU_EventType type, MARU_Window* win, const MARU_Event& e) {
    if (!enabled) return;
    
    // Skip noisy frame events
    if (type == MARU_WINDOW_FRAME) return;

    std::stringstream ss;
    if (type == MARU_WINDOW_RESIZED) {
        ss << "Size: " << e.resized.geometry.logical_size.x << "x" << e.resized.geometry.logical_size.y 
           << " (Pix: " << e.resized.geometry.pixel_size.x << "x" << e.resized.geometry.pixel_size.y << ")";
    } else if (type == MARU_KEY_STATE_CHANGED) {
        ss << "Key: " << (int)e.key.raw_key << " State: " << (e.key.state == MARU_BUTTON_STATE_PRESSED ? "PR" : "RE")
           << " Mods: " << formatModifiers(e.key.modifiers);
    } else if (type == MARU_MOUSE_MOVED) {
        ss << "Pos: (" << e.mouse_motion.position.x << "," << e.mouse_motion.position.y << ")"
           << " Delta: (" << e.mouse_motion.delta.x << "," << e.mouse_motion.delta.y << ")"
           << " Raw: (" << e.mouse_motion.raw_delta.x << "," << e.mouse_motion.raw_delta.y << ")";
    } else if (type == MARU_MOUSE_BUTTON_STATE_CHANGED) {
        ss << "Button: " << (int)e.mouse_button.button << " State: " << (e.mouse_button.state == MARU_BUTTON_STATE_PRESSED ? "PR" : "RE")
           << " Mods: " << formatModifiers(e.mouse_button.modifiers);
    } else if (type == MARU_MOUSE_SCROLLED) {
        ss << "Delta: (" << e.mouse_scroll.delta.x << "," << e.mouse_scroll.delta.y << ")";
    } else if (type == MARU_TEXT_INPUT_RECEIVED) {
        ss << "Text: '" << (e.text_input.text ? e.text_input.text : "") << "' Len: " << e.text_input.length;
    } else if (type == MARU_TEXT_COMPOSITION_UPDATED) {
        ss << "Preedit: '" << (e.text_composition.text ? e.text_composition.text : "") << "' Cursor: " << e.text_composition.cursor_index;
    } else if (type == MARU_FOCUS_CHANGED) {
        ss << "Focused: " << (e.focus.focused ? "YES" : "NO");
    } else if (type == MARU_WINDOW_MAXIMIZED) {
        ss << "Maximized: " << (e.maximized.maximized ? "YES" : "NO");
    } else if (type == MARU_MONITOR_CONNECTION_CHANGED) {
        ss << "Monitor: " << (void*)e.monitor_connection.monitor << " Connected: " << (e.monitor_connection.connected ? "YES" : "NO");
    } else {
        ss << "No detailed payload parser";
    }

    LogEntry entry;
    entry.timestamp = ImGui::GetTime();
    entry.frame_id = current_frame_id;
    entry.type = type;
    entry.details = ss.str();
    entry.window_handle = win;
    
    if (win) {
        auto it = window_titles.find(win);
        if (it != window_titles.end()) {
            entry.window_title = it->second;
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "Wnd:%p", (void*)win);
            entry.window_title = buf;
        }
    } else {
        entry.window_title = "Global";
    }

    logs[next_index] = std::move(entry);
    next_index = (next_index + 1) % max_logs;
    if (next_index == 0) full = true;
}

void EventLog::render() {
    if (!enabled) return;

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Event Log", &enabled)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) {
        logs.clear();
        logs.resize(max_logs);
        next_index = 0;
        full = false;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll);
    ImGui::SameLine();
    ImGui::Checkbox("Details", &show_details);

    ImGui::Separator();

    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("EventLogTable", show_details ? 5 : 4, flags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Window", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        if (show_details) ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
        
        ImGui::TableSetupScrollFreeze(0, 1); // Freeze the header row
        ImGui::TableHeadersRow();

        size_t count = full ? max_logs : next_index;
        size_t start = full ? next_index : 0;

        for (size_t i = 0; i < count; ++i) {
            const auto& entry = logs[(start + i) % max_logs];
            ImGui::TableNextRow();

            // Alternating colors based on frame ID
            ImU32 row_bg_color = (entry.frame_id % 2 == 0) ? IM_COL32(50, 50, 60, 255) : IM_COL32(40, 40, 50, 255);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_bg_color);

            ImGui::TableNextColumn();
            ImGui::Text("%.3f", entry.timestamp);
            ImGui::TableNextColumn();
            ImGui::Text("0x%06X", entry.frame_id);
            ImGui::TableNextColumn();
            ImGui::Text("%s", typeToString(entry.type));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.window_title.c_str());
            if (show_details) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.details.c_str());
            }
        }

        if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::End();
}

const char* EventLog::typeToString(MARU_EventType type) {
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
    if (type == MARU_WINDOW_FRAME) return "WINDOW_FRAME";
    if (type == MARU_TEXT_INPUT_RECEIVED) return "TEXT_INPUT_RECEIVED";
    if (type == MARU_FOCUS_CHANGED) return "FOCUS_CHANGED";
    if (type == MARU_WINDOW_MAXIMIZED) return "WINDOW_MAXIMIZED";
    if (type == MARU_TEXT_COMPOSITION_UPDATED) return "TEXT_COMPOSITION_UPDATED";
    if (type == MARU_USER_EVENT_0) return "USER_EVENT_0";
    return "UNKNOWN";
}
