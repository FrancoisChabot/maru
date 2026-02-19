#ifndef MARU_BACKEND_H_INCLUDED
#define MARU_BACKEND_H_INCLUDED

#include "maru/c/contexts.h"

#ifdef MARU_INDIRECT_BACKEND
typedef struct MARU_Backend {
  __typeof__(maru_destroyContext) *destroyContext;

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
