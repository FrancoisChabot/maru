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
  MARU_Status (*updateContext)(MARU_Context *context, uint64_t field_mask,
                               const MARU_ContextAttributes *attributes);

  MARU_Status (*createWindow)(MARU_Context *context,
                              const MARU_WindowCreateInfo *create_info,
                              MARU_Window **out_window);
  MARU_Status (*destroyWindow)(MARU_Window *window);
  MARU_Status (*getWindowGeometry)(MARU_Window *window,
                                   MARU_WindowGeometry *out_geometry);
  MARU_Status (*updateWindow)(MARU_Window *window, uint64_t field_mask,
                               const MARU_WindowAttributes *attributes);
  MARU_Status (*requestWindowFocus)(MARU_Window *window);
  MARU_Status (*getWindowBackendHandle)(MARU_Window *window,
                                         MARU_BackendType *out_type,
                                         MARU_BackendHandle *out_handle);

  MARU_Status (*getStandardCursor)(MARU_Context *context, MARU_CursorShape shape,
                                   MARU_Cursor **out_cursor);
  MARU_Status (*createCursor)(MARU_Context *context,
                              const MARU_CursorCreateInfo *create_info,
                              MARU_Cursor **out_cursor);
  MARU_Status (*destroyCursor)(MARU_Cursor *cursor);
  MARU_Status (*wakeContext)(MARU_Context *context);

  MARU_Status (*updateMonitors)(MARU_Context *context);
  MARU_Status (*destroyMonitor)(MARU_Monitor *monitor);
  MARU_Status (*getMonitorModes)(const MARU_Monitor *monitor,
                                 MARU_VideoModeList *out_list);
  MARU_Status (*setMonitorMode)(const MARU_Monitor *monitor,
                                MARU_VideoMode mode);
  MARU_Status (*resetMonitorMetrics)(MARU_Monitor *monitor);

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
