#include "monitor_explorer.h"
#include "imgui.h"

void MonitorExplorer_Render(MARU_Context* context, bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Monitors", p_open)) {
        uint32_t monitor_count = 0;
        MARU_Monitor *const *monitors = maru_getMonitors(context, &monitor_count);
        if (monitors) {
            for (uint32_t i = 0; i < monitor_count; i++) {
                MARU_Monitor* mon = monitors[i];
                if (ImGui::TreeNode((void*)(intptr_t)i, "Monitor %u: %s%s", i, maru_getMonitorName(mon), maru_isMonitorPrimary(mon) ? " (Primary)" : "")) {
                    ImGui::Text("Scale: %.2f", (double)maru_getMonitorScale(mon));
                    MARU_VideoMode mode = maru_getMonitorCurrentMode(mon);
                    ImGui::Text("Current Mode: %dx%d @ %u Hz", mode.size.x, mode.size.y, mode.refresh_rate_mhz / 1000);
                    MARU_Vec2Dip pos = maru_getMonitorLogicalPosition(mon);
                    MARU_Vec2Dip size = maru_getMonitorLogicalSize(mon);
                    ImGui::Text("Logical Pos: %.1f, %.1f", (double)pos.x, (double)pos.y);
                    ImGui::Text("Logical Size: %.1f, %.1f", (double)size.x, (double)size.y);
                    
                    if (ImGui::TreeNode("Available Modes")) {
                        uint32_t mode_count = 0;
                        const MARU_VideoMode *modes = maru_getMonitorModes(mon, &mode_count);
                        if (modes) {
                            for (uint32_t j = 0; j < mode_count; j++) {
                                ImGui::Text("%u: %dx%d @ %u Hz", j, modes[j].size.x, modes[j].size.y, modes[j].refresh_rate_mhz / 1000);
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
