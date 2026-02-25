// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "dataexchange_module.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

static bool _is_text_mime(const char* mime_type) {
    if (!mime_type) return false;
    return (strcmp(mime_type, "text/plain") == 0) ||
           (strcmp(mime_type, "text/plain;charset=utf-8") == 0) ||
           (strcmp(mime_type, "text/plain; charset=utf-8") == 0);
}

void DataExchangeModule::onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    dataexchange_enabled_ = false;
    attempted_enable_ = false;
    owns_clipboard_ = false;
    last_status_ = MARU_SUCCESS;
    mime_types_.clear();
}

void DataExchangeModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (!enabled_ || attempted_enable_) return;
    last_status_ = maru_dataexchange_enable(ctx);
    dataexchange_enabled_ = (last_status_ == MARU_SUCCESS);
    attempted_enable_ = true;
}

void DataExchangeModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)window;
    if (type == MARU_EVENT_DATA_REQUESTED) {
        const MARU_DataRequestEvent* req = &event.data_requested;
        if (!req || req->target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) return;
        if (!_is_text_mime(req->mime_type)) return;
        const size_t text_size = strnlen(input_buffer_, sizeof(input_buffer_));
        (void)maru_provideData(req, input_buffer_, text_size, MARU_DATA_PROVIDE_FLAG_NONE);
        return;
    }

    if (type == MARU_EVENT_DATA_RECEIVED) {
        const MARU_DataReceivedEvent* recv = &event.data_received;
        if (!recv || recv->user_tag != this) return;
        last_status_ = recv->status;
        if (recv->status != MARU_SUCCESS || !recv->data) return;

        const size_t copy_size = std::min(recv->size, sizeof(input_buffer_) - 1u);
        if (copy_size > 0) {
            memcpy(input_buffer_, recv->data, copy_size);
        }
        input_buffer_[copy_size] = '\0';
    }
}

void DataExchangeModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    if (ImGui::Begin("Data Exchange", &enabled_)) {
        if (ImGui::Button("Enable Data Exchange")) {
            last_status_ = maru_dataexchange_enable(ctx);
            dataexchange_enabled_ = (last_status_ == MARU_SUCCESS);
            attempted_enable_ = true;
        }
        ImGui::SameLine();
        ImGui::Text("Status: %s", dataexchange_enabled_ ? "enabled" : "disabled");

        ImGui::Separator();
        ImGui::InputTextMultiline("##dataexchange_input", input_buffer_, sizeof(input_buffer_),
                                  ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6));

        const char* clipboard_mimes[] = {
            "text/plain;charset=utf-8",
            "text/plain",
        };

        if (ImGui::Button("Announce Clipboard Text")) {
            last_status_ = maru_announceData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                             clipboard_mimes, 2, 0);
            owns_clipboard_ = (last_status_ == MARU_SUCCESS);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Clipboard Ownership")) {
            last_status_ = maru_announceData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                             NULL, 0, 0);
            owns_clipboard_ = false;
        }
        ImGui::Text("Ownership: %s", owns_clipboard_ ? "announced" : "none");

        if (ImGui::Button("Request Clipboard Text")) {
            last_status_ = maru_requestData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                            "text/plain;charset=utf-8", this);
            if (last_status_ != MARU_SUCCESS) {
                last_status_ = maru_requestData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD,
                                                "text/plain", this);
            }
        }

        if (ImGui::Button("Refresh MIME Types")) {
            mime_types_.clear();
            MARU_MIMETypeList list = {0};
            last_status_ = maru_getAvailableMIMETypes(
                window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, &list);
            if (last_status_ == MARU_SUCCESS && list.mime_types) {
                for (uint32_t i = 0; i < list.count; ++i) {
                    mime_types_.emplace_back(list.mime_types[i] ? list.mime_types[i] : "");
                }
            }
        }

        ImGui::Text("Last API status: %s", last_status_ == MARU_SUCCESS ? "MARU_SUCCESS" : "MARU_FAILURE");
        ImGui::Text("MIME Types (%zu)", mime_types_.size());
        if (ImGui::BeginChild("MimeTypes", ImVec2(0, 120), true)) {
            for (const std::string& mime : mime_types_) {
                ImGui::BulletText("%s", mime.c_str());
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
