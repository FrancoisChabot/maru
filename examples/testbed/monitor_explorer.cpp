#include "monitor_explorer.h"
#include "imgui.h"

void MonitorExplorer_Render(MARU_Context* context, bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Monitors", p_open)) {
        MARU_MonitorList monitors;
        if (maru_getMonitors(context, &monitors) == MARU_SUCCESS) {
            for (uint32_t i = 0; i < monitors.count; i++) {
                MARU_Monitor* mon = monitors.monitors[i];
                if (ImGui::TreeNode((void*)(intptr_t)i, "Monitor %u: %s%s", i, maru_getMonitorName(mon), maru_isMonitorPrimary(mon) ? " (Primary)" : "")) {
                    ImGui::Text("Scale: %.2f", maru_getMonitorScale(mon));
                    MARU_VideoMode mode = maru_getMonitorCurrentMode(mon);
                    ImGui::Text("Current Mode: %dx%d @ %u Hz", mode.size.x, mode.size.y, mode.refresh_rate_mhz / 1000);
                    MARU_Vec2Dip pos = maru_getMonitorLogicalPosition(mon);
                    MARU_Vec2Dip size = maru_getMonitorLogicalSize(mon);
                    ImGui::Text("Logical Pos: %.1f, %.1f", pos.x, pos.y);
                    ImGui::Text("Logical Size: %.1f, %.1f", size.x, size.y);
                    
                    if (ImGui::TreeNode("Available Modes")) {
                        MARU_VideoModeList modes;
                        if (maru_getMonitorModes(mon, &modes) == MARU_SUCCESS) {
                            for (uint32_t j = 0; j < modes.count; j++) {
                                ImGui::Text("%u: %dx%d @ %u Hz", j, modes.modes[j].size.x, modes.modes[j].size.y, modes.modes[j].refresh_rate_mhz / 1000);
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
}
