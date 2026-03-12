// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "../feature_module.h"
#include <vector>
#include <string>

class InstrumentationModule : public FeatureModule {
public:
    void update(MARU_Context* ctx, MARU_Window* window) override;
    void render(MARU_Context* ctx, MARU_Window* window) override;

    void onDiagnostic(const MARU_DiagnosticInfo* info);

    const char* getName() const override { return "Instrumentation"; }
    bool& getEnabled() override { return enabled_; }

private:
    struct LogEntry {
        double timestamp;
        MARU_Diagnostic diagnostic;
        std::string message;
        const MARU_Context* context;
        const MARU_Window* window;
    };

    std::vector<LogEntry> logs_;
    bool enabled_ = false;
    bool auto_scroll_ = true;

    const char* diagnosticToString(MARU_Diagnostic d);
};
