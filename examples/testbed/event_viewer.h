#pragma once
#include "maru/maru.h"

void EventViewer_HandleEvent(MARU_EventType type, const MARU_Event* event, uint64_t frame_id);
void EventViewer_Render(MARU_Window* window, bool* p_open);
