#ifndef MARU_INTERNAL_H_INCLUDED
#define MARU_INTERNAL_H_INCLUDED

#include <stdint.h>
#include <stdatomic.h>
#include "maru/maru.h"
#include "maru_backend.h"
#include "core_event_queue.h"

// The API headers define MARU_ContextExposed and MARU_WindowExposed
#include "maru/c/details/contexts.h"
#include "maru/c/details/windows.h"
#include "maru/c/details/cursors.h"
#include "maru/c/details/images.h"
#include "maru/c/details/monitors.h"
#include "maru/c/details/controllers.h"

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
typedef struct MARU_Cursor_Base MARU_Cursor_Base;
typedef struct MARU_DataRequestHandleBase {
  struct MARU_Context_Base *ctx_base;
} MARU_DataRequestHandleBase;

typedef struct MARU_CursorAnimationCallbacks {
  void (*select_frame)(MARU_Cursor_Base *cursor, uint32_t frame_index);
  bool (*reapply)(MARU_Cursor_Base *cursor);
  void (*on_reapplied)(MARU_Cursor_Base *cursor);
} MARU_CursorAnimationCallbacks;

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

  MARU_Monitor **monitor_cache;
  uint32_t monitor_cache_count;
  uint32_t monitor_cache_capacity;

  MARU_Window_Base *window_list_head;
  MARU_Cursor_Base *animated_cursor_head;
  uint32_t window_count;

  MARU_EventQueue queued_events;
  MARU_ButtonState8 *mouse_button_states;
  MARU_MouseButtonChannelInfo *mouse_button_channels;

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
  MARU_ButtonState8 *mouse_button_states;
  MARU_MouseButtonChannelInfo *mouse_button_channels;
  
  char *title_storage;
  char *surrounding_text_storage;

  bool pending_ready_event;
} MARU_Window_Base;

struct MARU_Cursor_Base {
  MARU_CursorExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_CursorMetrics metrics;
  const uint32_t *anim_frame_delays_ms;
  const MARU_CursorAnimationCallbacks *anim_callbacks;
  MARU_Cursor_Base *anim_prev;
  MARU_Cursor_Base *anim_next;
  uint32_t anim_frame_count;
  uint32_t anim_current_frame;
  uint64_t anim_next_frame_deadline_ms;
  bool anim_enabled;
};

typedef struct MARU_Image_Base {
  MARU_ImageExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  uint32_t width;
  uint32_t height;
  uint32_t stride_bytes;
  uint8_t *pixels;
} MARU_Image_Base;

typedef enum MARU_InternalEventId {
  MARU_EVENT_INTERNAL_LINUX_DEVICE_ADD = 1000,
  MARU_EVENT_INTERNAL_LINUX_DEVICE_REMOVE = 1001,
} MARU_InternalEventId;

typedef struct MARU_Monitor_Base {
  MARU_MonitorExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_MonitorMetrics metrics;
  
  _Atomic uint32_t ref_count;
  bool is_active; // If false, the monitor was disconnected but is still retained.
} MARU_Monitor_Base;

typedef struct MARU_Controller_Base {
  MARU_ControllerExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  MARU_ControllerMetrics metrics;
  
  _Atomic uint32_t ref_count;
  bool is_active;
} MARU_Controller_Base;

#ifdef MARU_ENABLE_DIAGNOSTICS
void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
#define MARU_REPORT_DIAGNOSTIC(ctx, diag, msg) _maru_reportDiagnostic(ctx, diag, msg)
#else
#define MARU_REPORT_DIAGNOSTIC(ctx, diag, msg) (void)0
#endif

void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventId type, MARU_Window *window, const MARU_Event *event);
void _maru_init_context_base(MARU_Context_Base *ctx_base);
void _maru_drain_queued_events(MARU_Context_Base *ctx_base);
void _maru_update_context_base(MARU_Context_Base *ctx_base, uint64_t field_mask, const MARU_ContextAttributes *attributes);
void _maru_cleanup_context_base(MARU_Context_Base *ctx_base);
void _maru_register_window(MARU_Context_Base *ctx_base, MARU_Window *window);
void _maru_unregister_window(MARU_Context_Base *ctx_base, MARU_Window *window);
uint32_t _maru_cursor_frame_delay_ms(uint32_t delay_ms);
bool _maru_register_animated_cursor(MARU_Cursor_Base *cursor, uint32_t frame_count,
                                    const uint32_t *frame_delays_ms,
                                    const MARU_CursorAnimationCallbacks *callbacks,
                                    uint64_t now_ms);
void _maru_unregister_animated_cursor(MARU_Cursor_Base *cursor);
bool _maru_advance_animated_cursors(MARU_Context_Base *ctx_base, uint64_t now_ms);
uint32_t _maru_adjust_timeout_for_cursor_animation(const MARU_Context_Base *ctx_base,
                                                   uint32_t timeout_ms,
                                                   uint64_t now_ms);

/** @brief Posts an event to the internal thread-safe queue. 
 *
 *  This should ONLY be used for events coming from other threads, or for user-posted
 *  events. System events originating from the main event loop SHOULD be dispatched
 *  directly using _maru_dispatch_event().
 */
MARU_Status _maru_post_event_internal(MARU_Context_Base *ctx_base, MARU_EventId type,
                                      MARU_Window *window, const MARU_Event *evt);

void _maru_monitor_free(MARU_Monitor_Base *monitor);
void _maru_controller_free(MARU_Controller_Base *controller);

#ifdef __cplusplus
}
#endif

#endif // MARU_INTERNAL_H_INCLUDED
