#ifndef MARU_BACKEND_H_INCLUDED
#define MARU_BACKEND_H_INCLUDED

#include "maru/maru.h"

#ifdef MARU_INDIRECT_BACKEND
typedef struct MARU_Backend {
  // maru_createContext() is intentionally ommited, as this is what populates the vtable

  __typeof__(maru_destroyContext) *destroyContext;
  __typeof__(maru_updateContext) *updateContext;

  __typeof__(maru_pumpEvents) *pumpEvents;
  __typeof__(maru_wakeContext) *wakeContext;

  __typeof__(maru_createWindow) *createWindow;
  __typeof__(maru_destroyWindow) *destroyWindow;
  __typeof__(maru_updateWindow) *updateWindow;
  __typeof__(maru_requestWindowFocus) *requestWindowFocus;
  __typeof__(maru_requestWindowFrame) *requestWindowFrame;
  __typeof__(maru_requestWindowAttention) *requestWindowAttention;

  __typeof__(maru_createCursor) *createCursor;
  __typeof__(maru_destroyCursor) *destroyCursor;
  __typeof__(maru_createImage) *createImage;
  __typeof__(maru_destroyImage) *destroyImage;

  __typeof__(maru_getControllers) *getControllers;
  __typeof__(maru_retainController) *retainController;
  __typeof__(maru_releaseController) *releaseController;
  __typeof__(maru_setControllerHapticLevels) *setControllerHapticLevels;

  MARU_Status (*announceData)(MARU_Window *window, MARU_DataExchangeTarget target,
                              MARU_StringList mime_types,
                              MARU_DropActionMask allowed_actions);
  __typeof__(maru_provideClipboardData) *provideData;
  MARU_Status (*requestData)(MARU_Window *window, MARU_DataExchangeTarget target,
                             const char *mime_type, void *userdata);
  MARU_Status (*getAvailableMIMETypes)(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       MARU_StringList *out_list);
  
  __typeof__(maru_getMonitors) *getMonitors;
  __typeof__(maru_retainMonitor) *retainMonitor;
  __typeof__(maru_releaseMonitor) *releaseMonitor;
  __typeof__(maru_getMonitorModes) *getMonitorModes;
  __typeof__(maru_setMonitorMode) *setMonitorMode;

  void *(*getContextNativeHandle)(MARU_Context *context);
  void *(*getWindowNativeHandle)(MARU_Window *window);

  __typeof__(maru_getVkExtensions) *getVkExtensions;
  __typeof__(maru_createVkSurface) *createVkSurface;

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
