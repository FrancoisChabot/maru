// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "imgui_impl_maru.h"
#include "maru/c/cursors.h"

#include <maru/c/events.h>
#include <maru/c/inputs.h>
#include <maru/c/details/windows.h>
#include <maru/c/windows.h>
#include <maru/maru.h>

struct ImGui_ImplMaru_Data {
    MARU_Window* Window;
    uint32_t TimeMs;
    MARU_Cursor* MouseCursors[ImGuiMouseCursor_COUNT];

    ImGui_ImplMaru_Data() { memset(this, 0, sizeof(*this)); }
};

static ImGui_ImplMaru_Data* ImGui_ImplMaru_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplMaru_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

static ImGuiKey ImGui_ImplMaru_KeyToImGuiKey(MARU_Key key) {
    switch (key) {
        case MARU_KEY_TAB: return ImGuiKey_Tab;
        case MARU_KEY_LEFT: return ImGuiKey_LeftArrow;
        case MARU_KEY_RIGHT: return ImGuiKey_RightArrow;
        case MARU_KEY_UP: return ImGuiKey_UpArrow;
        case MARU_KEY_DOWN: return ImGuiKey_DownArrow;
        case MARU_KEY_PAGE_UP: return ImGuiKey_PageUp;
        case MARU_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
        case MARU_KEY_HOME: return ImGuiKey_Home;
        case MARU_KEY_END: return ImGuiKey_End;
        case MARU_KEY_INSERT: return ImGuiKey_Insert;
        case MARU_KEY_DELETE: return ImGuiKey_Delete;
        case MARU_KEY_BACKSPACE: return ImGuiKey_Backspace;
        case MARU_KEY_SPACE: return ImGuiKey_Space;
        case MARU_KEY_ENTER: return ImGuiKey_Enter;
        case MARU_KEY_ESCAPE: return ImGuiKey_Escape;
        case MARU_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
        case MARU_KEY_COMMA: return ImGuiKey_Comma;
        case MARU_KEY_MINUS: return ImGuiKey_Minus;
        case MARU_KEY_PERIOD: return ImGuiKey_Period;
        case MARU_KEY_SLASH: return ImGuiKey_Slash;
        case MARU_KEY_SEMICOLON: return ImGuiKey_Semicolon;
        case MARU_KEY_EQUAL: return ImGuiKey_Equal;
        case MARU_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
        case MARU_KEY_BACKSLASH: return ImGuiKey_Backslash;
        case MARU_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case MARU_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
        case MARU_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
        case MARU_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
        case MARU_KEY_NUM_LOCK: return ImGuiKey_NumLock;
        case MARU_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
        case MARU_KEY_PAUSE: return ImGuiKey_Pause;
        case MARU_KEY_KP_0: return ImGuiKey_Keypad0;
        case MARU_KEY_KP_1: return ImGuiKey_Keypad1;
        case MARU_KEY_KP_2: return ImGuiKey_Keypad2;
        case MARU_KEY_KP_3: return ImGuiKey_Keypad3;
        case MARU_KEY_KP_4: return ImGuiKey_Keypad4;
        case MARU_KEY_KP_5: return ImGuiKey_Keypad5;
        case MARU_KEY_KP_6: return ImGuiKey_Keypad6;
        case MARU_KEY_KP_7: return ImGuiKey_Keypad7;
        case MARU_KEY_KP_8: return ImGuiKey_Keypad8;
        case MARU_KEY_KP_9: return ImGuiKey_Keypad9;
        case MARU_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
        case MARU_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case MARU_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case MARU_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case MARU_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
        case MARU_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
        case MARU_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
        case MARU_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
        case MARU_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
        case MARU_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
        case MARU_KEY_LEFT_META: return ImGuiKey_LeftSuper;
        case MARU_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
        case MARU_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
        case MARU_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
        case MARU_KEY_RIGHT_META: return ImGuiKey_RightSuper;
        case MARU_KEY_MENU: return ImGuiKey_Menu;
        case MARU_KEY_0: return ImGuiKey_0;
        case MARU_KEY_1: return ImGuiKey_1;
        case MARU_KEY_2: return ImGuiKey_2;
        case MARU_KEY_3: return ImGuiKey_3;
        case MARU_KEY_4: return ImGuiKey_4;
        case MARU_KEY_5: return ImGuiKey_5;
        case MARU_KEY_6: return ImGuiKey_6;
        case MARU_KEY_7: return ImGuiKey_7;
        case MARU_KEY_8: return ImGuiKey_8;
        case MARU_KEY_9: return ImGuiKey_9;
        case MARU_KEY_A: return ImGuiKey_A;
        case MARU_KEY_B: return ImGuiKey_B;
        case MARU_KEY_C: return ImGuiKey_C;
        case MARU_KEY_D: return ImGuiKey_D;
        case MARU_KEY_E: return ImGuiKey_E;
        case MARU_KEY_F: return ImGuiKey_F;
        case MARU_KEY_G: return ImGuiKey_G;
        case MARU_KEY_H: return ImGuiKey_H;
        case MARU_KEY_I: return ImGuiKey_I;
        case MARU_KEY_J: return ImGuiKey_J;
        case MARU_KEY_K: return ImGuiKey_K;
        case MARU_KEY_L: return ImGuiKey_L;
        case MARU_KEY_M: return ImGuiKey_M;
        case MARU_KEY_N: return ImGuiKey_N;
        case MARU_KEY_O: return ImGuiKey_O;
        case MARU_KEY_P: return ImGuiKey_P;
        case MARU_KEY_Q: return ImGuiKey_Q;
        case MARU_KEY_R: return ImGuiKey_R;
        case MARU_KEY_S: return ImGuiKey_S;
        case MARU_KEY_T: return ImGuiKey_T;
        case MARU_KEY_U: return ImGuiKey_U;
        case MARU_KEY_V: return ImGuiKey_V;
        case MARU_KEY_W: return ImGuiKey_W;
        case MARU_KEY_X: return ImGuiKey_X;
        case MARU_KEY_Y: return ImGuiKey_Y;
        case MARU_KEY_Z: return ImGuiKey_Z;
        case MARU_KEY_F1: return ImGuiKey_F1;
        case MARU_KEY_F2: return ImGuiKey_F2;
        case MARU_KEY_F3: return ImGuiKey_F3;
        case MARU_KEY_F4: return ImGuiKey_F4;
        case MARU_KEY_F5: return ImGuiKey_F5;
        case MARU_KEY_F6: return ImGuiKey_F6;
        case MARU_KEY_F7: return ImGuiKey_F7;
        case MARU_KEY_F8: return ImGuiKey_F8;
        case MARU_KEY_F9: return ImGuiKey_F9;
        case MARU_KEY_F10: return ImGuiKey_F10;
        case MARU_KEY_F11: return ImGuiKey_F11;
        case MARU_KEY_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

static void ImGui_ImplMaru_SetImeData(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
    ImGui_ImplMaru_Data* bd = ImGui_ImplMaru_GetBackendData();
    if (!bd) return;

    MARU_WindowAttributes attrs = {};
    if (data->WantVisible) {
        // ImGui provides an absolute position; Wayland text-input expects surface-local coordinates.
        const float viewport_x = viewport ? viewport->Pos.x : 0.0f;
        const float viewport_y = viewport ? viewport->Pos.y : 0.0f;
        const float local_x = data->InputPos.x - viewport_x;
        const float local_y = data->InputPos.y - viewport_y;
        const float line_h = (data->InputLineHeight > 1.0f) ? data->InputLineHeight : 1.0f;

        attrs.text_input_type = MARU_TEXT_INPUT_TYPE_TEXT;
        attrs.text_input_rect.origin.x = (MARU_Scalar)local_x;
        attrs.text_input_rect.origin.y = (MARU_Scalar)local_y;
        attrs.text_input_rect.size.x = (MARU_Scalar)1.0;
        attrs.text_input_rect.size.y = (MARU_Scalar)line_h;
        maru_updateWindow(
            bd->Window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_TEXT_INPUT_RECT, &attrs);
    } else {
        attrs.text_input_type = MARU_TEXT_INPUT_TYPE_NONE;
        maru_updateWindow(bd->Window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE, &attrs);
    }
}

bool ImGui_ImplMaru_Init(MARU_Window* window) {
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();

    ImGui_ImplMaru_Data* bd = IM_NEW(ImGui_ImplMaru_Data)();
    bd->Window = window;
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_maru";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_SetImeDataFn = ImGui_ImplMaru_SetImeData;

    return true;
}

void ImGui_ImplMaru_Shutdown() {
    ImGui_ImplMaru_Data* bd = ImGui_ImplMaru_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    for (int i = 0; i < ImGuiMouseCursor_COUNT; i++) {
        if (bd->MouseCursors[i]) maru_destroyCursor(bd->MouseCursors[i]);
        bd->MouseCursors[i] = NULL;
    }

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplMaru_UpdateMouseCursor() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplMaru_Data* bd = ImGui_ImplMaru_GetBackendData();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    // Respect app-managed cursor modes (e.g. locked/hidden in the testbed).
    if (maru_getWindowCursorMode(bd->Window) != MARU_CURSOR_NORMAL)
        return;

    const MARU_Cursor* active_cursor =
        ((const MARU_WindowExposed*)bd->Window)->current_cursor;
    if (active_cursor) {
        bool imgui_managed_cursor = false;
        for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i) {
            if (active_cursor == bd->MouseCursors[i]) {
                imgui_managed_cursor = true;
                break;
            }
        }

        // If another module applied a cursor override, keep it.
        if (!imgui_managed_cursor)
            return;
    }

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        if (maru_getWindowCursorMode(bd->Window) != MARU_CURSOR_HIDDEN) {
            MARU_WindowAttributes attrs = {};
            attrs.cursor_mode = MARU_CURSOR_HIDDEN;
            maru_updateWindow(bd->Window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
        }
    } else {
        MARU_Cursor* cursor = bd->MouseCursors[imgui_cursor];
        if (!cursor) {
             MARU_CursorShape shape = MARU_CURSOR_SHAPE_DEFAULT;
             switch (imgui_cursor) {
                 case ImGuiMouseCursor_Arrow: shape = MARU_CURSOR_SHAPE_DEFAULT; break;
                 case ImGuiMouseCursor_TextInput: shape = MARU_CURSOR_SHAPE_TEXT; break;
                 case ImGuiMouseCursor_ResizeAll: shape = MARU_CURSOR_SHAPE_MOVE; break;
                 case ImGuiMouseCursor_ResizeNS: shape = MARU_CURSOR_SHAPE_NS_RESIZE; break;
                 case ImGuiMouseCursor_ResizeEW: shape = MARU_CURSOR_SHAPE_EW_RESIZE; break;
                 case ImGuiMouseCursor_ResizeNESW: shape = MARU_CURSOR_SHAPE_NESW_RESIZE; break;
                 case ImGuiMouseCursor_ResizeNWSE: shape = MARU_CURSOR_SHAPE_NWSE_RESIZE; break;
                 case ImGuiMouseCursor_Hand: shape = MARU_CURSOR_SHAPE_HAND; break;
                 case ImGuiMouseCursor_NotAllowed: shape = MARU_CURSOR_SHAPE_NOT_ALLOWED; break;
             }
             MARU_Context* ctx = maru_getWindowContext(bd->Window);
             MARU_CursorCreateInfo ci = {};
             ci.source = MARU_CURSOR_SOURCE_SYSTEM;
             ci.system_shape = shape;
             maru_createCursor(ctx, &ci, &cursor);
             bd->MouseCursors[imgui_cursor] = cursor;
        }

        const MARU_WindowExposed* win = (const MARU_WindowExposed*)bd->Window;
        const bool mode_ok = (maru_getWindowCursorMode(bd->Window) == MARU_CURSOR_NORMAL);
        const bool cursor_ok = (win->current_cursor == cursor);
        if (!mode_ok || !cursor_ok) {
            MARU_WindowAttributes attrs = {};
            attrs.cursor_mode = MARU_CURSOR_NORMAL;
            attrs.cursor = cursor;
            maru_updateWindow(bd->Window, MARU_WINDOW_ATTR_CURSOR_MODE | MARU_WINDOW_ATTR_CURSOR, &attrs);
        }
    }
}

void ImGui_ImplMaru_NewFrame() {
    ImGui_ImplMaru_Data* bd = ImGui_ImplMaru_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    MARU_WindowGeometry geometry;
    maru_getWindowGeometry(bd->Window, &geometry);
    io.DisplaySize = ImVec2((float)geometry.logical_size.x, (float)geometry.logical_size.y);
    if (geometry.logical_size.x > 0 && geometry.logical_size.y > 0)
        io.DisplayFramebufferScale = ImVec2((float)geometry.pixel_size.x / (float)geometry.logical_size.x, (float)geometry.pixel_size.y / (float)geometry.logical_size.y);

    // TODO: delta time

    ImGui_ImplMaru_UpdateMouseCursor();
}

void ImGui_ImplMaru_HandleEvent(MARU_EventType type, const MARU_Event* event) {
    ImGuiIO& io = ImGui::GetIO();
    if (type == MARU_MOUSE_MOVED) {
        io.AddMousePosEvent((float)event->mouse_motion.position.x, (float)event->mouse_motion.position.y);
    } else if (type == MARU_MOUSE_BUTTON_STATE_CHANGED) {
        int mouse_button = -1;
        if (event->mouse_button.button_id == 0) mouse_button = 0;
        if (event->mouse_button.button_id == 1) mouse_button = 1;
        if (event->mouse_button.button_id == 2) mouse_button = 2;
        if (mouse_button != -1)
            io.AddMouseButtonEvent(mouse_button, event->mouse_button.state == MARU_BUTTON_STATE_PRESSED);
    } else if (type == MARU_MOUSE_SCROLLED) {
        io.AddMouseWheelEvent((float)event->mouse_scroll.delta.x, (float)event->mouse_scroll.delta.y);
    } else if (type == MARU_KEY_STATE_CHANGED) {
        ImGuiKey key = ImGui_ImplMaru_KeyToImGuiKey(event->key.raw_key);
        if (key != ImGuiKey_None) {
            io.AddKeyEvent(key, event->key.state == MARU_BUTTON_STATE_PRESSED);
            io.AddKeyEvent(ImGuiMod_Ctrl, (event->key.modifiers & MARU_MODIFIER_CONTROL) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (event->key.modifiers & MARU_MODIFIER_SHIFT) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (event->key.modifiers & MARU_MODIFIER_ALT) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (event->key.modifiers & MARU_MODIFIER_META) != 0);
        }
    } else if (type == MARU_TEXT_EDIT_COMMIT) {
        io.AddInputCharactersUTF8(event->text_edit_commit.committed_utf8);
    } else if (type == MARU_WINDOW_PRESENTATION_STATE_CHANGED) {
        if ((event->presentation.changed_fields & MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED) != 0u) {
            io.AddFocusEvent(event->presentation.focused);
        }
    }
}
