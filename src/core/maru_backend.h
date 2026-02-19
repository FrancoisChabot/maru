#ifndef MARU_BACKEND_H_INCLUDED
#define MARU_BACKEND_H_INCLUDED

#include "maru/c/contexts.h"
#include "maru/c/windows.h"
#include "maru/c/events.h"
#include "maru/c/ext/vulkan.h"

#ifdef MARU_INDIRECT_BACKEND
typedef struct MARU_Backend {
  __typeof__(maru_destroyContext) *destroyContext;
  __typeof__(maru_pumpEvents) *pumpEvents;

  __typeof__(maru_createWindow) *createWindow;
  __typeof__(maru_destroyWindow) *destroyWindow;
  __typeof__(maru_getWindowGeometry) *getWindowGeometry;

#ifdef MARU_ENABLE_VULKAN
  __typeof__(maru_getVkExtensions) *getVkExtensions;
  __typeof__(maru_createVkSurface) *createVkSurface;
#endif

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
