// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "timing_module.h"
#include "imgui.h"

#include <algorithm>
#include <array>

void TimingModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    current_frame_id_++;
}

void TimingModule::recordPumpTime(double timestamp_seconds, double pump_duration_ms) {
    if (paused_) {
        return;
    }

    if (samples_.size() < kHistorySize) {
        samples_.resize(kHistorySize);
    }

    samples_[next_sample_index_] = {
        .timestamp_seconds = timestamp_seconds,
        .frame_id = current_frame_id_,
        .pump_duration_ms = pump_duration_ms,
    };
    next_sample_index_ = (next_sample_index_ + 1) % kHistorySize;
    if (next_sample_index_ == 0) {
        samples_full_ = true;
    }

    if (auto_pause_threshold_ms_ > 0.0f &&
        pump_duration_ms >= static_cast<double>(auto_pause_threshold_ms_)) {
        paused_ = true;
    }
}

void TimingModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    if (!enabled_) return;

    ImGui::SetNextWindowSize(ImVec2(720, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Timing", &enabled_)) {
        ImGui::End();
        return;
    }

    if (paused_) {
        if (ImGui::Button("Resume")) {
            paused_ = false;
        }
    } else {
        if (ImGui::Button("Pause")) {
            paused_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        clearSamples();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputFloat("Auto-pause Threshold (ms)", &auto_pause_threshold_ms_, 0.5f, 1.0f, "%.2f");
    if (auto_pause_threshold_ms_ < 0.0f) {
        auto_pause_threshold_ms_ = 0.0f;
    }

    const size_t sample_count = samples_full_ ? kHistorySize : next_sample_index_;
    double latest_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double avg_ms = 0.0;

    if (sample_count > 0) {
        const size_t start = samples_full_ ? next_sample_index_ : 0;
        latest_ms = samples_[(start + sample_count - 1) % kHistorySize].pump_duration_ms;
        min_ms = latest_ms;
        max_ms = latest_ms;
        for (size_t i = 0; i < sample_count; ++i) {
            const double ms = samples_[(start + i) % kHistorySize].pump_duration_ms;
            min_ms = std::min(min_ms, ms);
            max_ms = std::max(max_ms, ms);
            avg_ms += ms;
        }
        avg_ms /= static_cast<double>(sample_count);
    }

    ImGui::Text("Recording: %s", paused_ ? "Paused" : "Running");
    ImGui::Text("Samples: %zu / %zu", sample_count, kHistorySize);
    if (sample_count > 0) {
        ImGui::Text("Latest %.3f ms  Min %.3f ms  Avg %.3f ms  Max %.3f ms",
                    latest_ms, min_ms, avg_ms, max_ms);
    } else {
        ImGui::TextUnformatted("No pump timing samples recorded yet.");
    }

    if (sample_count > 0) {
        std::array<float, kHistorySize> plot_values = {};
        const size_t start = samples_full_ ? next_sample_index_ : 0;
        for (size_t i = 0; i < sample_count; ++i) {
            plot_values[i] =
                static_cast<float>(samples_[(start + i) % kHistorySize].pump_duration_ms);
        }
        ImGui::PlotLines("maru_pumpEvents() ms",
                         plot_values.data(),
                         static_cast<int>(sample_count),
                         0,
                         nullptr,
                         0.0f,
                         (max_ms > 0.0) ? static_cast<float>(max_ms * 1.1) : 1.0f,
                         ImVec2(0.0f, 120.0f));
    }

    ImGui::Separator();

    if (ImGui::BeginTable("TimingHistoryTable",
                          3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Pump (ms)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        if (sample_count > 0) {
            const size_t start = samples_full_ ? next_sample_index_ : 0;
            for (size_t i = 0; i < sample_count; ++i) {
                const PumpSample& sample = samples_[(start + i) % kHistorySize];
                ImGui::TableNextRow();

                ImU32 row_bg_color =
                    (sample.frame_id % 2 == 0) ? IM_COL32(50, 50, 60, 255)
                                               : IM_COL32(40, 40, 50, 255);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_bg_color);

                ImGui::TableNextColumn();
                ImGui::Text("%.3f", sample.timestamp_seconds);
                ImGui::TableNextColumn();
                ImGui::Text("0x%06X", sample.frame_id);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", sample.pump_duration_ms);
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void TimingModule::clearSamples() {
    samples_.clear();
    next_sample_index_ = 0;
    samples_full_ = false;
}
