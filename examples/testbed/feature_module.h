// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "maru/maru.h"

class FeatureModule {
public:
    virtual ~FeatureModule() = default;

    // Called every frame. Handle logic here.
    virtual void update(MARU_Context* ctx, MARU_Window* window) = 0;

    // Called every frame. Draw specific ImGui window(s) here.
    virtual void render(MARU_Context* ctx, MARU_Window* window) = 0;

    // Handle events
    virtual void onEvent(MARU_EventType type, MARU_Window* window, const MARU_Event& event) {
        (void)type; (void)window; (void)event;
    }

    // For the Table of Contents
    virtual const char* getName() const = 0;
    virtual bool& getEnabled() = 0;

    // Called after primary context/window are recreated (if applicable)
    virtual void onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
        (void)ctx;
        (void)window;
    }
};
