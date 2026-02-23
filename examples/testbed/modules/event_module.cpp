// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "event_module.h"
#include "imgui.h"
#include "maru/c/events.h"
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
                MARU_ContextAttributes attrs = {};
                attrs.event_mask = mask;
                maru_updateContext(ctx, MARU_CONTEXT_ATTR_EVENT_MASK, &attrs);
                context_mask_ = mask;
            }
        };

        event_checkbox("CLOSE_REQUESTED", MARU_CLOSE_REQUESTED);
        event_checkbox("WINDOW_RESIZED", MARU_WINDOW_RESIZED);
        event_checkbox("KEY_STATE_CHANGED", MARU_KEY_STATE_CHANGED);
        event_checkbox("WINDOW_READY", MARU_WINDOW_READY);
        event_checkbox("MOUSE_MOVED", MARU_MOUSE_MOVED);
        event_checkbox("MOUSE_BUTTON_STATE_CHANGED", MARU_MOUSE_BUTTON_STATE_CHANGED);
        event_checkbox("MOUSE_SCROLLED", MARU_MOUSE_SCROLLED);
        event_checkbox("IDLE_STATE_CHANGED", MARU_IDLE_STATE_CHANGED);
        event_checkbox("MONITOR_CONNECTION_CHANGED", MARU_MONITOR_CONNECTION_CHANGED);
        event_checkbox("MONITOR_MODE_CHANGED", MARU_MONITOR_MODE_CHANGED);
        event_checkbox("WINDOW_FRAME", MARU_WINDOW_FRAME);
        event_checkbox("TEXT_INPUT_RECEIVED", MARU_TEXT_INPUT_RECEIVED);
        event_checkbox("FOCUS_CHANGED", MARU_FOCUS_CHANGED);
        event_checkbox("WINDOW_MAXIMIZED", MARU_WINDOW_MAXIMIZED);
        event_checkbox("TEXT_COMPOSITION_UPDATED", MARU_TEXT_COMPOSITION_UPDATED);

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
