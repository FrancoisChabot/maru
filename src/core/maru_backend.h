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
  __typeof__(maru_updateContext) *updateContext;

  __typeof__(maru_createWindow) *createWindow;
  __typeof__(maru_destroyWindow) *destroyWindow;
  __typeof__(maru_getWindowGeometry) *getWindowGeometry;
  __typeof__(maru_updateWindow) *updateWindow;
  __typeof__(maru_requestWindowFocus) *requestWindowFocus;
  MARU_Status (*requestWindowFrame)(MARU_Window *window);
  __typeof__(maru_getWindowBackendHandle) *getWindowBackendHandle;

  __typeof__(maru_getStandardCursor) *getStandardCursor;
  __typeof__(maru_createCursor) *createCursor;
  __typeof__(maru_destroyCursor) *destroyCursor;
  __typeof__(maru_wakeContext) *wakeContext;

  MARU_Status (*updateMonitors)(MARU_Context *context);
  void (*destroyMonitor)(MARU_Monitor *monitor);
  __typeof__(maru_getMonitorModes) *getMonitorModes;
  __typeof__(maru_setMonitorMode) *setMonitorMode;
  __typeof__(maru_resetMonitorMetrics) *resetMonitorMetrics;

  void *(*getContextNativeHandle)(MARU_Context *context);
  void *(*getWindowNativeHandle)(MARU_Window *window);

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
