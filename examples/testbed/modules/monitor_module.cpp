// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "monitor_module.h"
#include "imgui.h"
#include <cstdio>

void MonitorModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)window;
    (void)event;
    if (!auto_refresh_on_events_) {
        return;
    }
    if (type == MARU_EVENT_MONITOR_CONNECTION_CHANGED || type == MARU_EVENT_MONITOR_MODE_CHANGED) {
        refresh_requested_ = true;
    }
}

void MonitorModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
}

void MonitorModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (!enabled_) return;

    if (!ImGui::Begin("Monitors", &enabled_)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh")) {
        refresh_requested_ = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-refresh on monitor events", &auto_refresh_on_events_);

    if (has_mode_set_result_) {
        ImGui::SameLine();
        ImGui::Text("Last set mode result: %s", last_mode_set_status_ == MARU_SUCCESS ? "SUCCESS" : "FAILURE");
    }

    if (refresh_requested_) {
        refresh_requested_ = false;
    }

    MARU_MonitorList monitor_list = {};
    MARU_Status monitor_status = maru_getMonitors(ctx, &monitor_list);
    const uint32_t monitor_count = monitor_list.count;
    MARU_Monitor* const* monitors = monitor_list.monitors;

    ImGui::Separator();
    if (monitor_status != MARU_SUCCESS) {
        ImGui::Text("Detected monitors: unavailable (status=%d)", (int)monitor_status);
        ImGui::End();
        return;
    }
    ImGui::Text("Detected monitors: %u", monitor_count);

    if (monitor_count == 0 || !monitors) {
        ImGui::Text("No monitors detected.");
        ImGui::End();
        return;
    }

    for (uint32_t i = 0; i < monitor_count; ++i) {
        MARU_Monitor* monitor = monitors[i];
        const char* name = maru_getMonitorName(monitor);
        const bool is_primary = maru_isMonitorPrimary(monitor);
        const bool is_lost = maru_isMonitorLost(monitor);

        char header_label[192];
        snprintf(header_label, sizeof(header_label), "%s%s%s###Monitor%u",
                 name ? name : "Unknown Monitor",
                 is_primary ? " [Primary]" : "",
                 is_lost ? " [Lost]" : "",
                 i);

        if (!ImGui::CollapsingHeader(header_label, ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        const MARU_Vec2Dip pos = maru_getMonitorLogicalPosition(monitor);
        const MARU_Vec2Dip size = maru_getMonitorLogicalSize(monitor);
        const MARU_Scalar scale = maru_getMonitorScale(monitor);
        const MARU_Vec2Mm phys_size = maru_getMonitorPhysicalSize(monitor);

        ImGui::Text("Handle: %p", (void*)monitor);
        ImGui::Text("Logical Position: (%.1f, %.1f)", pos.x, pos.y);
        ImGui::Text("Logical Size: (%.1f, %.1f)", size.x, size.y);
        ImGui::Text("Physical Size: (%.1f, %.1f) mm", phys_size.x, phys_size.y);
        ImGui::Text("Scale: %.2f", scale);

        const MARU_VideoMode current_mode = maru_getMonitorCurrentMode(monitor);
        ImGui::Text("Current Mode: %dx%d @ %.2f Hz", current_mode.size.x,
                    current_mode.size.y,
                    (double)current_mode.refresh_rate_millihz / 1000.0);

        MARU_VideoModeList modes = {};
        maru_getMonitorModes(monitor, &modes);

        if (ImGui::TreeNode("Supported Modes")) {
            if (modes.count == 0 || !modes.modes) {
                ImGui::Text("No modes reported by backend.");
            } else if (ImGui::BeginTable("ModesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Refresh Rate");
                ImGui::TableSetupColumn("Action");
                ImGui::TableHeadersRow();

                for (uint32_t j = 0; j < modes.count; ++j) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%dx%d", modes.modes[j].size.x, modes.modes[j].size.y);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f Hz", (double)modes.modes[j].refresh_rate_millihz / 1000.0);
                    ImGui::TableNextColumn();

                    char btn_label[48];
                    snprintf(btn_label, sizeof(btn_label), "Set###Mode%u_%u", i, j);
                    if (ImGui::Button(btn_label)) {
                        last_mode_set_status_ = maru_setMonitorMode(monitor, modes.modes[j]);
                        has_mode_set_result_ = true;
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
