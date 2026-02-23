// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#pragma once

#include "imgui.h"
#include "maru/maru.h"

IMGUI_IMPL_API bool     ImGui_ImplMaru_Init(MARU_Window* window);
IMGUI_IMPL_API void     ImGui_ImplMaru_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplMaru_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplMaru_HandleEvent(MARU_EventType type, const MARU_Event* event);
