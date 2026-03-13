#ifndef MARU_BACKEND_H_INCLUDED
#define MARU_BACKEND_H_INCLUDED

#include "maru/maru.h"
#include "maru_vulkan_types.h"

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

  __typeof__(maru_announceClipboardData) *announceClipboardData;
  __typeof__(maru_announceDragData) *announceDragData;
  __typeof__(maru_provideClipboardData) *provideData;
  __typeof__(maru_requestClipboardData) *requestClipboardData;
  __typeof__(maru_requestDropData) *requestDropData;
  __typeof__(maru_getAvailableClipboardMIMETypes) *getAvailableClipboardMIMETypes;
  __typeof__(maru_getAvailableDropMIMETypes) *getAvailableDropMIMETypes;
  
  __typeof__(maru_getMonitors) *getMonitors;
  __typeof__(maru_retainMonitor) *retainMonitor;
  __typeof__(maru_releaseMonitor) *releaseMonitor;
  __typeof__(maru_getMonitorModes) *getMonitorModes;
  __typeof__(maru_setMonitorMode) *setMonitorMode;

  void *(*getContextNativeHandle)(MARU_Context *context);
  void *(*getWindowNativeHandle)(MARU_Window *window);

  MARU_Status (*getVkExtensions)(const MARU_Context *context,
                                 MARU_StringList *out_list);
  MARU_Status (*createVkSurface)(const MARU_Window *window, VkInstance instance,
                                 MARU_VkGetInstanceProcAddrFunc vk_loader,
                                 VkSurfaceKHR *out_surface);

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
