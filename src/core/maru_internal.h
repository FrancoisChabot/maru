#ifndef MARU_INTERNAL_H_INCLUDED
#define MARU_INTERNAL_H_INCLUDED

#include <stdint.h>
#include "maru/maru.h"
#include "maru_backend.h"

// The API headers define MARU_ContextExposed and MARU_WindowExposed
#include "maru/c/details/contexts.h"
#include "maru/c/details/windows.h"

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

typedef struct MARU_Context_Base {
  MARU_ContextExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif
  
  MARU_Allocator allocator;

  MARU_DiagnosticCallback diagnostic_cb;
  void *diagnostic_userdata;

  MARU_ContextTuning tuning;

  MARU_EventCallback event_cb;
  void *event_userdata;
  MARU_EventMask event_mask;

#ifdef MARU_VALIDATE_API_CALLS
  MARU_ThreadId creator_thread;
#endif
  // Internal common state will go here
} MARU_Context_Base;

void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
void _maru_dispatch_event(MARU_Context_Base *ctx, MARU_EventType type, MARU_Window *window, const MARU_Event *event);

typedef struct MARU_Window_Base {
  MARU_WindowExposed pub;
#ifdef MARU_INDIRECT_BACKEND
  const struct MARU_Backend *backend;
#endif

  MARU_Context_Base *ctx_base;
  // Internal common state will go here
} MARU_Window_Base;

#ifdef __cplusplus
}
#endif

#endif // MARU_INTERNAL_H_INCLUDED
