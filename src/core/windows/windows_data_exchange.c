// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_mem_internal.h"
#include "windows_internal.h"
#include <shlwapi.h>

// Helper to map MIME types to Windows clipboard formats
static UINT _maru_mime_to_clipboard_format(const char *mime_type) {
  if (strcmp(mime_type, "text/plain") == 0 ||
      strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
      strcmp(mime_type, "UTF8_STRING") == 0) {
    return CF_UNICODETEXT;
  }
  return RegisterClipboardFormatA(mime_type);
}

// Helper to map Windows clipboard formats to MIME types
const char *_maru_clipboard_format_to_mime(UINT format) {
  switch (format) {
  case CF_UNICODETEXT:
    return "text/plain";
  case CF_TEXT:
    return "text/plain;charset=ascii";
  default: {
    char name[256];
    if (GetClipboardFormatNameA(format, name, sizeof(name))) {
      // This is a bit dangerous due to the static buffer, but
      // maru_getAvailableMIMETypes documentation says it's only valid
      // until the next call to maru_pumpEvents.
      // Better would be to have a per-context or per-window cache.
      return name;
    }
    return NULL;
  }
  }
}

static void _maru_windows_emit_data_received(MARU_Context_Base *ctx_base,
                                             MARU_Window *window,
                                             MARU_DataExchangeTarget target,
                                             const char *mime_type,
                                             void *user_tag, MARU_Status status,
                                             const void *data, size_t size) {
  MARU_Event evt = {0};
  evt.data_received.user_tag = user_tag;
  evt.data_received.target = target;
  evt.data_received.mime_type = mime_type;
  evt.data_received.status = status;
  evt.data_received.data = data;
  evt.data_received.size = size;
  _maru_dispatch_event(ctx_base, MARU_EVENT_DATA_RECEIVED, window, &evt);
}

static char *_maru_windows_copy_string(MARU_Context_Base *ctx_base,
                                       const char *text) {
  if (!text) {
    return NULL;
  }
  const size_t len = strlen(text);
  char *copy = (char *)maru_context_alloc(ctx_base, len + 1u);
  if (!copy) {
    return NULL;
  }
  memcpy(copy, text, len + 1u);
  return copy;
}

static void
_maru_windows_enqueue_deferred_event(MARU_Context_Windows *ctx,
                                     MARU_WindowsDeferredEvent *evt) {
  if (!ctx || !evt) {
    return;
  }
  evt->next = NULL;
  if (ctx->deferred_event_tail) {
    ctx->deferred_event_tail->next = evt;
  } else {
    ctx->deferred_event_head = evt;
  }
  ctx->deferred_event_tail = evt;
}

void _maru_windows_drain_deferred_events(MARU_Context_Windows *ctx) {
  while (ctx && ctx->deferred_event_head) {
    MARU_WindowsDeferredEvent *evt = ctx->deferred_event_head;
    ctx->deferred_event_head = evt->next;
    if (!ctx->deferred_event_head) {
      ctx->deferred_event_tail = NULL;
    }

    if (evt->kind == MARU_WINDOWS_DEFERRED_EVENT_DATA_REQUESTED) {
      MARU_WindowsDataRequestHandle *handle = evt->request_handle;
      if (handle) {
        MARU_Event req_evt = {0};
        req_evt.data_requested.target = handle->target;
        req_evt.data_requested.mime_type = handle->mime_type;
        req_evt.data_requested.internal_handle = handle;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_DATA_REQUESTED,
                             handle->window, &req_evt);

        _maru_windows_emit_data_received(
            &ctx->base, handle->window, handle->target, handle->mime_type,
            handle->user_tag, handle->status, handle->provided_data,
            handle->provided_size);
        if (handle->provided_data) {
          maru_context_free(&ctx->base, handle->provided_data);
        }
        if (handle->mime_type) {
          maru_context_free(&ctx->base, handle->mime_type);
        }
        maru_context_free(&ctx->base, handle);
      }
    } else if (evt->kind == MARU_WINDOWS_DEFERRED_EVENT_DATA_RECEIVED) {
      _maru_windows_emit_data_received(
          &ctx->base, evt->window, evt->received.target,
          evt->received.mime_type, evt->received.user_tag, evt->received.status,
          evt->received.data, evt->received.size);
      if (evt->received.data) {
        maru_context_free(&ctx->base, (void *)evt->received.data);
      }
      if (evt->received.mime_type) {
        maru_context_free(&ctx->base, evt->received.mime_type);
      }
    }

    maru_context_free(&ctx->base, evt);
  }
}

MARU_Status maru_announceData_Windows(MARU_Window *window,
                                      MARU_DataExchangeTarget target,
                                      const char **mime_types, uint32_t count,
                                      MARU_DropActionMask allowed_actions) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return MARU_FAILURE;
  }

  MARU_Window_Windows *win = (MARU_Window_Windows *)window;

  if (!OpenClipboard(win->hwnd)) {
    return MARU_FAILURE;
  }

  if (!EmptyClipboard()) {
    CloseClipboard();
    return MARU_FAILURE;
  }

  for (uint32_t i = 0; i < count; ++i) {
    UINT format = _maru_mime_to_clipboard_format(mime_types[i]);
    if (format == 0 || !SetClipboardData(format, NULL)) {
      CloseClipboard();
      return MARU_FAILURE;
    }
  }

  CloseClipboard();
  return MARU_SUCCESS;
}

MARU_Status maru_provideData_Windows(const MARU_DataRequestEvent *request_event,
                                     const void *data, size_t size,
                                     MARU_DataProvideFlags flags) {
  (void)flags;
  if (request_event->target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return MARU_FAILURE;
  }

  MARU_WindowsDataRequestHandle *handle =
      (MARU_WindowsDataRequestHandle *)request_event->internal_handle;
  if (!handle || !handle->base.ctx_base) {
    return MARU_FAILURE;
  }

  if (handle->kind == MARU_WINDOWS_DATA_REQUEST_CAPTURE_LOCAL) {
    if (handle->provided_data) {
      maru_context_free(handle->base.ctx_base, handle->provided_data);
      handle->provided_data = NULL;
    }
    handle->provided_size = 0;
    handle->status = MARU_FAILURE;

    if (size > 0) {
      handle->provided_data = maru_context_alloc(handle->base.ctx_base, size);
      if (!handle->provided_data) {
        return MARU_FAILURE;
      }
      memcpy(handle->provided_data, data, size);
    }

    handle->provided_size = size;
    handle->status = MARU_SUCCESS;
    return MARU_SUCCESS;
  }

  UINT format = handle->format;
  HGLOBAL hGlobal = NULL;

  if (format == CF_UNICODETEXT) {
    // Convert UTF-8 to UTF-16
    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, (const char *)data, (int)size, NULL, 0);
    if (wlen > 0) {
      hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(WCHAR));
      if (hGlobal) {
        WCHAR *wstr = (WCHAR *)GlobalLock(hGlobal);
        MultiByteToWideChar(CP_UTF8, 0, (const char *)data, (int)size, wstr,
                            wlen);
        wstr[wlen] = L'\0';
        GlobalUnlock(hGlobal);
      }
    }
  } else {
    hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hGlobal) {
      void *ptr = GlobalLock(hGlobal);
      memcpy(ptr, data, size);
      GlobalUnlock(hGlobal);
    }
  }

  if (hGlobal) {
    if (SetClipboardData(format, hGlobal)) {
      return MARU_SUCCESS;
    }
    GlobalFree(hGlobal);
  }

  return MARU_FAILURE;
}

MARU_Status maru_requestData_Windows(MARU_Window *window,
                                     MARU_DataExchangeTarget target,
                                     const char *mime_type, void *user_tag) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return MARU_FAILURE;
  }

  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  MARU_Context_Base *ctx_base = win->base.ctx_base;
  UINT format = _maru_mime_to_clipboard_format(mime_type);

  if (GetClipboardOwner() == win->hwnd) {
    MARU_Context_Windows *ctx = (MARU_Context_Windows *)ctx_base;
    MARU_WindowsDataRequestHandle *handle =
        (MARU_WindowsDataRequestHandle *)maru_context_alloc(ctx_base,
                                                            sizeof(*handle));
    MARU_WindowsDeferredEvent *deferred = NULL;
    if (!handle) {
      return MARU_FAILURE;
    }
    memset(handle, 0, sizeof(*handle));
    handle->base.ctx_base = ctx_base;
    handle->kind = MARU_WINDOWS_DATA_REQUEST_CAPTURE_LOCAL;
    handle->format = format;
    handle->window = window;
    handle->target = target;
    handle->user_tag = user_tag;
    handle->status = MARU_FAILURE;
    handle->mime_type = _maru_windows_copy_string(ctx_base, mime_type);
    if (!handle->mime_type) {
      maru_context_free(ctx_base, handle);
      return MARU_FAILURE;
    }

    if (ctx_base->pump_ctx) {
      MARU_Event req_evt = {0};
      req_evt.data_requested.target = target;
      req_evt.data_requested.mime_type = handle->mime_type;
      req_evt.data_requested.internal_handle = handle;
      _maru_dispatch_event(ctx_base, MARU_EVENT_DATA_REQUESTED, window,
                           &req_evt);

      _maru_windows_emit_data_received(
          ctx_base, window, target, handle->mime_type, user_tag, handle->status,
          handle->provided_data, handle->provided_size);
      if (handle->provided_data) {
        maru_context_free(ctx_base, handle->provided_data);
      }
      maru_context_free(ctx_base, handle->mime_type);
      maru_context_free(ctx_base, handle);
      return MARU_SUCCESS;
    }

    deferred = (MARU_WindowsDeferredEvent *)maru_context_alloc(
        ctx_base, sizeof(*deferred));
    if (!deferred) {
      maru_context_free(ctx_base, handle->mime_type);
      maru_context_free(ctx_base, handle);
      return MARU_FAILURE;
    }
    memset(deferred, 0, sizeof(*deferred));
    deferred->kind = MARU_WINDOWS_DEFERRED_EVENT_DATA_REQUESTED;
    deferred->window = window;
    deferred->request_handle = handle;
    _maru_windows_enqueue_deferred_event(ctx, deferred);
    return maru_wakeContext((MARU_Context *)ctx);
  }

  if (!OpenClipboard(win->hwnd)) {
    return MARU_FAILURE;
  }

  HANDLE hData = GetClipboardData(format);
  MARU_Status status = MARU_FAILURE;
  const void *event_data = NULL;
  size_t event_size = 0;

  if (hData) {
    void *ptr = GlobalLock(hData);
    if (ptr) {
      size_t size = GlobalSize(hData);
      if (format == CF_UNICODETEXT) {
        // Convert UTF-16 to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)ptr, -1, NULL,
                                      0, NULL, NULL);
        if (len > 0) {
          char *utf8 = maru_context_alloc(ctx_base, len);
          if (utf8) {
            WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)ptr, -1, utf8, len,
                                NULL, NULL);
            event_data = utf8;
            event_size = (size_t)len - 1; // Exclude null terminator
            status = MARU_SUCCESS;
          }
        }
      } else {
        void *data_copy = maru_context_alloc(ctx_base, size);
        if (data_copy) {
          memcpy(data_copy, ptr, size);
          event_data = data_copy;
          event_size = size;
          status = MARU_SUCCESS;
        }
      }
      GlobalUnlock(hData);
    }
  }

  CloseClipboard();

  if (ctx_base->pump_ctx) {
    _maru_windows_emit_data_received(ctx_base, window, target, mime_type,
                                     user_tag, status, event_data, event_size);
    if (event_data) {
      maru_context_free(ctx_base, (void *)event_data);
    }
  } else {
    MARU_Context_Windows *ctx = (MARU_Context_Windows *)ctx_base;
    MARU_WindowsDeferredEvent *deferred =
        (MARU_WindowsDeferredEvent *)maru_context_alloc(ctx_base,
                                                        sizeof(*deferred));
    char *mime_copy = _maru_windows_copy_string(ctx_base, mime_type);
    if (!deferred || !mime_copy) {
      if (deferred) {
        maru_context_free(ctx_base, deferred);
      }
      if (mime_copy) {
        maru_context_free(ctx_base, mime_copy);
      }
      if (event_data) {
        maru_context_free(ctx_base, (void *)event_data);
      }
      return MARU_FAILURE;
    }
    memset(deferred, 0, sizeof(*deferred));
    deferred->kind = MARU_WINDOWS_DEFERRED_EVENT_DATA_RECEIVED;
    deferred->window = window;
    deferred->received.target = target;
    deferred->received.mime_type = mime_copy;
    deferred->received.user_tag = user_tag;
    deferred->received.data = event_data;
    deferred->received.size = event_size;
    deferred->received.status = status;
    _maru_windows_enqueue_deferred_event(ctx, deferred);
    return maru_wakeContext((MARU_Context *)ctx);
  }

  return MARU_SUCCESS;
}

MARU_Status maru_getAvailableMIMETypes_Windows(MARU_Window *window,
                                               MARU_DataExchangeTarget target,
                                               MARU_MIMETypeList *out_list) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD || !out_list) {
    return MARU_FAILURE;
  }

  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  if (!OpenClipboard(win->hwnd)) {
    return MARU_FAILURE;
  }

  uint32_t count = CountClipboardFormats();
  if (count == 0) {
    CloseClipboard();
    out_list->count = 0;
    out_list->mime_types = NULL;
    return MARU_SUCCESS;
  }

  if (ctx->clipboard_mime_query_storage) {
    maru_context_free(&ctx->base, ctx->clipboard_mime_query_storage);
    ctx->clipboard_mime_query_storage = NULL;
  }
  if (ctx->clipboard_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard_mime_query_ptr);
    ctx->clipboard_mime_query_ptr = NULL;
  }

  const char **mimes = maru_context_alloc(&ctx->base, count * sizeof(char *));
  char *storage = maru_context_alloc(&ctx->base, (size_t)count * 256u);
  if (!mimes || !storage) {
    if (mimes) {
      maru_context_free(&ctx->base, (void *)mimes);
    }
    if (storage) {
      maru_context_free(&ctx->base, storage);
    }
    CloseClipboard();
    return MARU_FAILURE;
  }

  uint32_t actual_count = 0;
  UINT format = 0;
  char *cursor = storage;
  while ((format = EnumClipboardFormats(format)) != 0) {
    const char *mime = _maru_clipboard_format_to_mime(format);
    if (mime) {
      size_t len = strlen(mime);
      memcpy(cursor, mime, len + 1u);
      mimes[actual_count++] = cursor;
      cursor += len + 1u;
    }
  }

  CloseClipboard();

  ctx->clipboard_mime_query_storage = storage;
  ctx->clipboard_mime_query_ptr = mimes;
  ctx->clipboard_mime_query_count = actual_count;

  out_list->count = actual_count;
  out_list->mime_types = mimes;

  return MARU_SUCCESS;
}
