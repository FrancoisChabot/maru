// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "event_log_module.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstring>

EventLogModule::EventLogModule() {
    logs_.resize(max_logs_);
}

void EventLogModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
    current_frame_id_++;
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

void EventLogModule::onEvent(MARU_EventId type, MARU_Window* win, const MARU_Event& e) {
    if (!enabled_) return;
    if (hide_mouse_moved_ && type == MARU_EVENT_MOUSE_MOVED) return;
    
    std::stringstream ss;
    if (type == MARU_EVENT_WINDOW_RESIZED) {
        ss << "Size: " << e.window_resized.geometry.dip_size.x << "x" << e.window_resized.geometry.dip_size.y 
           << " (Pix: " << e.window_resized.geometry.px_size.x << "x" << e.window_resized.geometry.px_size.y << ")";
    } else if (type == MARU_EVENT_KEY_CHANGED) {
        ss << "Key: " << (int)e.key_changed.raw_key << " State: " << (e.key_changed.state == MARU_BUTTON_STATE_PRESSED ? "PR" : "RE")
           << " Mods: " << formatModifiers(e.key_changed.modifiers);
    } else if (type == MARU_EVENT_MOUSE_MOVED) {
        ss << "Pos: (" << e.mouse_moved.dip_position.x << "," << e.mouse_moved.dip_position.y << ")"
           << " Delta: (" << e.mouse_moved.dip_delta.x << "," << e.mouse_moved.dip_delta.y << ")"
           << " Raw: (" << e.mouse_moved.raw_dip_delta.x << "," << e.mouse_moved.raw_dip_delta.y << ")";
    } else if (type == MARU_EVENT_MOUSE_BUTTON_CHANGED) {
        ss << "Button: " << (int)e.mouse_button_changed.button_id << " State: " << (e.mouse_button_changed.state == MARU_BUTTON_STATE_PRESSED ? "PR" : "RE")
           << " Mods: " << formatModifiers(e.mouse_button_changed.modifiers);
    } else if (type == MARU_EVENT_MOUSE_SCROLLED) {
        ss << "Delta: (" << e.mouse_scrolled.dip_delta.x << "," << e.mouse_scrolled.dip_delta.y << ")" << "Steps: (" << e.mouse_scrolled.steps.x << "," << e.mouse_scrolled.steps.y << ")";
    } else if (type == MARU_EVENT_IDLE_CHANGED) {
        ss << "Idle: " << (e.idle_changed.is_idle ? "YES" : "NO") << " Timeout: " << e.idle_changed.timeout_ms << "ms";
    } else if (type == MARU_EVENT_TEXT_EDIT_STARTED) {
        ss << "IME Start: Session=" << e.text_edit_started.session_id;
    } else if (type == MARU_EVENT_TEXT_EDIT_UPDATED) {
        ss << "IME Update: Session=" << e.text_edit_updated.session_id 
           << " Preedit: '" << (e.text_edit_updated.preedit_utf8 ? e.text_edit_updated.preedit_utf8 : "") << "'"
           << " Caret: " << e.text_edit_updated.caret.start_byte 
           << " Sel: [" << e.text_edit_updated.selection.start_byte << ", " << e.text_edit_updated.selection.length_byte << "]";
    } else if (type == MARU_EVENT_TEXT_EDIT_COMMITTED) {
        const std::string committed =
            (e.text_edit_committed.committed_utf8 &&
             e.text_edit_committed.committed_length_bytes > 0)
                ? std::string(e.text_edit_committed.committed_utf8,
                              e.text_edit_committed.committed_length_bytes)
                : std::string();
        ss << "IME Commit: Session=" << e.text_edit_committed.session_id
           << " Del: (" << e.text_edit_committed.delete_before_bytes << ", " << e.text_edit_committed.delete_after_bytes << ")"
           << " Text: '" << committed << "'";
    } else if (type == MARU_EVENT_TEXT_EDIT_ENDED) {
        ss << "IME End: Session=" << e.text_edit_ended.session_id << " Canceled: " << (e.text_edit_ended.canceled ? "YES" : "NO");
    } else if (type == MARU_EVENT_WINDOW_STATE_CHANGED) {
        ss << "ChangedMask=0x" << std::hex << e.window_state_changed.changed_fields << std::dec
           << " Visible: " << (e.window_state_changed.visible ? "YES" : "NO")
           << " Minimized: " << (e.window_state_changed.minimized ? "YES" : "NO")
           << " Maximized: " << (e.window_state_changed.maximized ? "YES" : "NO")
           << " Focused: " << (e.window_state_changed.focused ? "YES" : "NO")
           << " Fullscreen: " << (e.window_state_changed.fullscreen ? "YES" : "NO")
           << " Resizable: " << (e.window_state_changed.resizable ? "YES" : "NO")
           << " Decorated: " << (e.window_state_changed.decorated ? "YES" : "NO")
           << " Icon: " << (e.window_state_changed.icon ? "SET" : "NONE")
           << " IconChanged: "
           << ((e.window_state_changed.changed_fields & MARU_WINDOW_STATE_CHANGED_ICON) ? "YES" : "NO");
    } else if (type == MARU_EVENT_MONITOR_CHANGED) {
        ss << "Monitor: " << (void*)e.monitor_changed.monitor << " Connected: " << (e.monitor_changed.connected ? "YES" : "NO");
    } else if (type == MARU_EVENT_DROP_ENTERED) {
        ss << "Drop Enter: (" << e.drop_entered.dip_position.x << "," << e.drop_entered.dip_position.y << ") Count=" << e.drop_entered.available_types.count;
    } else if (type == MARU_EVENT_DROP_HOVERED) {
        ss << "Drop Hover: (" << e.drop_hovered.dip_position.x << "," << e.drop_hovered.dip_position.y << ") Count=" << e.drop_hovered.available_types.count;
    } else if (type == MARU_EVENT_DROP_EXITED) {
        ss << "Drop Exit";
    } else if (type == MARU_EVENT_DROP_DROPPED) {
        ss << "Drop Dropped: (" << e.drop_dropped.dip_position.x << "," << e.drop_dropped.dip_position.y << ") Count=" << e.drop_dropped.available_types.count;
    } else if (type == MARU_EVENT_DATA_RECEIVED) {
        ss << "Data Received: Status=" << (int)e.data_received.status << " Mime=" << (e.data_received.mime_type ? e.data_received.mime_type : "N/A") << " Size=" << e.data_received.size;
    } else if (type == MARU_EVENT_DATA_REQUESTED) {
        ss << "Data Requested: Mime=" << (e.data_requested.mime_type ? e.data_requested.mime_type : "N/A");
    } else if (type == MARU_EVENT_DATA_CONSUMED) {
        ss << "Data Consumed: Mime=" << (e.data_consumed.mime_type ? e.data_consumed.mime_type : "N/A") << " Size=" << e.data_consumed.size;
    } else if (type == MARU_EVENT_DRAG_FINISHED) {
        ss << "Drag Finished: Action=" << (int)e.drag_finished.action;
    } else if (type == MARU_EVENT_CONTROLLER_CHANGED) {
        ss << "Controller: " << (void*)e.controller_changed.controller << " Connected: " << (e.controller_changed.connected ? "YES" : "NO");
    } else if (type == MARU_EVENT_CONTROLLER_BUTTON_CHANGED) {
        ss << "Controller: " << (void*)e.controller_button_changed.controller << " Button=" << e.controller_button_changed.button_id << " State=" << (e.controller_button_changed.state == MARU_BUTTON_STATE_PRESSED ? "PR" : "RE");
    } else {
        ss << "No detailed payload parser";
    }

    LogEntry entry;
    entry.timestamp = ImGui::GetTime();
    entry.frame_id = current_frame_id_;
    entry.type = type;
    entry.details = ss.str();
    entry.window_handle = win;
    
    if (win) {
        auto it = window_titles_.find(win);
        if (it != window_titles_.end()) {
            entry.window_title = it->second;
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "Wnd:%p", (void*)win);
            entry.window_title = buf;
        }
    } else {
        entry.window_title = "Global";
    }

    logs_[next_index_] = std::move(entry);
    next_index_ = (next_index_ + 1) % max_logs_;
    if (next_index_ == 0) full_ = true;
}

void EventLogModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
    if (!enabled_) return;

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Event Log", &enabled_)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) {
        logs_.clear();
        logs_.resize(max_logs_);
        next_index_ = 0;
        full_ = false;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    ImGui::Checkbox("Details", &show_details_);
    ImGui::SameLine();
    ImGui::Checkbox("Frame Events", &log_frame_events_);
    ImGui::SameLine();
    ImGui::Checkbox("Hide Mouse Moved", &hide_mouse_moved_);

    ImGui::Separator();

    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("EventLogTable", show_details_ ? 5 : 4, flags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Window", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        if (show_details_) ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
        
        ImGui::TableSetupScrollFreeze(0, 1); // Freeze the header row
        ImGui::TableHeadersRow();

        size_t count = full_ ? max_logs_ : next_index_;
        size_t start = full_ ? next_index_ : 0;

        for (size_t i = 0; i < count; ++i) {
            const auto& entry = logs_[(start + i) % max_logs_];
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
            if (show_details_) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.details.c_str());
            }
        }

        if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::End();
}

const char* EventLogModule::typeToString(MARU_EventId type) {
    if (type == MARU_EVENT_CLOSE_REQUESTED) return "CLOSE_REQUESTED";
    if (type == MARU_EVENT_WINDOW_RESIZED) return "WINDOW_RESIZED";
    if (type == MARU_EVENT_KEY_CHANGED) return "KEY_CHANGED";
    if (type == MARU_EVENT_WINDOW_READY) return "WINDOW_READY";
    if (type == MARU_EVENT_MOUSE_MOVED) return "MOUSE_MOVED";
    if (type == MARU_EVENT_MOUSE_BUTTON_CHANGED) return "MOUSE_BUTTON_STATE_CHANGED";
    if (type == MARU_EVENT_MOUSE_SCROLLED) return "MOUSE_SCROLLED";
    if (type == MARU_EVENT_IDLE_CHANGED) return "IDLE_STATE_CHANGED";
    if (type == MARU_EVENT_MONITOR_CHANGED) return "MONITOR_CHANGED";
    if (type == MARU_EVENT_MONITOR_MODE_CHANGED) return "MONITOR_MODE_CHANGED";
    if (type == MARU_EVENT_WINDOW_FRAME) return "WINDOW_FRAME";
    if (type == MARU_EVENT_WINDOW_STATE_CHANGED) return "WINDOW_STATE_CHANGED";
    if (type == MARU_EVENT_TEXT_EDIT_STARTED) return "TEXT_EDIT_STARTED";
    if (type == MARU_EVENT_TEXT_EDIT_UPDATED) return "TEXT_EDIT_UPDATED";
    if (type == MARU_EVENT_TEXT_EDIT_COMMITTED) return "TEXT_EDIT_COMMITTED";
    if (type == MARU_EVENT_TEXT_EDIT_ENDED) return "TEXT_EDIT_ENDED";
    if (type == MARU_EVENT_DROP_ENTERED) return "DROP_ENTERED";
    if (type == MARU_EVENT_DROP_HOVERED) return "DROP_HOVERED";
    if (type == MARU_EVENT_DROP_EXITED) return "DROP_EXITED";
    if (type == MARU_EVENT_DROP_DROPPED) return "DROP_DROPPED";
    if (type == MARU_EVENT_DATA_RECEIVED) return "DATA_RECEIVED";
    if (type == MARU_EVENT_DATA_REQUESTED) return "DATA_REQUESTED";
    if (type == MARU_EVENT_DATA_CONSUMED) return "DATA_CONSUMED";
    if (type == MARU_EVENT_DRAG_FINISHED) return "DRAG_FINISHED";
    if (type == MARU_EVENT_CONTROLLER_CHANGED) return "CONTROLLER_CHANGED";
    if (type == MARU_EVENT_CONTROLLER_BUTTON_CHANGED) return "CONTROLLER_BUTTON_CHANGED";
    if (type == MARU_EVENT_USER_0) return "USER_EVENT_0";
    return "UNKNOWN";
}
