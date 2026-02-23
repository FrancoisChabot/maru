// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "window_module.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

void WindowModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void WindowModule::render(MARU_Context* ctx, MARU_Window* window) {
    if (!enabled_) return;

    if (ImGui::Begin("Window", &enabled_)) {
        if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Handle: %p", (void*)window);
            ImGui::Text("Ready: %s", maru_isWindowReady(window) ? "YES" : "NO");
            ImGui::Text("Focused: %s", maru_isWindowFocused(window) ? "YES" : "NO");
            ImGui::Text("Maximized: %s", maru_isWindowMaximized(window) ? "YES" : "NO");
            ImGui::Text("Fullscreen: %s", maru_isWindowFullscreen(window) ? "YES" : "NO");
            
            MARU_WindowGeometry geometry;
            maru_getWindowGeometry(window, &geometry);
            ImGui::Text("Logical Size: %.1fx%.1f", geometry.logical_size.x, geometry.logical_size.y);
            ImGui::Text("Pixel Size: %dx%d", geometry.pixel_size.x, geometry.pixel_size.y);
            ImGui::Text("Position: %.1f, %.1f", geometry.origin.x, geometry.origin.y);
        }

        if (ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::InputText("Title", title_buf_, sizeof(title_buf_))) {
                MARU_WindowAttributes attrs = {};
                attrs.title = title_buf_;
                maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE, &attrs);
            }

            static int size[2] = {1280, 800};
            ImGui::InputInt2("Logical Size", size);
            if (ImGui::Button("Set Size")) {
                MARU_WindowAttributes attrs = {};
                attrs.logical_size = {(MARU_Scalar)size[0], (MARU_Scalar)size[1]};
                maru_updateWindow(window, MARU_WINDOW_ATTR_LOGICAL_SIZE, &attrs);
            }

            bool fullscreen = maru_isWindowFullscreen(window);
            if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
                MARU_WindowAttributes attrs = {};
                attrs.fullscreen = fullscreen;
                maru_updateWindow(window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
            }

            bool maximized = maru_isWindowMaximized(window);
            if (ImGui::Checkbox("Maximized", &maximized)) {
                MARU_WindowAttributes attrs = {};
                attrs.maximized = maximized;
                maru_updateWindow(window, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
            }

            bool mouse_passthrough = maru_isWindowMousePassthrough(window);
            if (ImGui::Checkbox("Mouse Passthrough", &mouse_passthrough)) {
                MARU_WindowAttributes attrs = {};
                attrs.mouse_passthrough = mouse_passthrough;
                maru_updateWindow(window, MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH, &attrs);
            }
        }
        
        if (ImGui::Button("Request Focus")) {
            maru_requestWindowFocus(window);
        }
    }
    ImGui::End();
}
