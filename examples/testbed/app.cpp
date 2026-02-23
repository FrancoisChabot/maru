// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "app.h"
#include "modules/event_log_module.h"
#include "modules/metrics_module.h"
#include "modules/monitor_module.h"
#include "modules/cursor_module.h"
#include "modules/window_module.h"
#include "modules/event_module.h"
#include "modules/input_module.h"
#include "modules/instrumentation_module.h"
#include "imgui.h"

App::App(MARU_Window* window, VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, uint32_t queue_family, VkQueue queue, VkDescriptorPool descriptor_pool) {
    (void)instance; (void)physical_device; (void)device; (void)queue_family; (void)queue; (void)descriptor_pool;
    primary_window_handle_ = window;

    auto inst = std::make_unique<InstrumentationModule>();
    inst_module_ = inst.get();
    modules_.push_back(std::move(inst));

    modules_.push_back(std::make_unique<EventModule>());
    modules_.push_back(std::make_unique<EventLogModule>());
    modules_.push_back(std::make_unique<MetricsModule>());
    modules_.push_back(std::make_unique<MonitorModule>());
    modules_.push_back(std::make_unique<CursorModule>());
    modules_.push_back(std::make_unique<WindowModule>());
    modules_.push_back(std::make_unique<InputModule>());
}

App::~App() {
}

void App::onEvent(MARU_EventType type, MARU_Window* window, const MARU_Event& event) {
    if (type == MARU_CLOSE_REQUESTED && window == primary_window_handle_) {
        exit_requested_ = true;
    }

    for (auto& mod : modules_) {
        mod->onEvent(type, window, event);
    }
}

void App::onDiagnostic(const MARU_DiagnosticInfo* info) {
    if (inst_module_) {
        inst_module_->onDiagnostic(info);
    }
}

AppStatus App::update(MARU_Context* ctx, MARU_Window* window) {
    if (exit_requested_ || maru_isWindowLost(window)) {
        return AppStatus::EXIT;
    }

    for (auto& mod : modules_) {
        mod->update(ctx, window);
    }

    return AppStatus::KEEP_GOING;
}

void App::updateCursor(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx;
    MARU_CursorMode requested_mode = MARU_CURSOR_NORMAL;
    for (auto& mod : modules_) {
        if (auto cursor_mod = dynamic_cast<CursorModule*>(mod.get())) {
            requested_mode = cursor_mod->getRequestedMode();
            break;
        }
    }

    if (requested_mode != MARU_CURSOR_NORMAL) {
        MARU_WindowAttributes attrs = {};
        attrs.cursor_mode = requested_mode;
        maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
    }
}

void App::renderUi(MARU_Context* ctx, MARU_Window* window) {
    {
        ImGui::Begin("MARU Testbed");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        
        ImGui::Separator();
        ImGui::Text("Modules");
        
        for (auto& mod : modules_) {
            ImGui::Checkbox(mod->getName(), &mod->getEnabled());
        }

        ImGui::End();
    }

    for (auto& mod : modules_) {
        if (mod->getEnabled()) {
            mod->render(ctx, window);
        }
    }
}

void App::onContextRecreated(MARU_Context* ctx, MARU_Window* window) {
    primary_window_handle_ = window;
    for (auto& mod : modules_) {
        mod->onContextRecreated(ctx, window);
    }
}
