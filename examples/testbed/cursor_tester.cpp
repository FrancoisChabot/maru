#include "cursor_tester.h"
#include "imgui.h"
#include <vector>

void CursorTester_Render(MARU_Context* context, MARU_Window* window, bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cursor Tester", p_open)) {
        
        const char* mode_names[] = { "Normal", "Hidden", "Locked" };
        int current_mode = (int)maru_getWindowCursorMode(window);
        if (ImGui::Combo("Cursor Mode", &current_mode, mode_names, IM_ARRAYSIZE(mode_names))) {
            maru_setWindowCursorMode(window, (MARU_CursorMode)current_mode);
        }
        ImGui::TextDisabled("(Press ESC to reset to Normal)");

        ImGui::Separator();
        ImGui::Text("Standard Cursors:");
        
        struct { MARU_CursorShape shape; const char* name; } shapes[] = {
            { MARU_CURSOR_SHAPE_DEFAULT, "Default" },
            { MARU_CURSOR_SHAPE_HELP, "Help" },
            { MARU_CURSOR_SHAPE_HAND, "Hand" },
            { MARU_CURSOR_SHAPE_WAIT, "Wait" },
            { MARU_CURSOR_SHAPE_CROSSHAIR, "Crosshair" },
            { MARU_CURSOR_SHAPE_TEXT, "Text" },
            { MARU_CURSOR_SHAPE_MOVE, "Move" },
            { MARU_CURSOR_SHAPE_NOT_ALLOWED, "Not Allowed" },
            { MARU_CURSOR_SHAPE_EW_RESIZE, "EW Resize" },
            { MARU_CURSOR_SHAPE_NS_RESIZE, "NS Resize" },
            { MARU_CURSOR_SHAPE_NESW_RESIZE, "NESW Resize" },
            { MARU_CURSOR_SHAPE_NWSE_RESIZE, "NWSE Resize" }
        };

        for (int i = 0; i < IM_ARRAYSIZE(shapes); i++) {
            if (ImGui::Button(shapes[i].name)) {
                MARU_Cursor* cursor = NULL;
                if (maru_getStandardCursor(context, shapes[i].shape, &cursor) == MARU_SUCCESS) {
                    maru_setWindowCursor(window, cursor);
                }
            }
            if ((i + 1) % 3 != 0) ImGui::SameLine();
        }

        ImGui::Separator();
        static MARU_Cursor* custom_cursor = NULL;
        if (ImGui::Button("Create Custom Cursor (Checkered)")) {
            if (custom_cursor) {
                maru_destroyCursor(custom_cursor);
                custom_cursor = NULL;
            }

            uint32_t pixels[16 * 16];
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 16; x++) {
                    pixels[y * 16 + x] = ((x / 4 + y / 4) % 2 == 0) ? 0xFFFF0000 : 0xFF00FF00;
                }
            }

            MARU_CursorCreateInfo ci = {};
            ci.size = { 16, 16 };
            ci.hot_spot = { 8, 8 };
            ci.pixels = pixels;
            
            if (maru_createCursor(context, &ci, &custom_cursor) == MARU_SUCCESS) {
                maru_setWindowCursor(window, custom_cursor);
            }
        }
        
        if (custom_cursor) {
            if (ImGui::Button("Set Custom Cursor")) {
                maru_setWindowCursor(window, custom_cursor);
            }
            ImGui::SameLine();
            if (ImGui::Button("Destroy Custom Cursor")) {
                maru_destroyCursor(custom_cursor);
                custom_cursor = NULL;
            }
        }
    }
    ImGui::End();
}
