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
    owns_clipboard_ = false;
    owns_primary_selection_ = false;
    last_status_ = MARU_SUCCESS;
    has_received_target_ = false;
    clipboard_mime_types_.clear();
    primary_mime_types_.clear();

    MARU_WindowAttributes attrs = {0};
    attrs.accept_drop = true;
    maru_updateWindow(window, MARU_WINDOW_ATTR_ACCEPT_DROP, &attrs);
}

void DataExchangeModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
}

static std::string _dnd_info;

void DataExchangeModule::onEvent(MARU_EventId type, MARU_Window* window, const MARU_Event& event) {
    (void)window;

    if (type == MARU_EVENT_DROP_ENTERED) {
        _dnd_info = "Drop Entered";
        if (event.drop_enter.feedback && event.drop_enter.feedback->action) {
            *event.drop_enter.feedback->action = MARU_DROP_ACTION_COPY;
        }
        return;
    }
    if (type == MARU_EVENT_DROP_HOVERED) {
        _dnd_info = "Drop Hovered at " + std::to_string(event.drop_hover.position.x) + ", " + std::to_string(event.drop_hover.position.y);
        if (event.drop_hover.feedback && event.drop_hover.feedback->action) {
            *event.drop_hover.feedback->action = MARU_DROP_ACTION_COPY;
        }
        return;
    }
    if (type == MARU_EVENT_DROP_EXITED) {
        _dnd_info = "Drop Exited";
        return;
    }
    if (type == MARU_EVENT_DROP_DROPPED) {
        _dnd_info = "Drop Dropped";
        return;
    }

    if (type == MARU_EVENT_DATA_REQUESTED) {
        const MARU_DataRequestEvent* req = &event.data_requested;
        if (!req) return;
        if (req->target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
            req->target != MARU_DATA_EXCHANGE_TARGET_PRIMARY) return;
        if (!_is_text_mime(req->mime_type)) return;
        const size_t text_size = strnlen(input_buffer_, sizeof(input_buffer_));
        (void)maru_provideData(req, input_buffer_, text_size, MARU_DATA_PROVIDE_FLAG_NONE);
        return;
    }

    if (type == MARU_EVENT_DATA_RECEIVED) {
        const MARU_DataReceivedEvent* recv = &event.data_received;
        if (!recv || recv->user_tag != this) return;
        last_status_ = recv->status;
        last_received_target_ = recv->target;
        has_received_target_ = true;
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
        ImGui::Text("Status: implicit on context creation");

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
            clipboard_mime_types_.clear();
            MARU_MIMETypeList list = {0};
            last_status_ = maru_getAvailableMIMETypes(
                window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, &list);
            if (last_status_ == MARU_SUCCESS && list.mime_types) {
                for (uint32_t i = 0; i < list.count; ++i) {
                    clipboard_mime_types_.emplace_back(list.mime_types[i] ? list.mime_types[i] : "");
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Announce Primary Selection Text")) {
            last_status_ = maru_announceData(window, MARU_DATA_EXCHANGE_TARGET_PRIMARY,
                                             clipboard_mimes, 2, 0);
            owns_primary_selection_ = (last_status_ == MARU_SUCCESS);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Primary Selection Ownership")) {
            last_status_ = maru_announceData(window, MARU_DATA_EXCHANGE_TARGET_PRIMARY,
                                             NULL, 0, 0);
            owns_primary_selection_ = false;
        }
        ImGui::Text("Primary ownership: %s", owns_primary_selection_ ? "announced" : "none");

        if (ImGui::Button("Request Primary Selection Text")) {
            last_status_ = maru_requestData(window, MARU_DATA_EXCHANGE_TARGET_PRIMARY,
                                            "text/plain;charset=utf-8", this);
            if (last_status_ != MARU_SUCCESS) {
                last_status_ = maru_requestData(window, MARU_DATA_EXCHANGE_TARGET_PRIMARY,
                                                "text/plain", this);
            }
        }

        if (ImGui::Button("Refresh Primary MIME Types")) {
            primary_mime_types_.clear();
            MARU_MIMETypeList list = {0};
            last_status_ = maru_getAvailableMIMETypes(
                window, MARU_DATA_EXCHANGE_TARGET_PRIMARY, &list);
            if (last_status_ == MARU_SUCCESS && list.mime_types) {
                for (uint32_t i = 0; i < list.count; ++i) {
                    primary_mime_types_.emplace_back(list.mime_types[i] ? list.mime_types[i] : "");
                }
            }
        }

        ImGui::Text("Last API status: %s", last_status_ == MARU_SUCCESS ? "MARU_SUCCESS" : "MARU_FAILURE");
        if (has_received_target_) {
            ImGui::Text("Last received target: %s",
                        last_received_target_ == MARU_DATA_EXCHANGE_TARGET_PRIMARY
                            ? "primary"
                            : (last_received_target_ == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD
                                   ? "clipboard"
                                   : "other"));
        } else {
            ImGui::Text("Last received target: none");
        }
        ImGui::Text("Clipboard MIME Types (%zu)", clipboard_mime_types_.size());
        if (ImGui::BeginChild("ClipboardMimeTypes", ImVec2(0, 90), true)) {
            for (const std::string& mime : clipboard_mime_types_) {
                ImGui::BulletText("%s", mime.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Text("Primary MIME Types (%zu)", primary_mime_types_.size());
        if (ImGui::BeginChild("PrimaryMimeTypes", ImVec2(0, 90), true)) {
            for (const std::string& mime : primary_mime_types_) {
                ImGui::BulletText("%s", mime.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Drag and Drop Info:");
        ImGui::Text("%s", _dnd_info.empty() ? "No active DnD" : _dnd_info.c_str());
        
        static MARU_Scalar drop_zone_size = 100.0;
        ImGui::Button("Drop Zone", ImVec2(drop_zone_size, drop_zone_size));
    }
    ImGui::End();
}
