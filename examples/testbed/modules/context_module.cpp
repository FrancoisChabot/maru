// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "context_module.h"
#include "imgui.h"

ContextModule::ContextModule() {}

ContextModule::~ContextModule() {}

void ContextModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    (void)window;
}

void ContextModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    ImGui::Begin("Context Management", &enabled_);

    if (ImGui::Checkbox("Inhibit System Idle", &inhibit_idle_)) {
        maru_setContextInhibitsSystemIdle(ctx, inhibit_idle_);
    }

    if (ImGui::SliderInt("Idle Timeout (ms)", &idle_timeout_ms_, 0, 10000)) {
        maru_setContextIdleTimeout(ctx, (uint32_t)idle_timeout_ms_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##timeout")) {
        idle_timeout_ms_ = 0;
        maru_setContextIdleTimeout(ctx, 0);
    }

    ImGui::End();
}
