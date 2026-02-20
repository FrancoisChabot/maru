#ifndef MARU_BACKEND_H_INCLUDED
#define MARU_BACKEND_H_INCLUDED

#include "maru/c/contexts.h"
#include "maru/c/windows.h"
#include "maru/c/events.h"
#include "maru/c/ext/vulkan.h"

#ifdef MARU_INDIRECT_BACKEND
typedef struct MARU_Backend {
  MARU_Status (*destroyContext)(MARU_Context *context);
  MARU_Status (*pumpEvents)(MARU_Context *context, uint32_t timeout_ms);

  MARU_Status (*createWindow)(MARU_Context *context,
                              const MARU_WindowCreateInfo *create_info,
                              MARU_Window **out_window);
  MARU_Status (*destroyWindow)(MARU_Window *window);
  MARU_Status (*getWindowGeometry)(MARU_Window *window,
                                   MARU_WindowGeometry *out_geometry);

#ifdef MARU_ENABLE_VULKAN
  MARU_Status (*getVkExtensions)(MARU_Context *context,
                                 MARU_ExtensionList *out_list);
  MARU_Status (*createVkSurface)(MARU_Window *window, VkInstance instance,
                                 VkSurfaceKHR *out_surface);
#endif

  // More to be added directly here
} MARU_Backend;
#endif

#endif // MARU_BACKEND_H_INCLUDED
