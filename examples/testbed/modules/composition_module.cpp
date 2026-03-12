// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "composition_module.h"
#include "imgui.h"
#include <cstring>

void CompositionModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void CompositionModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)window;
    
    if (type == MARU_EVENT_TEXT_EDIT_STARTED) {
        current_session_.id = event.text_edit_started.session_id;
        current_session_.active = true;
        current_session_.preedit = "";
    } else if (type == MARU_EVENT_TEXT_EDIT_UPDATED) {
        if (current_session_.active && current_session_.id == event.text_edit_updated.session_id) {
            current_session_.preedit = event.text_edit_updated.preedit_utf8 ? event.text_edit_updated.preedit_utf8 : "";
            current_session_.caret = event.text_edit_updated.caret.start_byte;
            current_session_.sel_start = event.text_edit_updated.selection.start_byte;
            current_session_.sel_len = event.text_edit_updated.selection.length_byte;
        }
    } else if (type == MARU_EVENT_TEXT_EDIT_COMMITTED) {
        if (event.text_edit_committed.committed_length > 0 &&
            event.text_edit_committed.committed_utf8) {
            commit_history_.push_back(std::string(
                event.text_edit_committed.committed_utf8,
                event.text_edit_committed.committed_length));
            if (commit_history_.size() > 20) commit_history_.erase(commit_history_.begin());
        }
        
        // When a commit happens, the preedit is typically cleared by the next update or implicitly
        if (current_session_.active && current_session_.id == event.text_edit_committed.session_id) {
             current_session_.preedit = "";
        }
    } else if (type == MARU_EVENT_TEXT_EDIT_ENDED) {
        if (current_session_.active && current_session_.id == event.text_edit_ended.session_id) {
            current_session_.active = false;
        }
    }
}

void CompositionModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Composition Test", &enabled_)) {
        
        ImGui::TextWrapped("This module tests IME composition. Click the box below and use your IME (e.g., Fcitx5, Emoji picker, etc.)");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Standard ImGui Multiline Input:");
        ImGui::InputTextMultiline("##multiline", multiline_buffer_, IM_ARRAYSIZE(multiline_buffer_), ImVec2(-FLT_MIN, 150));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Raw Maru IME State:");
        
        if (current_session_.active) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "ACTIVE SESSION: %llu", (unsigned long long)current_session_.id);
            ImGui::Text("Preedit: \"%s\"", current_session_.preedit.c_str());
            ImGui::Text("Caret: %u", current_session_.caret);
            ImGui::Text("Selection: [%u, %u]", current_session_.sel_start, current_session_.sel_len);
            
            // Visual representation of preedit
            ImGui::Text("Visual: [");
            ImGui::SameLine(0, 0);
            for (size_t i = 0; i <= current_session_.preedit.length(); ++i) {
                if (i == current_session_.caret) {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "|"); 
                    ImGui::SameLine(0, 0);
                }
                if (i < current_session_.preedit.length()) {
                    char c[2] = {current_session_.preedit[i], 0};
                    bool in_sel = (i >= current_session_.sel_start && i < current_session_.sel_start + current_session_.sel_len);
                    if (in_sel) ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", c);
                    else ImGui::Text("%s", c);
                    ImGui::SameLine(0, 0);
                }
            }
            ImGui::Text("]");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No active composition session.");
        }
        
        ImGui::Spacing();
        ImGui::Text("Recent Commits:");
        ImGui::BeginChild("Commits", ImVec2(0, 150), true);
        for (const auto& commit : commit_history_) {
            ImGui::Text("\"%s\"", commit.c_str());
        }
        ImGui::EndChild();
        
        if (ImGui::Button("Clear History")) {
            commit_history_.clear();
        }
    }
    ImGui::End();
}
