#ifndef MARU_WINDOW_STATE_H_INCLUDED
#define MARU_WINDOW_STATE_H_INCLUDED

#include "maru/maru.h"

static inline MARU_WindowPresentationState
_maru_window_presentation_state_from_flags(uint64_t flags) {
  (void)flags;
  return MARU_WINDOW_PRESENTATION_NORMAL;
}

#endif // MARU_WINDOW_STATE_H_INCLUDED
