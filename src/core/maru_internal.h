#ifndef MARU_INTERNAL_H_INCLUDED
#define MARU_INTERNAL_H_INCLUDED

#include <stdint.h>
#include "maru/maru.h"
#include "maru_backend.h"
#include "core_event_queue.h"

// The API headers define MARU_ContextExposed and MARU_WindowExposed
#include "maru/c/details/contexts.h"
#include "maru/c/details/windows.h"
#include "maru/c/details/cursors.h"
#include "maru/c/details/monitors.h"

/**
 * @file maru_internal.h
 * @brief Base structures for opaque handles.
 */

#ifdef __cplusplus
extern "C" {
#endif

void *_maru_default_alloc(size_t size, void *userdata);
void *_maru_default_realloc(void *ptr, size_t new_size, void *userdata);
void _maru_default_free(void *ptr, void *userdata);

#ifdef MARU_VALIDATE_API_CALLS
// We'll need a way to get thread IDs. For now, let's just use a placeholder
// until we implement a proper platform-specific thread utility.
typedef uint64_t MARU_ThreadId;
#endif

typedef struct MARU_External_Lib_Base {
  void *handle;
  bool available;
} MARU_External_Lib_Base;

typedef struct MARU_PumpContext {
  MARU_EventCallback callback;
  void *userdata;
} MARU_PumpContext;

typedef struct MARU_Window_Base MARU_Window_Base;

typedef struct MARU_Context_Base {
  MARU_ContextExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif
  
  MARU_Allocator allocator;

  MARU_DiagnosticCallback diagnostic_cb;
  void *diagnostic_userdata;

  MARU_ContextTuning tuning;

  MARU_ContextAttributes attrs_requested;
  MARU_ContextAttributes attrs_effective;
  uint64_t attrs_dirty_mask;

  MARU_PumpContext *pump_ctx;
  MARU_EventMask event_mask;
  bool inhibit_idle;

  MARU_ContextMetrics metrics;
  MARU_UserEventMetrics user_event_metrics;

  MARU_ExtensionCleanupCallback extension_cleanup[MARU_EXT_COUNT];

  MARU_Monitor **monitor_cache;
  uint32_t monitor_cache_count;
  uint32_t monitor_cache_capacity;

  MARU_Window_Base *window_list_head;
  uint32_t window_count;

  MARU_EventQueue user_event_queue;

#ifdef MARU_VALIDATE_API_CALLS
  MARU_ThreadId creator_thread;
#endif
  // Internal common state will go here
} MARU_Context_Base;

typedef struct MARU_Window_Base {
  MARU_WindowExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_Window_Base *ctx_prev;
  MARU_Window_Base *ctx_next;
  
  MARU_WindowMetrics metrics;

  MARU_WindowAttributes attrs_requested;
  MARU_WindowAttributes attrs_effective;
  uint64_t attrs_dirty_mask;

  MARU_ButtonState8 keyboard_state[MARU_KEY_COUNT];
  
  char *title_storage;
  char *surrounding_text_storage;
} MARU_Window_Base;

typedef struct MARU_Cursor_Base {
  MARU_CursorExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_CursorMetrics metrics;
} MARU_Cursor_Base;

typedef struct MARU_Monitor_Base {
  MARU_MonitorExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_MonitorMetrics metrics;
  
  uint32_t ref_count;
  bool is_active; // If false, the monitor was disconnected but is still retained.
} MARU_Monitor_Base;

#ifdef MARU_ENABLE_DIAGNOSTICS
void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
#define MARU_REPORT_DIAGNOSTIC(ctx, diag, msg) _maru_reportDiagnostic(ctx, diag, msg)
#else
#define MARU_REPORT_DIAGNOSTIC(ctx, diag, msg) (void)0
#endif

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventType type, MARU_Window *window, const MARU_Event *event);
void _maru_init_context_base(MARU_Context_Base *ctx_base);
void _maru_drain_user_events(MARU_Context_Base *ctx_base);
void _maru_update_context_base(MARU_Context_Base *ctx_base, uint64_t field_mask, const MARU_ContextAttributes *attributes);
void _maru_cleanup_context_base(MARU_Context_Base *ctx_base);
void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window);
void _maru_unregister_window(MARU_Context_Base *ctx_base, MARU_Window *window);

void _maru_monitor_free(MARU_Monitor_Base *monitor);

#ifdef __cplusplus
}
#endif

#endif // MARU_INTERNAL_H_INCLUDED
