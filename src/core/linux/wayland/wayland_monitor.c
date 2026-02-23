#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

void _maru_wayland_bind_output(MARU_Context_WL *ctx, uint32_t name, uint32_t version) {
  MARU_Monitor_WL *monitor = maru_context_alloc(&ctx->base, sizeof(MARU_Monitor_WL));
 
}

void _maru_wayland_remove_output(MARU_Context_WL *ctx, uint32_t name) {
 
}

MARU_Monitor *const *maru_getMonitors_WL(MARU_Context *context, uint32_t *out_count) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;
  *out_count = 0;
  return NULL;
}

MARU_Status maru_updateMonitors_WL(MARU_Context *context) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)context;

  return MARU_FAILURE;
}

const MARU_VideoMode *maru_getMonitorModes_WL(const MARU_Monitor *monitor_handle, uint32_t *out_count) {
  const MARU_Monitor_WL *monitor = (const MARU_Monitor_WL *)monitor_handle;
  *out_count = 0;
  return NULL;
}

MARU_Status maru_setMonitorMode_WL(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  (void)monitor; (void)mode;
  return MARU_FAILURE; // Wayland doesn't support setting modes directly
}

