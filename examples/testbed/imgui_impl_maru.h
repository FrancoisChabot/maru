// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#include <maru/maru.h>

IMGUI_IMPL_API bool     ImGui_ImplMaru_Init(MARU_Window* window);
IMGUI_IMPL_API void     ImGui_ImplMaru_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplMaru_NewFrame();

// You can either call this from your event callback, or let the implementation handle it if it could (but here it can't)
IMGUI_IMPL_API void     ImGui_ImplMaru_HandleEvent(MARU_EventType type, const MARU_Event* event);
