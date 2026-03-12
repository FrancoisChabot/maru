// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "event_module.h"
#include "imgui.h"
#include "maru/maru.h"
#include <cstdio>

void EventModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void EventModule::onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
    context_mask_ = MARU_ALL_EVENTS;
}

void EventModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;
    (void)window;

    if (ImGui::Begin("Event Mask", &enabled_)) {
        MARU_EventMask mask = context_mask_;
        
        auto event_checkbox = [&](const char* name, MARU_EventMask bit) {
            bool val = (mask & bit) != 0;
            if (ImGui::Checkbox(name, &val)) {
                if (val) mask |= bit; else mask &= ~bit;
                context_mask_ = mask;
            }
        };

        event_checkbox("CLOSE_REQUESTED", MARU_MASK_CLOSE_REQUESTED);
        event_checkbox("WINDOW_RESIZED", MARU_MASK_WINDOW_RESIZED);
        event_checkbox("KEY_CHANGED", MARU_MASK_KEY_CHANGED);
        event_checkbox("WINDOW_READY", MARU_MASK_WINDOW_READY);
        event_checkbox("MOUSE_MOVED", MARU_MASK_MOUSE_MOVED);
        event_checkbox("MOUSE_BUTTON_STATE_CHANGED", MARU_MASK_MOUSE_BUTTON_CHANGED);
        event_checkbox("MOUSE_SCROLLED", MARU_MASK_MOUSE_SCROLLED);
        event_checkbox("IDLE_STATE_CHANGED", MARU_MASK_IDLE_CHANGED);
        event_checkbox("MONITOR_CHANGED", MARU_MASK_MONITOR_CHANGED);
        event_checkbox("MONITOR_MODE_CHANGED", MARU_MASK_MONITOR_MODE_CHANGED);
        event_checkbox("WINDOW_FRAME", MARU_MASK_WINDOW_FRAME);
        event_checkbox("WINDOW_PRESENTATION_CHANGED", MARU_MASK_WINDOW_PRESENTATION_CHANGED);
        event_checkbox("TEXT_EDIT_STARTED", MARU_MASK_TEXT_EDIT_STARTED);
        event_checkbox("TEXT_EDIT_UPDATED", MARU_MASK_TEXT_EDIT_UPDATED);
        event_checkbox("TEXT_EDIT_COMMITTED", MARU_MASK_TEXT_EDIT_COMMITTED);
        event_checkbox("TEXT_EDIT_ENDED", MARU_MASK_TEXT_EDIT_ENDED);

        ImGui::Separator();
        static int idle_timeout = 0;
        if (ImGui::SliderInt("Idle Timeout (ms)", &idle_timeout, 0, 10000)) {
            MARU_ContextAttributes attrs = {};
            attrs.idle_timeout_ms = (uint32_t)idle_timeout;
            maru_updateContext(ctx, MARU_CONTEXT_ATTR_IDLE_TIMEOUT, &attrs);
        }
    }
    ImGui::End();
}
