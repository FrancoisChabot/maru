// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "monitor_module.h"
#include "imgui.h"
#include <cstdio>
#include <vector>

void MonitorModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void MonitorModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (!enabled_) return;

    if (ImGui::Begin("Monitors", &enabled_)) {
        uint32_t monitor_count = 0;
        MARU_Monitor* const* monitors = maru_getMonitors(ctx, &monitor_count);
        
        if (monitor_count == 0) {
            ImGui::Text("No monitors detected.");
        } else {
            for (uint32_t i = 0; i < monitor_count; ++i) {
                MARU_Monitor* monitor = monitors[i];
                const char* name = maru_getMonitorName(monitor);
                bool is_primary = maru_isMonitorPrimary(monitor);
                
                char header_label[128];
                snprintf(header_label, sizeof(header_label), "%s%s###Monitor%u", 
                         name ? name : "Unknown Monitor", 
                         is_primary ? " [Primary]" : "", 
                         i);
                
                if (ImGui::CollapsingHeader(header_label)) {
                    MARU_Vec2Dip pos = maru_getMonitorLogicalPosition(monitor);
                    MARU_Vec2Dip size = maru_getMonitorLogicalSize(monitor);
                    MARU_Scalar scale = maru_getMonitorScale(monitor);
                    MARU_Vec2Dip phys_size = maru_getMonitorPhysicalSize(monitor);
                    
                    ImGui::Text("Handle: %p", (void*)monitor);
                    ImGui::Text("Logical Position: (%.1f, %.1f)", pos.x, pos.y);
                    ImGui::Text("Logical Size: (%.1f, %.1f)", size.x, size.y);
                    ImGui::Text("Physical Size: (%.1f, %.1f) mm", phys_size.x, phys_size.y);
                    ImGui::Text("Scale: %.2f", scale);
                    
                    MARU_VideoMode current_mode = maru_getMonitorCurrentMode(monitor);
                    ImGui::Text("Current Mode: %dx%d @ %.2f Hz", 
                                current_mode.size.x, current_mode.size.y, 
                                (float)current_mode.refresh_rate_mhz / 1000.0f);
                    
                    if (ImGui::TreeNode("Supported Modes")) {
                        uint32_t mode_count = 0;
                        const MARU_VideoMode* modes = maru_getMonitorModes(monitor, &mode_count);
                        
                        if (ImGui::BeginTable("ModesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("Size");
                            ImGui::TableSetupColumn("Refresh Rate");
                            ImGui::TableSetupColumn("Action");
                            ImGui::TableHeadersRow();
                            
                            for (uint32_t j = 0; j < mode_count; ++j) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%dx%d", modes[j].size.x, modes[j].size.y);
                                ImGui::TableNextColumn();
                                ImGui::Text("%.2f Hz", (float)modes[j].refresh_rate_mhz / 1000.0f);
                                ImGui::TableNextColumn();
                                
                                char btn_label[32];
                                snprintf(btn_label, sizeof(btn_label), "Set###Mode%u_%u", i, j);
                                if (ImGui::Button(btn_label)) {
                                    maru_setMonitorMode(monitor, modes[j]);
                                }
                            }
                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                }
            }
        }
    }
    ImGui::End();
}
