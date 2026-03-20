#ifndef MARU_WINDOW_STATE_H_INCLUDED
#define MARU_WINDOW_STATE_H_INCLUDED

#include "maru/maru.h"

static inline MARU_WindowPresentationState
_maru_window_presentation_state_from_flags(uint64_t flags) {
  if ((flags & MARU_WINDOW_STATE_FULLSCREEN) != 0) {
    return MARU_WINDOW_PRESENTATION_FULLSCREEN;
  }
  if ((flags & MARU_WINDOW_STATE_MINIMIZED) != 0) {
    return MARU_WINDOW_PRESENTATION_MINIMIZED;
  }
  return MARU_WINDOW_PRESENTATION_NORMAL;
}

#endif // MARU_WINDOW_STATE_H_INCLUDED
