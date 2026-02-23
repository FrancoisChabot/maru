// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "instrumentation_module.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

void InstrumentationModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void InstrumentationModule::onDiagnostic(const MARU_DiagnosticInfo* info) {
    LogEntry entry;
    entry.timestamp = ImGui::GetTime();
    entry.diagnostic = info->diagnostic;
    entry.message = info->message ? info->message : "";
    entry.context = info->context;
    entry.window = info->window;
    logs_.push_back(std::move(entry));
    
    if (logs_.size() > 1000) {
        logs_.erase(logs_.begin());
    }
}

void InstrumentationModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
    if (!enabled_) return;

    if (ImGui::Begin("Instrumentation", &enabled_)) {
        if (ImGui::Button("Clear")) {
            logs_.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll_);

        ImGui::Separator();

        if (ImGui::BeginTable("InstrumentationTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Window", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& entry : logs_) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", entry.timestamp);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(diagnosticToString(entry.diagnostic));
                ImGui::TableNextColumn();
                if (entry.window) ImGui::Text("%p", (void*)entry.window); else ImGui::Text("N/A");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.message.c_str());
            }

            if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

const char* InstrumentationModule::diagnosticToString(MARU_Diagnostic d) {
    switch (d) {
        case MARU_DIAGNOSTIC_NONE: return "NONE";
        case MARU_DIAGNOSTIC_INFO: return "INFO";
        case MARU_DIAGNOSTIC_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
        case MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE: return "DYNAMIC_LIB_FAILURE";
        case MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED: return "FEATURE_UNSUPPORTED";
        case MARU_DIAGNOSTIC_BACKEND_FAILURE: return "BACKEND_FAILURE";
        case MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE: return "BACKEND_UNAVAILABLE";
        case MARU_DIAGNOSTIC_VULKAN_FAILURE: return "VULKAN_FAILURE";
        case MARU_DIAGNOSTIC_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case MARU_DIAGNOSTIC_PRECONDITION_FAILURE: return "PRECONDITION_FAILURE";
        case MARU_DIAGNOSTIC_INTERNAL: return "INTERNAL";
        case MARU_DIAGNOSTIC_UNKNOWN:
        default: return "UNKNOWN";
    }
}
