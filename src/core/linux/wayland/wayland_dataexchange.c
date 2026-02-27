// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#define _GNU_SOURCE
#include "wayland_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maru_mem_internal.h"

#define MARU_WL_SEAT_CAPABILITY_POINTER 1u
#define MARU_WL_SEAT_CAPABILITY_KEYBOARD 2u
#define MARU_WL_DATA_REQUEST_MAGIC 0x4d445852u /* "MDXR" */

typedef struct MARU_WaylandDataRequestHandle {
  MARU_DataRequestHandleBase base;
  uint32_t magic;
  int fd;
  MARU_DataExchangeTarget target;
  const char *mime_type;
  MARU_Window *window;
  bool consumed;
} MARU_WaylandDataRequestHandle;

static uint32_t _maru_wl_parse_uri_list(MARU_Context_WL *ctx, const char *data,
                                        size_t size, const char ***out_paths) {
  uint32_t count = 0;
  const char *curr = data;
  const char *end = data + size;

  while (curr < end) {
    const char *line_end = (const char *)memchr(curr, '\n', (size_t)(end - curr));
    if (!line_end) line_end = end;

    const char *line_start = curr;
    while (line_start < line_end && (*line_start == ' ' || *line_start == '\t')) {
      line_start++;
    }

    if (line_start < line_end && *line_start != '#') {
      count++;
    }
    curr = (line_end < end) ? line_end + 1 : end;
  }

  if (count == 0) {
    *out_paths = NULL;
    return 0;
  }

  const char **paths =
      (const char **)maru_context_alloc(&ctx->base, sizeof(char *) * count);
  if (!paths) {
    *out_paths = NULL;
    return 0;
  }

  uint32_t idx = 0;
  curr = data;
  while (curr < end && idx < count) {
    const char *line_end = (const char *)memchr(curr, '\n', (size_t)(end - curr));
    if (!line_end) line_end = end;

    const char *line_start = curr;
    while (line_start < line_end && (*line_start == ' ' || *line_start == '\t')) {
      line_start++;
    }

    const char *content_end = line_end;
    while (content_end > line_start &&
           (content_end[-1] == '\r' || content_end[-1] == ' ' ||
            content_end[-1] == '\t')) {
      content_end--;
    }

    if (line_start < content_end && *line_start != '#') {
      const char *path_start = line_start;
      if (content_end - line_start >= 7 &&
          memcmp(line_start, "file://", 7) == 0) {
        path_start += 7;
      }

      const size_t path_len = (size_t)(content_end - path_start);
      char *copy = (char *)maru_context_alloc(&ctx->base, path_len + 1);
      if (copy) {
        memcpy(copy, path_start, path_len);
        copy[path_len] = '\0';
        paths[idx++] = copy;
      }
    }
    curr = (line_end < end) ? line_end + 1 : end;
  }

  *out_paths = paths;
  return idx;
}

static const char *_maru_wl_copy_string(MARU_Context_Base *ctx_base,
                                        const char *text) {
  if (!text) return NULL;
  const size_t len = strlen(text);
  char *copy = (char *)maru_context_alloc(ctx_base, len + 1u);
  if (!copy) return NULL;
  memcpy(copy, text, len + 1u);
  return copy;
}

static void _maru_wl_clear_mime_query(MARU_Context_WL *ctx) {
  if (ctx->clipboard.mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard.mime_query_ptr);
    ctx->clipboard.mime_query_ptr = NULL;
  }
  ctx->clipboard.mime_query_count = 0;
}

static void _maru_wl_clear_primary_mime_query(MARU_Context_WL *ctx) {
  if (ctx->primary_selection.mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->primary_selection.mime_query_ptr);
    ctx->primary_selection.mime_query_ptr = NULL;
  }
  ctx->primary_selection.mime_query_count = 0;
}

static void _maru_wl_clear_dnd_mime_query(MARU_Context_WL *ctx) {
  if (ctx->clipboard.dnd_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard.dnd_mime_query_ptr);
    ctx->clipboard.dnd_mime_query_ptr = NULL;
  }
  ctx->clipboard.dnd_mime_query_count = 0;
}

static void _maru_wl_clear_announced_mimes(MARU_Context_WL *ctx) {
  for (uint32_t i = 0; i < ctx->clipboard.announced_mime_count; ++i) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard.announced_mime_types[i]);
  }
  maru_context_free(&ctx->base, (void *)ctx->clipboard.announced_mime_types);
  ctx->clipboard.announced_mime_types = NULL;
  ctx->clipboard.announced_mime_count = 0;
  ctx->clipboard.announced_mime_capacity = 0;
}

static void _maru_wl_clear_primary_announced_mimes(MARU_Context_WL *ctx) {
  for (uint32_t i = 0; i < ctx->primary_selection.announced_mime_count; ++i) {
    maru_context_free(&ctx->base,
                      (void *)ctx->primary_selection.announced_mime_types[i]);
  }
  maru_context_free(&ctx->base, (void *)ctx->primary_selection.announced_mime_types);
  ctx->primary_selection.announced_mime_types = NULL;
  ctx->primary_selection.announced_mime_count = 0;
  ctx->primary_selection.announced_mime_capacity = 0;
}

static void _maru_wl_clear_dnd_announced_mimes(MARU_Context_WL *ctx) {
  for (uint32_t i = 0; i < ctx->clipboard.dnd_announced_mime_count; ++i) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard.dnd_announced_mime_types[i]);
  }
  maru_context_free(&ctx->base, (void *)ctx->clipboard.dnd_announced_mime_types);
  ctx->clipboard.dnd_announced_mime_types = NULL;
  ctx->clipboard.dnd_announced_mime_count = 0;
  ctx->clipboard.dnd_announced_mime_capacity = 0;
}

static void _maru_wl_clear_read_cache(MARU_Context_WL *ctx) {
  ctx->clipboard.read_cache_size = 0;
}

static void _maru_wl_clear_primary_read_cache(MARU_Context_WL *ctx) {
  ctx->primary_selection.read_cache_size = 0;
}

static bool _maru_wl_set_announced_mimes(MARU_Context_WL *ctx,
                                         const char **mime_types, uint32_t count) {
  _maru_wl_clear_announced_mimes(ctx);
  if (count == 0) return true;

  const char **list =
      (const char **)maru_context_alloc(&ctx->base, sizeof(char *) * count);
  if (!list) return false;
  memset((void *)list, 0, sizeof(char *) * count);

  for (uint32_t i = 0; i < count; ++i) {
    list[i] = _maru_wl_copy_string(&ctx->base, mime_types[i]);
    if (!list[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        maru_context_free(&ctx->base, (void *)list[j]);
      }
      maru_context_free(&ctx->base, (void *)list);
      return false;
    }
  }

  ctx->clipboard.announced_mime_types = list;
  ctx->clipboard.announced_mime_count = count;
  ctx->clipboard.announced_mime_capacity = count;
  return true;
}

static bool _maru_wl_set_primary_announced_mimes(MARU_Context_WL *ctx,
                                                 const char **mime_types,
                                                 uint32_t count) {
  _maru_wl_clear_primary_announced_mimes(ctx);
  if (count == 0) return true;

  const char **list =
      (const char **)maru_context_alloc(&ctx->base, sizeof(char *) * count);
  if (!list) return false;
  memset((void *)list, 0, sizeof(char *) * count);

  for (uint32_t i = 0; i < count; ++i) {
    list[i] = _maru_wl_copy_string(&ctx->base, mime_types[i]);
    if (!list[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        maru_context_free(&ctx->base, (void *)list[j]);
      }
      maru_context_free(&ctx->base, (void *)list);
      return false;
    }
  }

  ctx->primary_selection.announced_mime_types = list;
  ctx->primary_selection.announced_mime_count = count;
  ctx->primary_selection.announced_mime_capacity = count;
  return true;
}

static bool _maru_wl_set_dnd_announced_mimes(MARU_Context_WL *ctx,
                                             const char **mime_types,
                                             uint32_t count) {
  _maru_wl_clear_dnd_announced_mimes(ctx);
  if (count == 0) return true;

  const char **list =
      (const char **)maru_context_alloc(&ctx->base, sizeof(char *) * count);
  if (!list) return false;
  memset((void *)list, 0, sizeof(char *) * count);

  for (uint32_t i = 0; i < count; ++i) {
    list[i] = _maru_wl_copy_string(&ctx->base, mime_types[i]);
    if (!list[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        maru_context_free(&ctx->base, (void *)list[j]);
      }
      maru_context_free(&ctx->base, (void *)list);
      return false;
    }
  }

  ctx->clipboard.dnd_announced_mime_types = list;
  ctx->clipboard.dnd_announced_mime_count = count;
  ctx->clipboard.dnd_announced_mime_capacity = count;
  return true;
}

static MARU_WaylandDataOfferMeta *_maru_wl_find_offer_meta(MARU_Context_WL *ctx,
                                                            struct wl_data_offer *offer) {
  for (MARU_WaylandDataOfferMeta *it = ctx->clipboard.offer_metas; it;
       it = it->next) {
    if (it->offer == offer) return it;
  }
  return NULL;
}

static MARU_WaylandPrimaryOfferMeta *_maru_wl_find_primary_offer_meta(
    MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *offer) {
  for (MARU_WaylandPrimaryOfferMeta *it = ctx->primary_selection.offer_metas; it;
       it = it->next) {
    if (it->offer == offer) return it;
  }
  return NULL;
}

static void _maru_wl_destroy_offer_meta(MARU_Context_WL *ctx,
                                        MARU_WaylandDataOfferMeta *meta) {
  if (!meta) return;
  if (meta->offer) {
    maru_wl_data_offer_destroy(ctx, meta->offer);
  }
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    maru_context_free(&ctx->base, (void *)meta->mime_types[i]);
  }
  maru_context_free(&ctx->base, (void *)meta->mime_types);
  maru_context_free(&ctx->base, meta);
}

static void _maru_wl_destroy_primary_offer_meta(MARU_Context_WL *ctx,
                                                MARU_WaylandPrimaryOfferMeta *meta) {
  if (!meta) return;
  if (meta->offer) {
    maru_zwp_primary_selection_offer_v1_destroy(ctx, meta->offer);
  }
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    maru_context_free(&ctx->base, (void *)meta->mime_types[i]);
  }
  maru_context_free(&ctx->base, (void *)meta->mime_types);
  maru_context_free(&ctx->base, meta);
}

static void _maru_wl_remove_offer_meta(MARU_Context_WL *ctx,
                                       struct wl_data_offer *offer) {
  MARU_WaylandDataOfferMeta **link = &ctx->clipboard.offer_metas;
  while (*link) {
    MARU_WaylandDataOfferMeta *curr = *link;
    if (curr->offer == offer) {
      *link = curr->next;
      _maru_wl_destroy_offer_meta(ctx, curr);
      return;
    }
    link = &curr->next;
  }
}

static void _maru_wl_remove_primary_offer_meta(
    MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *offer) {
  MARU_WaylandPrimaryOfferMeta **link = &ctx->primary_selection.offer_metas;
  while (*link) {
    MARU_WaylandPrimaryOfferMeta *curr = *link;
    if (curr->offer == offer) {
      *link = curr->next;
      _maru_wl_destroy_primary_offer_meta(ctx, curr);
      return;
    }
    link = &curr->next;
  }
}

static void _maru_wl_clear_offer_metas(MARU_Context_WL *ctx) {
  MARU_WaylandDataOfferMeta *it = ctx->clipboard.offer_metas;
  ctx->clipboard.offer_metas = NULL;
  while (it) {
    MARU_WaylandDataOfferMeta *next = it->next;
    _maru_wl_destroy_offer_meta(ctx, it);
    it = next;
  }
  ctx->clipboard.offer = NULL;
}

static void _maru_wl_clear_primary_offer_metas(MARU_Context_WL *ctx) {
  MARU_WaylandPrimaryOfferMeta *it = ctx->primary_selection.offer_metas;
  ctx->primary_selection.offer_metas = NULL;
  while (it) {
    MARU_WaylandPrimaryOfferMeta *next = it->next;
    _maru_wl_destroy_primary_offer_meta(ctx, it);
    it = next;
  }
  ctx->primary_selection.offer = NULL;
}

static bool _maru_wl_offer_meta_append_mime(MARU_Context_WL *ctx,
                                            MARU_WaylandDataOfferMeta *meta,
                                            const char *mime_type) {
  if (!meta || !mime_type) return false;
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    if (strcmp(meta->mime_types[i], mime_type) == 0) return true;
  }
  if (meta->mime_count == meta->mime_capacity) {
    uint32_t next_capacity = (meta->mime_capacity == 0) ? 8u : meta->mime_capacity * 2u;
    const char **next =
        (const char **)maru_context_realloc(&ctx->base, (void *)meta->mime_types,
                                            sizeof(char *) * meta->mime_capacity,
                                            sizeof(char *) * next_capacity);
    if (!next) return false;
    meta->mime_types = next;
    meta->mime_capacity = next_capacity;
  }
  const char *copy = _maru_wl_copy_string(&ctx->base, mime_type);
  if (!copy) return false;
  meta->mime_types[meta->mime_count++] = copy;
  return true;
}

static bool _maru_wl_primary_offer_meta_append_mime(
    MARU_Context_WL *ctx, MARU_WaylandPrimaryOfferMeta *meta,
    const char *mime_type) {
  if (!meta || !mime_type) return false;
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    if (strcmp(meta->mime_types[i], mime_type) == 0) return true;
  }
  if (meta->mime_count == meta->mime_capacity) {
    uint32_t next_capacity = (meta->mime_capacity == 0) ? 8u : meta->mime_capacity * 2u;
    const char **next = (const char **)maru_context_realloc(
        &ctx->base, (void *)meta->mime_types, sizeof(char *) * meta->mime_capacity,
        sizeof(char *) * next_capacity);
    if (!next) return false;
    meta->mime_types = next;
    meta->mime_capacity = next_capacity;
  }
  const char *copy = _maru_wl_copy_string(&ctx->base, mime_type);
  if (!copy) return false;
  meta->mime_types[meta->mime_count++] = copy;
  return true;
}

static bool _maru_wl_offer_has_mime(MARU_Context_WL *ctx,
                                    struct wl_data_offer *offer,
                                    const char *mime_type) {
  MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, offer);
  if (!meta || !mime_type) return false;
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    if (strcmp(meta->mime_types[i], mime_type) == 0) return true;
  }
  return false;
}

static bool _maru_wl_primary_offer_has_mime(
    MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *offer,
    const char *mime_type) {
  MARU_WaylandPrimaryOfferMeta *meta = _maru_wl_find_primary_offer_meta(ctx, offer);
  if (!meta || !mime_type) return false;
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    if (strcmp(meta->mime_types[i], mime_type) == 0) return true;
  }
  return false;
}

static bool _maru_wl_flush_or_mark_lost(MARU_Context_WL *ctx, const char *error_msg) {
  if (maru_wl_display_flush(ctx, ctx->wl.display) < 0 && errno != EAGAIN) {
    ctx->base.pub.flags |= MARU_CONTEXT_STATE_LOST;
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           error_msg);
    return false;
  }
  if (maru_wl_display_get_error(ctx, ctx->wl.display) != 0) {
    ctx->base.pub.flags |= MARU_CONTEXT_STATE_LOST;
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_BACKEND_FAILURE,
                           "Wayland display reported a fatal protocol/connection error");
    return false;
  }
  return true;
}

static bool _maru_wl_dataexchange_target_supported(MARU_Context_WL *ctx,
                                                   MARU_DataExchangeTarget target) {
  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD ||
      target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    if (!ctx->protocols.opt.wl_data_device_manager) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD
                                 ? "Clipboard requires wl_data_device_manager"
                                 : "Drag and drop requires wl_data_device_manager");
      return false;
    }
    return true;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    if (!ctx->protocols.opt.zwp_primary_selection_device_manager_v1) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
                             "Primary selection requires zwp_primary_selection_device_manager_v1");
      return false;
    }
    return true;
  }
  return false;
}

static void _maru_wl_dataexchange_ensure_seat_devices(MARU_Context_WL *ctx) {
  if (!ctx->wl.seat) return;
  if ((ctx->protocols.opt.wl_data_device_manager && !ctx->clipboard.device) ||
      (ctx->protocols.opt.zwp_primary_selection_device_manager_v1 &&
       !ctx->primary_selection.device)) {
    _maru_wayland_dataexchange_onSeatCapabilities(
        ctx, ctx->wl.seat,
        MARU_WL_SEAT_CAPABILITY_POINTER | MARU_WL_SEAT_CAPABILITY_KEYBOARD);
  }
}

static void _clipboard_source_target(void *data, struct wl_data_source *source,
                                     const char *mime_type) {
  (void)data;
  (void)source;
  (void)mime_type;
}

static void _clipboard_source_dnd_drop_performed(void *data,
                                                 struct wl_data_source *source) {
  (void)data;
  (void)source;
}

static void _clipboard_source_dnd_finished(void *data,
                                           struct wl_data_source *source) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || ctx->clipboard.dnd_source != source) return;

  MARU_Event evt = {0};
  // TODO: we should probably report the final action here.
  evt.drag_finished.action = MARU_DROP_ACTION_NONE;

  MARU_Window *event_window = ctx->linux_common.xkb.focused_window;
  if (!event_window) event_window = ctx->linux_common.pointer.focused_window;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_DRAG_FINISHED, event_window, &evt);

  ctx->clipboard.dnd_source = NULL;
  maru_wl_data_source_destroy(ctx, source);
  _maru_wl_clear_dnd_announced_mimes(ctx);
}

static void _clipboard_source_action(void *data, struct wl_data_source *source,
                                     uint32_t dnd_action) {
  (void)data;
  (void)source;
  (void)dnd_action;
}

static void _maru_wl_destroy_request_handle(MARU_Context_WL *ctx,
                                            MARU_WaylandDataRequestHandle *handle) {
  if (!handle) return;
  if (handle->fd >= 0) {
    close(handle->fd);
    handle->fd = -1;
  }
  maru_context_free(&ctx->base, (void *)handle->mime_type);
  maru_context_free(&ctx->base, handle);
}

static void _clipboard_source_send(void *data, struct wl_data_source *source,
                                   const char *mime_type, int32_t fd) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || fd < 0 || !mime_type) {
    if (fd >= 0) close(fd);
    return;
  }

  MARU_DataExchangeTarget target = MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
  if (ctx->clipboard.dnd_source == source) {
    target = MARU_DATA_EXCHANGE_TARGET_DRAG_DROP;
  }

  MARU_WaylandDataRequestHandle *handle =
      (MARU_WaylandDataRequestHandle *)maru_context_alloc(&ctx->base, sizeof(*handle));
  if (!handle) {
    close(fd);
    return;
  }
  memset(handle, 0, sizeof(*handle));
  
  MARU_Window *event_window = ctx->linux_common.xkb.focused_window;
  if (!event_window) event_window = ctx->linux_common.pointer.focused_window;

  handle->base.ctx_base = &ctx->base;
  handle->magic = MARU_WL_DATA_REQUEST_MAGIC;
  handle->fd = fd;
  handle->target = target;
  handle->window = event_window;
  handle->mime_type = _maru_wl_copy_string(&ctx->base, mime_type);
  if (!handle->mime_type) {
    _maru_wl_destroy_request_handle(ctx, handle);
    return;
  }

  MARU_Event evt = {0};
  evt.data_requested.target = target;
  evt.data_requested.mime_type = handle->mime_type;
  evt.data_requested.internal_handle = handle;

  _maru_dispatch_event(&ctx->base, MARU_EVENT_DATA_REQUESTED, event_window, &evt);

  _maru_wl_destroy_request_handle(ctx, handle);
}

static void _primary_selection_source_send(
    void *data, struct zwp_primary_selection_source_v1 *source,
    const char *mime_type, int32_t fd) {
  (void)source;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || fd < 0 || !mime_type) {
    if (fd >= 0) close(fd);
    return;
  }

  MARU_WaylandDataRequestHandle *handle =
      (MARU_WaylandDataRequestHandle *)maru_context_alloc(&ctx->base, sizeof(*handle));
  if (!handle) {
    close(fd);
    return;
  }
  memset(handle, 0, sizeof(*handle));
  
  MARU_Window *event_window = ctx->linux_common.xkb.focused_window;
  if (!event_window) event_window = ctx->linux_common.pointer.focused_window;

  handle->base.ctx_base = &ctx->base;
  handle->magic = MARU_WL_DATA_REQUEST_MAGIC;
  handle->fd = fd;
  handle->target = MARU_DATA_EXCHANGE_TARGET_PRIMARY;
  handle->window = event_window;
  handle->mime_type = _maru_wl_copy_string(&ctx->base, mime_type);
  if (!handle->mime_type) {
    _maru_wl_destroy_request_handle(ctx, handle);
    return;
  }

  MARU_Event evt = {0};
  evt.data_requested.target = MARU_DATA_EXCHANGE_TARGET_PRIMARY;
  evt.data_requested.mime_type = handle->mime_type;
  evt.data_requested.internal_handle = handle;

  _maru_dispatch_event(&ctx->base, MARU_EVENT_DATA_REQUESTED, event_window, &evt);

  _maru_wl_destroy_request_handle(ctx, handle);
}

static void _clipboard_source_cancelled(void *data, struct wl_data_source *source) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx) return;
  if (ctx->clipboard.source && ctx->clipboard.source == source) {
    struct wl_data_source *current = ctx->clipboard.source;
    ctx->clipboard.source = NULL;
    maru_wl_data_source_destroy(ctx, current);
    _maru_wl_clear_announced_mimes(ctx);
  } else if (ctx->clipboard.dnd_source && ctx->clipboard.dnd_source == source) {
    struct wl_data_source *current = ctx->clipboard.dnd_source;
    ctx->clipboard.dnd_source = NULL;
    maru_wl_data_source_destroy(ctx, current);
    _maru_wl_clear_dnd_announced_mimes(ctx);
  }
}

static void _primary_selection_source_cancelled(
    void *data, struct zwp_primary_selection_source_v1 *source) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx) return;
  if (ctx->primary_selection.source && ctx->primary_selection.source == source) {
    struct zwp_primary_selection_source_v1 *current = ctx->primary_selection.source;
    ctx->primary_selection.source = NULL;
    maru_zwp_primary_selection_source_v1_destroy(ctx, current);
    _maru_wl_clear_primary_announced_mimes(ctx);
  }
}

static const struct wl_data_source_listener _clipboard_source_listener = {
    .target = _clipboard_source_target,
    .send = _clipboard_source_send,
    .cancelled = _clipboard_source_cancelled,
    .dnd_drop_performed = _clipboard_source_dnd_drop_performed,
    .dnd_finished = _clipboard_source_dnd_finished,
    .action = _clipboard_source_action,
};

static const struct zwp_primary_selection_source_v1_listener
    _primary_selection_source_listener = {
        .send = _primary_selection_source_send,
        .cancelled = _primary_selection_source_cancelled,
};

static void _clipboard_offer_handle_offer(void *data, struct wl_data_offer *offer,
                                          const char *mime_type) {
  MARU_WaylandDataOfferMeta *meta = (MARU_WaylandDataOfferMeta *)data;
  if (!meta || !meta->offer) return;
  MARU_Context_WL *ctx = meta->ctx;
  if (!ctx || !mime_type) return;
  _maru_wl_offer_meta_append_mime(ctx, meta, mime_type);
}

static void _clipboard_offer_handle_source_actions(void *data,
                                                   struct wl_data_offer *offer,
                                                   uint32_t source_actions) {
  (void)data;
  (void)offer;
  (void)source_actions;
}

static void _clipboard_offer_handle_action(void *data, struct wl_data_offer *offer,
                                           uint32_t dnd_action) {
  (void)data;
  (void)offer;
  (void)dnd_action;
}

static const struct wl_data_offer_listener _clipboard_offer_listener = {
    .offer = _clipboard_offer_handle_offer,
    .source_actions = _clipboard_offer_handle_source_actions,
    .action = _clipboard_offer_handle_action,
};

static void _primary_selection_offer_handle_offer(
    void *data, struct zwp_primary_selection_offer_v1 *offer,
    const char *mime_type) {
  MARU_WaylandPrimaryOfferMeta *meta = (MARU_WaylandPrimaryOfferMeta *)data;
  if (!meta || !meta->offer) return;
  MARU_Context_WL *ctx = meta->ctx;
  if (!ctx || !mime_type) return;
  _maru_wl_primary_offer_meta_append_mime(ctx, meta, mime_type);
}

static const struct zwp_primary_selection_offer_v1_listener
    _primary_selection_offer_listener = {
        .offer = _primary_selection_offer_handle_offer,
};

static void _clipboard_data_device_data_offer(void *data,
                                              struct wl_data_device *data_device,
                                              struct wl_data_offer *offer) {
  (void)data_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || !offer) return;

  MARU_WaylandDataOfferMeta *meta =
      (MARU_WaylandDataOfferMeta *)maru_context_alloc(&ctx->base, sizeof(*meta));
  if (!meta) {
    maru_wl_data_offer_destroy(ctx, offer);
    return;
  }
  memset(meta, 0, sizeof(*meta));
  meta->offer = offer;
  meta->ctx = ctx;
  maru_wl_data_offer_add_listener(ctx, offer, &_clipboard_offer_listener, meta);
  meta->next = ctx->clipboard.offer_metas;
  ctx->clipboard.offer_metas = meta;
}

static void _clipboard_data_device_enter(void *data, struct wl_data_device *data_device,
                                         uint32_t serial, struct wl_surface *surface,
                                         wl_fixed_t x, wl_fixed_t y,
                                         struct wl_data_offer *offer) {
  (void)data_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || !surface || !offer) return;

  MARU_Window_WL *window = (MARU_Window_WL *)maru_wl_surface_get_user_data(ctx, surface);
  if (!window || !window->base.attrs_effective.accept_drop) {
    maru_wl_data_offer_accept(ctx, offer, serial, NULL);
    return;
  }

  ctx->clipboard.dnd_offer = offer;
  ctx->clipboard.dnd_serial = serial;
  ctx->clipboard.dnd_session_userdata = NULL;
  ctx->clipboard.dnd_window = window;

  MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, offer);
  if (!meta) return;

  MARU_DropAction action = MARU_DROP_ACTION_NONE;
  MARU_DropFeedback feedback = {
      .action = &action,
      .session_userdata = &ctx->clipboard.dnd_session_userdata,
  };

  MARU_Event evt = {0};
  evt.drop_enter.position.x = (MARU_Scalar)wl_fixed_to_double(x);
  evt.drop_enter.position.y = (MARU_Scalar)wl_fixed_to_double(y);
  evt.drop_enter.feedback = &feedback;
  evt.drop_enter.available_types.mime_types = meta->mime_types;
  evt.drop_enter.available_types.count = meta->mime_count;
  evt.drop_enter.modifiers = _maru_wayland_get_modifiers(ctx);

  _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_ENTERED, (MARU_Window *)window, &evt);

  uint32_t wl_action = 0;
  if (action & MARU_DROP_ACTION_COPY) wl_action |= 1u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY */;
  if (action & MARU_DROP_ACTION_MOVE) wl_action |= 2u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE */;

  if (wl_action != 0) {
    const char *accepted_mime = (meta->mime_count > 0) ? meta->mime_types[0] : NULL;
    maru_wl_data_offer_accept(ctx, offer, serial, accepted_mime);
    maru_wl_data_offer_set_actions(ctx, offer, wl_action, wl_action);
  } else {
    maru_wl_data_offer_accept(ctx, offer, serial, NULL);
  }
}

static void _clipboard_data_device_leave(void *data, struct wl_data_device *data_device) {
  (void)data_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx) return;

  MARU_Window_WL *window = ctx->clipboard.dnd_window;
  if (window) {
    MARU_Event evt = {0};
    evt.drop_leave.session_userdata = &ctx->clipboard.dnd_session_userdata;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_EXITED, (MARU_Window *)window, &evt);
  }
  ctx->clipboard.dnd_offer = NULL;
  ctx->clipboard.dnd_serial = 0;
  ctx->clipboard.dnd_session_userdata = NULL;
  ctx->clipboard.dnd_window = NULL;
}

static void _clipboard_data_device_motion(void *data, struct wl_data_device *data_device,
                                          uint32_t time, wl_fixed_t x,
                                          wl_fixed_t y) {
  (void)data_device;
  (void)time;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || !ctx->clipboard.dnd_offer) return;

  MARU_Window_WL *window = ctx->clipboard.dnd_window;
  if (!window || !window->base.attrs_effective.accept_drop) return;

  MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, ctx->clipboard.dnd_offer);
  if (!meta) return;

  MARU_DropAction action = MARU_DROP_ACTION_NONE;
  MARU_DropFeedback feedback = {
      .action = &action,
      .session_userdata = &ctx->clipboard.dnd_session_userdata,
  };

  MARU_Event evt = {0};
  evt.drop_hover.position.x = (MARU_Scalar)wl_fixed_to_double(x);
  evt.drop_hover.position.y = (MARU_Scalar)wl_fixed_to_double(y);
  evt.drop_hover.feedback = &feedback;
  evt.drop_hover.available_types.mime_types = meta->mime_types;
  evt.drop_hover.available_types.count = meta->mime_count;
  evt.drop_hover.modifiers = _maru_wayland_get_modifiers(ctx);

  _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_HOVERED, (MARU_Window *)window, &evt);

  uint32_t wl_action = 0;
  if (action & MARU_DROP_ACTION_COPY) wl_action |= 1u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY */;
  if (action & MARU_DROP_ACTION_MOVE) wl_action |= 2u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE */;

  if (wl_action != 0) {
    const char *accepted_mime = (meta->mime_count > 0) ? meta->mime_types[0] : NULL;
    maru_wl_data_offer_accept(ctx, ctx->clipboard.dnd_offer, ctx->clipboard.dnd_serial, accepted_mime);
    maru_wl_data_offer_set_actions(ctx, ctx->clipboard.dnd_offer, wl_action, wl_action);
  } else {
    maru_wl_data_offer_accept(ctx, ctx->clipboard.dnd_offer, ctx->clipboard.dnd_serial, NULL);
  }
}

static void _clipboard_data_device_drop(void *data, struct wl_data_device *data_device) {
  (void)data_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || !ctx->clipboard.dnd_offer) return;

  MARU_Window_WL *window = ctx->clipboard.dnd_window;
  if (!window || !window->base.attrs_effective.accept_drop) return;

  MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, ctx->clipboard.dnd_offer);
  if (!meta) return;

  bool has_uri_list = false;
  for (uint32_t i = 0; i < meta->mime_count; ++i) {
    if (strcmp(meta->mime_types[i], "text/uri-list") == 0) {
      has_uri_list = true;
      break;
    }
  }

  if (has_uri_list) {
    ctx->clipboard.dnd_drop.pending = true;
    ctx->clipboard.dnd_drop.position.x = ctx->linux_common.pointer.x;
    ctx->clipboard.dnd_drop.position.y = ctx->linux_common.pointer.y;
    ctx->clipboard.dnd_drop.modifiers = _maru_wayland_get_modifiers(ctx);
    ctx->clipboard.dnd_drop.offer = ctx->clipboard.dnd_offer;
    
    // We use (void*)1 as a special user_tag to identify internal pre-fetch
    maru_requestData_WL((MARU_Window *)window, MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                         "text/uri-list", (void *)1);
  } else {
    MARU_Event evt = {0};
    evt.drop.position.x = ctx->linux_common.pointer.x;
    evt.drop.position.y = ctx->linux_common.pointer.y;
    evt.drop.session_userdata = &ctx->clipboard.dnd_session_userdata;
    evt.drop.available_types.mime_types = meta->mime_types;
    evt.drop.available_types.count = meta->mime_count;
    evt.drop.modifiers = _maru_wayland_get_modifiers(ctx);
    evt.drop.paths = NULL;
    evt.drop.path_count = 0;

    _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED, (MARU_Window *)window, &evt);
    maru_wl_data_offer_finish(ctx, ctx->clipboard.dnd_offer);
  }

  ctx->clipboard.dnd_offer = NULL;
  ctx->clipboard.dnd_serial = 0;
  ctx->clipboard.dnd_window = NULL;
}

static void _clipboard_data_device_selection(void *data,
                                             struct wl_data_device *data_device,
                                             struct wl_data_offer *offer) {
  (void)data_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx) return;

  if (ctx->clipboard.offer && ctx->clipboard.offer != offer) {
    _maru_wl_remove_offer_meta(ctx, ctx->clipboard.offer);
  }
  ctx->clipboard.offer = offer;
  _maru_wl_clear_read_cache(ctx);
  _maru_wl_clear_mime_query(ctx);
}

static const struct wl_data_device_listener _clipboard_data_device_listener = {
    .data_offer = _clipboard_data_device_data_offer,
    .enter = _clipboard_data_device_enter,
    .leave = _clipboard_data_device_leave,
    .motion = _clipboard_data_device_motion,
    .drop = _clipboard_data_device_drop,
    .selection = _clipboard_data_device_selection,
};

static void _primary_selection_device_data_offer(
    void *data, struct zwp_primary_selection_device_v1 *selection_device,
    struct zwp_primary_selection_offer_v1 *offer) {
  (void)selection_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx || !offer) return;

  MARU_WaylandPrimaryOfferMeta *meta =
      (MARU_WaylandPrimaryOfferMeta *)maru_context_alloc(&ctx->base, sizeof(*meta));
  if (!meta) {
    maru_zwp_primary_selection_offer_v1_destroy(ctx, offer);
    return;
  }
  memset(meta, 0, sizeof(*meta));
  meta->offer = offer;
  meta->ctx = ctx;
  maru_zwp_primary_selection_offer_v1_add_listener(
      ctx, offer, &_primary_selection_offer_listener, meta);
  meta->next = ctx->primary_selection.offer_metas;
  ctx->primary_selection.offer_metas = meta;
}

static void _primary_selection_device_selection(
    void *data, struct zwp_primary_selection_device_v1 *selection_device,
    struct zwp_primary_selection_offer_v1 *offer) {
  (void)selection_device;
  MARU_Context_WL *ctx = (MARU_Context_WL *)data;
  if (!ctx) return;

  if (ctx->primary_selection.offer && ctx->primary_selection.offer != offer) {
    _maru_wl_remove_primary_offer_meta(ctx, ctx->primary_selection.offer);
  }
  ctx->primary_selection.offer = offer;
  _maru_wl_clear_primary_read_cache(ctx);
  _maru_wl_clear_primary_mime_query(ctx);
}

static const struct zwp_primary_selection_device_v1_listener
    _primary_selection_device_listener = {
        .data_offer = _primary_selection_device_data_offer,
        .selection = _primary_selection_device_selection,
};

void _maru_wayland_dataexchange_onSeatCapabilities(MARU_Context_WL *ctx,
                                                   struct wl_seat *seat,
                                                   uint32_t caps) {
  (void)caps;
  if (!ctx || !seat) return;
  if (ctx->protocols.opt.wl_data_device_manager && !ctx->clipboard.device) {
    ctx->clipboard.device = maru_wl_data_device_manager_get_data_device(
        ctx, ctx->protocols.opt.wl_data_device_manager, seat);
    if (ctx->clipboard.device) {
      maru_wl_data_device_add_listener(ctx, ctx->clipboard.device,
                                       &_clipboard_data_device_listener, ctx);
    }
  }
  if (ctx->protocols.opt.zwp_primary_selection_device_manager_v1 &&
      !ctx->primary_selection.device) {
    ctx->primary_selection.device = maru_zwp_primary_selection_device_manager_v1_get_device(
        ctx, ctx->protocols.opt.zwp_primary_selection_device_manager_v1, seat);
    if (ctx->primary_selection.device) {
      maru_zwp_primary_selection_device_v1_add_listener(
          ctx, ctx->primary_selection.device, &_primary_selection_device_listener,
          ctx);
    }
  }
}

void _maru_wayland_dataexchange_onSeatRemoved(MARU_Context_WL *ctx) {
  if (!ctx) return;
  if (ctx->clipboard.device) {
    maru_wl_data_device_destroy(ctx, ctx->clipboard.device);
    ctx->clipboard.device = NULL;
  }
  if (ctx->clipboard.source) {
    struct wl_data_source *source = ctx->clipboard.source;
    ctx->clipboard.source = NULL;
    maru_wl_data_source_destroy(ctx, source);
  }
  if (ctx->clipboard.dnd_source) {
    struct wl_data_source *source = ctx->clipboard.dnd_source;
    ctx->clipboard.dnd_source = NULL;
    maru_wl_data_source_destroy(ctx, source);
  }
  _maru_wl_clear_offer_metas(ctx);
  _maru_wl_clear_announced_mimes(ctx);
  _maru_wl_clear_dnd_announced_mimes(ctx);
  _maru_wl_clear_mime_query(ctx);
  _maru_wl_clear_dnd_mime_query(ctx);
  if (ctx->primary_selection.device) {
    maru_zwp_primary_selection_device_v1_destroy(ctx, ctx->primary_selection.device);
    ctx->primary_selection.device = NULL;
  }
  if (ctx->primary_selection.source) {
    struct zwp_primary_selection_source_v1 *source = ctx->primary_selection.source;
    ctx->primary_selection.source = NULL;
    maru_zwp_primary_selection_source_v1_destroy(ctx, source);
  }
  _maru_wl_clear_primary_offer_metas(ctx);
  _maru_wl_clear_primary_announced_mimes(ctx);
  _maru_wl_clear_primary_mime_query(ctx);
  memset(&ctx->clipboard.dnd_drop, 0, sizeof(ctx->clipboard.dnd_drop));
  maru_linux_dataexchange_destroyTransfers(&ctx->base, &ctx->data_transfers);
  ctx->clipboard.serial = 0;
  ctx->primary_selection.serial = 0;
}

void _maru_wayland_dataexchange_destroy(MARU_Context_WL *ctx) {
  if (!ctx) return;
  _maru_wayland_dataexchange_onSeatRemoved(ctx);
  maru_context_free(&ctx->base, ctx->clipboard.read_cache);
  ctx->clipboard.read_cache = NULL;
  ctx->clipboard.read_cache_capacity = 0;
  ctx->clipboard.read_cache_size = 0;
  maru_context_free(&ctx->base, ctx->primary_selection.read_cache);
  ctx->primary_selection.read_cache = NULL;
  ctx->primary_selection.read_cache_capacity = 0;
  ctx->primary_selection.read_cache_size = 0;
  maru_linux_dataexchange_destroyTransfers(&ctx->base, &ctx->data_transfers);
}

bool _maru_wayland_dataexchange_handle_internal_event(void *ctx_ptr,
                                                       MARU_EventId type,
                                                       MARU_Window *window,
                                                       const MARU_Event *event) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)ctx_ptr;
  if (type != MARU_EVENT_DATA_RECEIVED) return false;
  if (event->data_received.user_tag != (void *)1) return false;

  if (ctx->clipboard.dnd_drop.pending) {
    const char **paths = NULL;
    uint32_t path_count = 0;

    if (event->data_received.status == MARU_SUCCESS && event->data_received.data) {
      path_count = _maru_wl_parse_uri_list(ctx, (const char *)event->data_received.data,
                                          event->data_received.size, &paths);
    }

    MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, ctx->clipboard.dnd_drop.offer);
    
    MARU_Event drop_evt = {0};
    drop_evt.drop.position = ctx->clipboard.dnd_drop.position;
    drop_evt.drop.session_userdata = &ctx->clipboard.dnd_session_userdata;
    drop_evt.drop.modifiers = ctx->clipboard.dnd_drop.modifiers;
    drop_evt.drop.paths = paths;
    drop_evt.drop.path_count = (uint16_t)path_count;
    if (meta) {
      drop_evt.drop.available_types.mime_types = meta->mime_types;
      drop_evt.drop.available_types.count = meta->mime_count;
    }

    ctx->clipboard.dnd_drop.pending = false;

    _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED, window, &drop_evt);

    if (paths) {
      for (uint32_t i = 0; i < path_count; ++i) {
        maru_context_free(&ctx->base, (void *)paths[i]);
      }
      maru_context_free(&ctx->base, (void *)paths);
    }

    if (ctx->clipboard.dnd_drop.offer) {
        maru_wl_data_offer_finish(ctx, ctx->clipboard.dnd_drop.offer);
        ctx->clipboard.dnd_drop.offer = NULL;
    }
    
    ctx->clipboard.dnd_session_userdata = NULL;
  }

  return true;
}

MARU_Status maru_announceData_WL(MARU_Window *window, MARU_DataExchangeTarget target,
                                 const char **mime_types, uint32_t count,
                                 MARU_DropActionMask allowed_actions) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
      target != MARU_DATA_EXCHANGE_TARGET_PRIMARY &&
      target != MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    return MARU_FAILURE;
  }

  MARU_Window_WL *wl_window = (MARU_Window_WL *)window;
  MARU_Context_WL *ctx = (MARU_Context_WL *)wl_window->base.ctx_base;
  if (!_maru_wl_dataexchange_target_supported(ctx, target)) {
    return MARU_FAILURE;
  }
  _maru_wl_dataexchange_ensure_seat_devices(ctx);

  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    if (!ctx->clipboard.device || ctx->clipboard.serial == 0) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_PRECONDITION_FAILURE,
                             "No valid data device/serial available for drag and drop");
      return MARU_FAILURE;
    }

    _maru_wl_clear_dnd_mime_query(ctx);

    if (count == 0) {
      if (ctx->clipboard.dnd_source) {
        struct wl_data_source *source = ctx->clipboard.dnd_source;
        ctx->clipboard.dnd_source = NULL;
        maru_wl_data_source_destroy(ctx, source);
      }
      _maru_wl_clear_dnd_announced_mimes(ctx);
      return MARU_SUCCESS;
    }

    struct wl_data_source *source = maru_wl_data_device_manager_create_data_source(
        ctx, ctx->protocols.opt.wl_data_device_manager);
    if (!source) {
      return MARU_FAILURE;
    }
    maru_wl_data_source_add_listener(ctx, source, &_clipboard_source_listener, ctx);

    for (uint32_t i = 0; i < count; ++i) {
      maru_wl_data_source_offer(ctx, source, mime_types[i]);
    }

    uint32_t wl_actions = 0;
    if (allowed_actions & MARU_DROP_ACTION_COPY) wl_actions |= 1u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY */;
    if (allowed_actions & MARU_DROP_ACTION_MOVE) wl_actions |= 2u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE */;
    if (allowed_actions & MARU_DROP_ACTION_LINK) wl_actions |= 4u /* WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK */;

    if (wl_actions != 0) {
      maru_wl_data_source_set_actions(ctx, source, wl_actions);
    }

    maru_wl_data_device_start_drag(ctx, ctx->clipboard.device, source,
                                   wl_window->wl.surface, NULL,
                                   ctx->clipboard.serial);

    if (ctx->clipboard.dnd_source) {
      struct wl_data_source *old_source = ctx->clipboard.dnd_source;
      ctx->clipboard.dnd_source = NULL;
      maru_wl_data_source_destroy(ctx, old_source);
    }
    ctx->clipboard.dnd_source = source;

    if (!_maru_wl_set_dnd_announced_mimes(ctx, mime_types, count)) {
      return MARU_FAILURE;
    }

    return _maru_wl_flush_or_mark_lost(
               ctx, "wl_display_flush() failed while starting drag and drop")
               ? MARU_SUCCESS
               : MARU_ERROR_CONTEXT_LOST;
  }

  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    if (!ctx->clipboard.device || ctx->clipboard.serial == 0) {
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx,
                             MARU_DIAGNOSTIC_PRECONDITION_FAILURE,
                             "No valid clipboard device/serial available for clipboard ownership");
      return MARU_FAILURE;
    }

    _maru_wl_clear_mime_query(ctx);

    if (count == 0) {
      maru_wl_data_device_set_selection(ctx, ctx->clipboard.device, NULL,
                                        ctx->clipboard.serial);
      if (ctx->clipboard.source) {
        struct wl_data_source *source = ctx->clipboard.source;
        ctx->clipboard.source = NULL;
        maru_wl_data_source_destroy(ctx, source);
      }
      _maru_wl_clear_announced_mimes(ctx);
      return _maru_wl_flush_or_mark_lost(
                 ctx, "wl_display_flush() failed while clearing clipboard")
                 ? MARU_SUCCESS
                 : MARU_ERROR_CONTEXT_LOST;
    }

    struct wl_data_source *source = maru_wl_data_device_manager_create_data_source(
        ctx, ctx->protocols.opt.wl_data_device_manager);
    if (!source) {
      return MARU_FAILURE;
    }
    maru_wl_data_source_add_listener(ctx, source, &_clipboard_source_listener, ctx);

    for (uint32_t i = 0; i < count; ++i) {
      maru_wl_data_source_offer(ctx, source, mime_types[i]);
    }

    maru_wl_data_device_set_selection(ctx, ctx->clipboard.device, source,
                                      ctx->clipboard.serial);
    if (ctx->clipboard.source) {
      struct wl_data_source *old_source = ctx->clipboard.source;
      ctx->clipboard.source = NULL;
      maru_wl_data_source_destroy(ctx, old_source);
    }
    ctx->clipboard.source = source;

    if (!_maru_wl_set_announced_mimes(ctx, mime_types, count)) {
      return MARU_FAILURE;
    }

    return _maru_wl_flush_or_mark_lost(
               ctx, "wl_display_flush() failed while setting clipboard")
               ? MARU_SUCCESS
               : MARU_ERROR_CONTEXT_LOST;
  }

  if (!ctx->primary_selection.device || ctx->primary_selection.serial == 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx, MARU_DIAGNOSTIC_PRECONDITION_FAILURE,
                           "No valid primary-selection device/serial available for primary selection ownership");
    return MARU_FAILURE;
  }

  _maru_wl_clear_primary_mime_query(ctx);

  if (count == 0) {
    maru_zwp_primary_selection_device_v1_set_selection(
        ctx, ctx->primary_selection.device, NULL, ctx->primary_selection.serial);
    if (ctx->primary_selection.source) {
      struct zwp_primary_selection_source_v1 *source = ctx->primary_selection.source;
      ctx->primary_selection.source = NULL;
      maru_zwp_primary_selection_source_v1_destroy(ctx, source);
    }
    _maru_wl_clear_primary_announced_mimes(ctx);
    return _maru_wl_flush_or_mark_lost(
               ctx, "wl_display_flush() failed while clearing primary selection")
               ? MARU_SUCCESS
               : MARU_ERROR_CONTEXT_LOST;
  }

  struct zwp_primary_selection_source_v1 *primary_source =
      maru_zwp_primary_selection_device_manager_v1_create_source(
          ctx, ctx->protocols.opt.zwp_primary_selection_device_manager_v1);
  if (!primary_source) {
    return MARU_FAILURE;
  }
  maru_zwp_primary_selection_source_v1_add_listener(
      ctx, primary_source, &_primary_selection_source_listener, ctx);

  for (uint32_t i = 0; i < count; ++i) {
    maru_zwp_primary_selection_source_v1_offer(ctx, primary_source, mime_types[i]);
  }

  maru_zwp_primary_selection_device_v1_set_selection(
      ctx, ctx->primary_selection.device, primary_source, ctx->primary_selection.serial);
  if (ctx->primary_selection.source) {
    struct zwp_primary_selection_source_v1 *old_source = ctx->primary_selection.source;
    ctx->primary_selection.source = NULL;
    maru_zwp_primary_selection_source_v1_destroy(ctx, old_source);
  }
  ctx->primary_selection.source = primary_source;

  if (!_maru_wl_set_primary_announced_mimes(ctx, mime_types, count)) {
    return MARU_FAILURE;
  }

  return _maru_wl_flush_or_mark_lost(
             ctx, "wl_display_flush() failed while setting primary selection")
             ? MARU_SUCCESS
             : MARU_ERROR_CONTEXT_LOST;
}

MARU_Status maru_requestData_WL(MARU_Window *window, MARU_DataExchangeTarget target,
                                const char *mime_type, void *user_tag) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
      target != MARU_DATA_EXCHANGE_TARGET_PRIMARY &&
      target != MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    return MARU_FAILURE;
  }

  MARU_Window_WL *wl_window = (MARU_Window_WL *)window;
  MARU_Context_WL *ctx = (MARU_Context_WL *)wl_window->base.ctx_base;
  if (!_maru_wl_dataexchange_target_supported(ctx, target)) {
    return MARU_FAILURE;
  }
  _maru_wl_dataexchange_ensure_seat_devices(ctx);
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return MARU_FAILURE;
  }

  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    if (!ctx->clipboard.offer ||
        !_maru_wl_offer_has_mime(ctx, ctx->clipboard.offer, mime_type)) {
      close(pipefd[0]);
      close(pipefd[1]);
      return MARU_FAILURE;
    }
    maru_wl_data_offer_receive(ctx, ctx->clipboard.offer, mime_type, pipefd[1]);
  } else if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    if (!ctx->primary_selection.offer ||
        !_maru_wl_primary_offer_has_mime(ctx, ctx->primary_selection.offer,
                                         mime_type)) {
      close(pipefd[0]);
      close(pipefd[1]);
      return MARU_FAILURE;
    }
    maru_zwp_primary_selection_offer_v1_receive(
        ctx, ctx->primary_selection.offer, mime_type, pipefd[1]);
  } else {
    struct wl_data_offer *offer = ctx->clipboard.dnd_offer;
    if (!offer && ctx->clipboard.dnd_drop.pending) {
      offer = ctx->clipboard.dnd_drop.offer;
    }

    if (!offer || !_maru_wl_offer_has_mime(ctx, offer, mime_type)) {
      close(pipefd[0]);
      close(pipefd[1]);
      return MARU_FAILURE;
    }
    maru_wl_data_offer_receive(ctx, offer, mime_type, pipefd[1]);
  }
  close(pipefd[1]);

  const int flags = fcntl(pipefd[0], F_GETFL, 0);
  if (flags >= 0) {
    (void)fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
  }

  if (!_maru_wl_flush_or_mark_lost(ctx,
                                   target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD
                                       ? "wl_display_flush() failed while requesting clipboard data"
                                       : "wl_display_flush() failed while requesting primary selection data")) {
    close(pipefd[0]);
    return MARU_ERROR_CONTEXT_LOST;
  }

  return maru_linux_dataexchange_queueTransfer(
      &ctx->base, &ctx->data_transfers, pipefd[0], window, target, mime_type,
      user_tag);
}

MARU_Status maru_getAvailableMIMETypes_WL(MARU_Window *window,
                                          MARU_DataExchangeTarget target,
                                          MARU_MIMETypeList *out_list) {
  if (target != MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
      target != MARU_DATA_EXCHANGE_TARGET_PRIMARY &&
      target != MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    out_list->mime_types = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }

  MARU_Window_WL *wl_window = (MARU_Window_WL *)window;
  MARU_Context_WL *ctx = (MARU_Context_WL *)wl_window->base.ctx_base;
  if (!_maru_wl_dataexchange_target_supported(ctx, target)) {
    out_list->mime_types = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }
  _maru_wl_dataexchange_ensure_seat_devices(ctx);

  const char **src = NULL;
  uint32_t src_count = 0;

  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    _maru_wl_clear_mime_query(ctx);
    if (ctx->clipboard.announced_mime_count > 0) {
      src = ctx->clipboard.announced_mime_types;
      src_count = ctx->clipboard.announced_mime_count;
    } else if (ctx->clipboard.offer) {
      MARU_WaylandDataOfferMeta *meta =
          _maru_wl_find_offer_meta(ctx, ctx->clipboard.offer);
      if (meta && meta->mime_count > 0) {
        src = meta->mime_types;
        src_count = meta->mime_count;
      }
    }
  } else if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    _maru_wl_clear_primary_mime_query(ctx);
    if (ctx->primary_selection.announced_mime_count > 0) {
      src = ctx->primary_selection.announced_mime_types;
      src_count = ctx->primary_selection.announced_mime_count;
    } else if (ctx->primary_selection.offer) {
      MARU_WaylandPrimaryOfferMeta *meta =
          _maru_wl_find_primary_offer_meta(ctx, ctx->primary_selection.offer);
      if (meta && meta->mime_count > 0) {
        src = meta->mime_types;
        src_count = meta->mime_count;
      }
    }
  } else {
    _maru_wl_clear_dnd_mime_query(ctx);
    if (ctx->clipboard.dnd_announced_mime_count > 0) {
      src = ctx->clipboard.dnd_announced_mime_types;
      src_count = ctx->clipboard.dnd_announced_mime_count;
    } else {
      struct wl_data_offer *offer = ctx->clipboard.dnd_offer;
      if (!offer && ctx->clipboard.dnd_drop.pending) {
        offer = ctx->clipboard.dnd_drop.offer;
      }
      if (offer) {
        MARU_WaylandDataOfferMeta *meta =
            _maru_wl_find_offer_meta(ctx, offer);
        if (meta && meta->mime_count > 0) {
          src = meta->mime_types;
          src_count = meta->mime_count;
        }
      }
    }
  }

  if (src_count > 0) {
    const char **query = (const char **)maru_context_alloc(
        &ctx->base, sizeof(char *) * src_count);
    if (!query) {
      out_list->mime_types = NULL;
      out_list->count = 0;
      return MARU_FAILURE;
    }
    memcpy((void *)query, src, sizeof(char *) * src_count);
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
      ctx->clipboard.mime_query_ptr = query;
      ctx->clipboard.mime_query_count = src_count;
    } else if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
      ctx->primary_selection.mime_query_ptr = query;
      ctx->primary_selection.mime_query_count = src_count;
    } else {
      ctx->clipboard.dnd_mime_query_ptr = query;
      ctx->clipboard.dnd_mime_query_count = src_count;
    }
  }

  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    out_list->mime_types = ctx->clipboard.mime_query_ptr;
    out_list->count = ctx->clipboard.mime_query_count;
  } else if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    out_list->mime_types = ctx->primary_selection.mime_query_ptr;
    out_list->count = ctx->primary_selection.mime_query_count;
  } else {
    out_list->mime_types = ctx->clipboard.dnd_mime_query_ptr;
    out_list->count = ctx->clipboard.dnd_mime_query_count;
  }
  return MARU_SUCCESS;
}

MARU_Status maru_provideData_WL(const MARU_DataRequestEvent *request_event,
                                const void *data, size_t size,
                                MARU_DataProvideFlags flags) {
  if (!request_event || !request_event->internal_handle) return MARU_FAILURE;

  MARU_WaylandDataRequestHandle *handle =
      (MARU_WaylandDataRequestHandle *)request_event->internal_handle;
  if (handle->magic != MARU_WL_DATA_REQUEST_MAGIC || handle->fd < 0 ||
      handle->consumed) {
    return MARU_FAILURE;
  }

  MARU_Context_WL *ctx = (MARU_Context_WL *)handle->base.ctx_base;
  const bool zero_copy = (flags & MARU_DATA_PROVIDE_FLAG_ZERO_COPY) != 0;

  MARU_Status status = maru_linux_dataexchange_queueWriteTransfer(
      &ctx->base, &ctx->data_transfers, handle->fd, handle->window,
      handle->target, handle->mime_type, data, size, zero_copy);

  if (status == MARU_SUCCESS) {
    handle->fd = -1;
    handle->consumed = true;
  }

  return status;
}

void _maru_wayland_dataexchange_handle_internal_transfer_complete(
    void *ctx_ptr, MARU_Window *window, MARU_DataExchangeTarget target,
    const char *mime_type, const void *data, size_t size, MARU_Status status) {
  MARU_Context_WL *ctx = (MARU_Context_WL *)ctx_ptr;
  (void)target;
  (void)mime_type;

  if (ctx->clipboard.dnd_drop.pending) {
    const char **paths = NULL;
    uint32_t path_count = 0;

    if (status == MARU_SUCCESS && data) {
      path_count = _maru_wl_parse_uri_list(ctx, (const char *)data, size, &paths);
    }

    MARU_WaylandDataOfferMeta *meta = _maru_wl_find_offer_meta(ctx, ctx->clipboard.dnd_drop.offer);
    
    MARU_Event drop_evt = {0};
    drop_evt.drop.position = ctx->clipboard.dnd_drop.position;
    drop_evt.drop.session_userdata = &ctx->clipboard.dnd_session_userdata;
    drop_evt.drop.modifiers = ctx->clipboard.dnd_drop.modifiers;
    drop_evt.drop.paths = paths;
    drop_evt.drop.path_count = (uint16_t)path_count;
    if (meta) {
      drop_evt.drop.available_types.mime_types = meta->mime_types;
      drop_evt.drop.available_types.count = meta->mime_count;
    }

    ctx->clipboard.dnd_drop.pending = false;

    _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED, window, &drop_evt);

    if (paths) {
      for (uint32_t i = 0; i < path_count; ++i) {
        maru_context_free(&ctx->base, (void *)paths[i]);
      }
      maru_context_free(&ctx->base, (void *)paths);
    }

    if (ctx->clipboard.dnd_drop.offer) {
        maru_wl_data_offer_finish(ctx, ctx->clipboard.dnd_drop.offer);
        ctx->clipboard.dnd_drop.offer = NULL;
    }
    
    ctx->clipboard.dnd_session_userdata = NULL;
  }
}
