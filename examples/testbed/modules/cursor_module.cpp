// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "cursor_module.h"
#include "imgui.h"
#include <cstdio>
#include <vector>

void CursorModule::update(MARU_Context* ctx, MARU_Window* window) {
    (void)ctx; (void)window;
}

void CursorModule::render(MARU_Context* ctx, MARU_Window* window) {
    (void)window;
    if (!enabled_) return;

    if (ImGui::Begin("Cursors", &enabled_)) {
        if (ImGui::CollapsingHeader("Cursor Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::RadioButton("Normal", requested_mode_ == MARU_CURSOR_NORMAL)) requested_mode_ = MARU_CURSOR_NORMAL;
            if (ImGui::RadioButton("Hidden", requested_mode_ == MARU_CURSOR_HIDDEN)) requested_mode_ = MARU_CURSOR_HIDDEN;
            if (ImGui::RadioButton("Locked", requested_mode_ == MARU_CURSOR_LOCKED)) requested_mode_ = MARU_CURSOR_LOCKED;
            
            if (requested_mode_ != MARU_CURSOR_NORMAL) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press ESC to return to Normal mode");
                
                // Scan events for ESC
                // In a real app we'd handle this in onEvent, but here we can check imgui
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    requested_mode_ = MARU_CURSOR_NORMAL;
                }
            }
        }

        if (ImGui::CollapsingHeader("Standard Shapes", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto show_shape = [&](const char* name, MARU_CursorShape shape) {
                if (ImGui::Button(name)) {
                    MARU_Cursor* cursor = nullptr;
                    if (maru_getStandardCursor(ctx, shape, &cursor) == MARU_SUCCESS) {
                        MARU_WindowAttributes attrs = {};
                        attrs.cursor = cursor;
                        maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
                    }
                }
            };

            ImGui::Columns(2, "ShapesColumns", false);
            show_shape("Default", MARU_CURSOR_SHAPE_DEFAULT);
            show_shape("Help", MARU_CURSOR_SHAPE_HELP);
            show_shape("Hand", MARU_CURSOR_SHAPE_HAND);
            show_shape("Wait", MARU_CURSOR_SHAPE_WAIT);
            show_shape("Crosshair", MARU_CURSOR_SHAPE_CROSSHAIR);
            show_shape("Text", MARU_CURSOR_SHAPE_TEXT);
            ImGui::NextColumn();
            show_shape("Move", MARU_CURSOR_SHAPE_MOVE);
            show_shape("Not Allowed", MARU_CURSOR_SHAPE_NOT_ALLOWED);
            show_shape("EW Resize", MARU_CURSOR_SHAPE_EW_RESIZE);
            show_shape("NS Resize", MARU_CURSOR_SHAPE_NS_RESIZE);
            show_shape("NESW Resize", MARU_CURSOR_SHAPE_NESW_RESIZE);
            show_shape("NWSE Resize", MARU_CURSOR_SHAPE_NWSE_RESIZE);
            ImGui::Columns(1);
        }

        if (ImGui::CollapsingHeader("Custom Cursor", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Create/Set 16x16 Checkered Cursor")) {
                if (custom_cursor_) {
                    maru_destroyCursor(custom_cursor_);
                }

                uint32_t pixels[16 * 16];
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        pixels[y * 16 + x] = ((x / 4 + y / 4) % 2 == 0) ? 0xFFFFFFFF : 0xFF000000;
                    }
                }

                MARU_CursorCreateInfo ci = {};
                ci.size = {16, 16};
                ci.hot_spot = {8, 8};
                ci.pixels = pixels;
                
                if (maru_createCursor(ctx, &ci, &custom_cursor_) == MARU_SUCCESS) {
                    MARU_WindowAttributes attrs = {};
                    attrs.cursor = custom_cursor_;
                    maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
                }
            }
            
            if (custom_cursor_) {
                ImGui::SameLine();
                if (ImGui::Button("Destroy Custom")) {
                    maru_destroyCursor(custom_cursor_);
                    custom_cursor_ = nullptr;
                }
            }
        }
    }
    ImGui::End();
}
