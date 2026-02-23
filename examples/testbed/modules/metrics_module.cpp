// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "metrics_module.h"
#include "imgui.h"
#include <cstdio>

void MetricsModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void MetricsModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (!enabled_) return;

    if (ImGui::Begin("Metrics", &enabled_)) {
        const MARU_ContextMetrics* metrics = maru_getContextMetrics(ctx);
        if (metrics) {
            if (ImGui::CollapsingHeader("User Events", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (metrics->user_events) {
                    ImGui::Text("Peak Count: %u", metrics->user_events->peak_count);
                    ImGui::Text("Current Capacity: %u", metrics->user_events->current_capacity);
                    ImGui::Text("Drop Count: %u", metrics->user_events->drop_count);
                } else {
                    ImGui::Text("User event metrics not available.");
                }
            }
        } else {
            ImGui::Text("Context metrics not available.");
        }

        if (ImGui::Button("Reset Metrics")) {
            maru_resetContextMetrics(ctx);
        }
    }
    ImGui::End();
}
