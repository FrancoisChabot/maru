// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru_internal.h"
#include "maru_mem_internal.h"
#include "x11_internal.h"
#include <string.h>
#include <limits.h>

Atom _maru_x11_target_to_selection_atom(const MARU_Context_X11 *ctx,
                                               MARU_DataExchangeTarget target) {
  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return ctx->selection_clipboard;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    return ctx->selection_primary;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    return ctx->xdnd_selection;
  }
  return None;
}

MARU_DataExchangeTarget
_maru_x11_selection_atom_to_target(const MARU_Context_X11 *ctx, Atom selection_atom) {
  if (selection_atom == ctx->selection_clipboard) {
    return MARU_DATA_EXCHANGE_TARGET_CLIPBOARD;
  }
  if (selection_atom == ctx->selection_primary) {
    return MARU_DATA_EXCHANGE_TARGET_PRIMARY;
  }
  return MARU_DATA_EXCHANGE_TARGET_DRAG_DROP;
}

MARU_X11DataOffer *_maru_x11_get_offer(MARU_Context_X11 *ctx,
                                              MARU_DataExchangeTarget target) {
  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return &ctx->clipboard_offer;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    return &ctx->primary_offer;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    return &ctx->dnd_offer;
  }
  return NULL;
}

MARU_X11DataRequestPending *_maru_x11_get_pending_request(
    MARU_Context_X11 *ctx, MARU_DataExchangeTarget target) {
  if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
    return &ctx->clipboard_request;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_PRIMARY) {
    return &ctx->primary_request;
  }
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    return &ctx->dnd_request;
  }
  return NULL;
}

void _maru_x11_clear_mime_query_cache(MARU_Context_X11 *ctx) {
  if (ctx->clipboard_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard_mime_query_ptr);
    ctx->clipboard_mime_query_ptr = NULL;
    ctx->clipboard_mime_query_count = 0;
  }
  if (ctx->primary_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->primary_mime_query_ptr);
    ctx->primary_mime_query_ptr = NULL;
    ctx->primary_mime_query_count = 0;
  }
  if (ctx->dnd_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->dnd_mime_query_ptr);
    ctx->dnd_mime_query_ptr = NULL;
    ctx->dnd_mime_query_count = 0;
  }
}

void _maru_x11_clear_offer(MARU_Context_X11 *ctx, MARU_X11DataOffer *offer) {
  if (!offer) {
    return;
  }
  if (offer->mime_types) {
    for (uint32_t i = 0; i < offer->mime_count; ++i) {
      if (offer->mime_types[i]) {
        maru_context_free(&ctx->base, offer->mime_types[i]);
      }
    }
    maru_context_free(&ctx->base, offer->mime_types);
    offer->mime_types = NULL;
  }
  if (offer->mime_atoms) {
    maru_context_free(&ctx->base, offer->mime_atoms);
    offer->mime_atoms = NULL;
  }
  offer->mime_count = 0;
  offer->owner_window = NULL;
}

void _maru_x11_clear_pending_request(MARU_Context_X11 *ctx,
                                            MARU_X11DataRequestPending *request) {
  if (!request) {
    return;
  }
  if (request->mime_type) {
    maru_context_free(&ctx->base, request->mime_type);
    request->mime_type = NULL;
  }
  request->pending = false;
  request->window = NULL;
  request->target_atom = None;
  request->user_tag = NULL;
}

bool _maru_x11_set_dnd_selected_mime(MARU_Context_X11 *ctx,
                                             MARU_X11DnDSession *session,
                                             const char *mime, Atom atom) {
  if (session->selected_mime) {
    maru_context_free(&ctx->base, session->selected_mime);
    session->selected_mime = NULL;
  }
  session->selected_target_atom = None;
  if (!mime) {
    return true;
  }
  if (!_maru_x11_copy_string(ctx, mime, &session->selected_mime)) {
    return false;
  }
  session->selected_target_atom = atom;
  return true;
}

void _maru_x11_clear_dnd_session(MARU_Context_X11 *ctx) {
  MARU_X11DnDSession *session = &ctx->dnd_session;
  if (session->selected_mime) {
    maru_context_free(&ctx->base, session->selected_mime);
    session->selected_mime = NULL;
  }
  if (session->offered_mimes) {
    for (uint32_t i = 0; i < session->offered_count; ++i) {
      if (session->offered_mimes[i]) {
        maru_context_free(&ctx->base, session->offered_mimes[i]);
      }
    }
    maru_context_free(&ctx->base, session->offered_mimes);
    session->offered_mimes = NULL;
  }
  if (session->offered_atoms) {
    maru_context_free(&ctx->base, session->offered_atoms);
    session->offered_atoms = NULL;
  }
  memset(session, 0, sizeof(*session));
}

bool _maru_x11_dnd_store_offer_atoms(MARU_Context_X11 *ctx,
                                            MARU_X11DnDSession *session,
                                            const Atom *atoms, uint32_t count) {
  if (session->selected_mime) {
    maru_context_free(&ctx->base, session->selected_mime);
    session->selected_mime = NULL;
  }
  session->selected_target_atom = None;
  if (session->offered_mimes) {
    for (uint32_t i = 0; i < session->offered_count; ++i) {
      if (session->offered_mimes[i]) {
        maru_context_free(&ctx->base, session->offered_mimes[i]);
      }
    }
    maru_context_free(&ctx->base, session->offered_mimes);
    session->offered_mimes = NULL;
  }
  if (session->offered_atoms) {
    maru_context_free(&ctx->base, session->offered_atoms);
    session->offered_atoms = NULL;
  }
  session->offered_count = 0;
  if (count == 0 || !atoms) {
    return true;
  }

  session->offered_atoms =
      (Atom *)maru_context_alloc(&ctx->base, sizeof(Atom) * count);
  session->offered_mimes =
      (char **)maru_context_alloc(&ctx->base, sizeof(char *) * count);
  if (!session->offered_atoms || !session->offered_mimes) {
    if (session->offered_atoms) {
      maru_context_free(&ctx->base, session->offered_atoms);
      session->offered_atoms = NULL;
    }
    if (session->offered_mimes) {
      maru_context_free(&ctx->base, session->offered_mimes);
      session->offered_mimes = NULL;
    }
    session->offered_count = 0;
    return false;
  }
  memset(session->offered_mimes, 0, sizeof(char *) * count);
  memcpy(session->offered_atoms, atoms, sizeof(Atom) * count);
  session->offered_count = count;

  for (uint32_t i = 0; i < count; ++i) {
    char *atom_name = ctx->x11_lib.XGetAtomName(ctx->display, atoms[i]);
    if (!atom_name) {
      continue;
    }
    (void)_maru_x11_copy_string(ctx, atom_name, &session->offered_mimes[i]);
    ctx->x11_lib.XFree(atom_name);
  }
  return true;
}

bool _maru_x11_dnd_load_type_list(MARU_Context_X11 *ctx,
                                         MARU_X11DnDSession *session) {
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char *prop = NULL;

  const int xres = ctx->x11_lib.XGetWindowProperty(
      ctx->display, session->source_window, ctx->xdnd_type_list, 0, 256, False,
      XA_ATOM, &actual_type, &actual_format, &item_count, &bytes_after, &prop);
  if (xres != Success || actual_type != XA_ATOM || actual_format != 32 ||
      item_count == 0 || !prop) {
    if (prop) {
      ctx->x11_lib.XFree(prop);
    }
    return false;
  }

  bool ok = _maru_x11_dnd_store_offer_atoms(ctx, session, (const Atom *)prop,
                                            (uint32_t)item_count);
  ctx->x11_lib.XFree(prop);
  return ok;
}

MARU_DropAction _maru_x11_atom_to_drop_action(const MARU_Context_X11 *ctx,
                                                      Atom atom) {
  if (atom == ctx->xdnd_action_move) {
    return MARU_DROP_ACTION_MOVE;
  }
  if (atom == ctx->xdnd_action_link) {
    return MARU_DROP_ACTION_LINK;
  }
  if (atom == ctx->xdnd_action_copy) {
    return MARU_DROP_ACTION_COPY;
  }
  return MARU_DROP_ACTION_NONE;
}

Atom _maru_x11_drop_action_to_atom(const MARU_Context_X11 *ctx,
                                          MARU_DropAction action) {
  if (action == MARU_DROP_ACTION_MOVE) {
    return ctx->xdnd_action_move;
  }
  if (action == MARU_DROP_ACTION_LINK) {
    return ctx->xdnd_action_link;
  }
  return ctx->xdnd_action_copy;
}

void _maru_x11_dnd_set_position_from_packed(MARU_Context_X11 *ctx,
                                                   MARU_X11DnDSession *session,
                                                   long packed_xy) {
  const int16_t x_root = (int16_t)((packed_xy >> 16) & 0xffffL);
  const int16_t y_root = (int16_t)(packed_xy & 0xffffL);
  int x_local = (int)x_root;
  int y_local = (int)y_root;
  if (session->target_window) {
    Window child = None;
    (void)ctx->x11_lib.XTranslateCoordinates(
        ctx->display, ctx->root, session->target_window->handle, (int)x_root,
        (int)y_root, &x_local, &y_local, &child);
  }
  session->position.x = (MARU_Scalar)x_local;
  session->position.y = (MARU_Scalar)y_local;
}

const char *_maru_x11_dnd_first_mime(const MARU_X11DnDSession *session,
                                            Atom *out_atom) {
  if (out_atom) {
    *out_atom = None;
  }
  for (uint32_t i = 0; i < session->offered_count; ++i) {
    if (session->offered_mimes && session->offered_mimes[i] &&
        session->offered_mimes[i][0] != '\0') {
      if (out_atom && session->offered_atoms) {
        *out_atom = session->offered_atoms[i];
      }
      return session->offered_mimes[i];
    }
  }
  return NULL;
}

bool _maru_x11_dnd_find_mime(const MARU_X11DnDSession *session,
                                    const char *mime, Atom *out_atom) {
  if (out_atom) {
    *out_atom = None;
  }
  if (!session || !mime) {
    return false;
  }
  for (uint32_t i = 0; i < session->offered_count; ++i) {
    if (!session->offered_mimes || !session->offered_mimes[i]) {
      continue;
    }
    if (strcmp(session->offered_mimes[i], mime) == 0) {
      if (out_atom && session->offered_atoms) {
        *out_atom = session->offered_atoms[i];
      }
      return true;
    }
  }
  return false;
}

uint32_t _maru_x11_parse_uri_list(MARU_Context_X11 *ctx, const char *data,
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
      char *copy = (char *)maru_context_alloc(&ctx->base, path_len + 1u);
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

void _maru_x11_send_xdnd_status(MARU_Context_X11 *ctx,
                                       const MARU_X11DnDSession *session,
                                       bool accept) {
  if (!session->source_window || !session->target_window) {
    return;
  }
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = session->source_window;
  ev.xclient.message_type = ctx->xdnd_status;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)session->target_window->handle;
  ev.xclient.data.l[1] = accept ? 1L : 0L;
  ev.xclient.data.l[2] = 0;
  ev.xclient.data.l[3] = 0;
  ev.xclient.data.l[4] = (long)_maru_x11_drop_action_to_atom(ctx, session->selected_action);
  ctx->x11_lib.XSendEvent(ctx->display, session->source_window, False, NoEventMask,
                          &ev);
}

void _maru_x11_send_xdnd_finished(MARU_Context_X11 *ctx,
                                         const MARU_X11DnDSession *session,
                                         bool accepted) {
  if (!session->source_window || !session->target_window) {
    return;
  }
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = session->source_window;
  ev.xclient.message_type = ctx->xdnd_finished;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)session->target_window->handle;
  ev.xclient.data.l[1] = accepted ? 1L : 0L;
  ev.xclient.data.l[2] = (long)_maru_x11_drop_action_to_atom(ctx, session->selected_action);
  ctx->x11_lib.XSendEvent(ctx->display, session->source_window, False, NoEventMask,
                          &ev);
}

MARU_DropAction _maru_x11_pick_allowed_action(MARU_DropActionMask mask) {
  if ((mask & MARU_DROP_ACTION_COPY) != 0) return MARU_DROP_ACTION_COPY;
  if ((mask & MARU_DROP_ACTION_MOVE) != 0) return MARU_DROP_ACTION_MOVE;
  if ((mask & MARU_DROP_ACTION_LINK) != 0) return MARU_DROP_ACTION_LINK;
  return MARU_DROP_ACTION_NONE;
}

void _maru_x11_send_xdnd_enter_source(MARU_Context_X11 *ctx, Window target,
                                             Window source, uint32_t version,
                                             const MARU_X11DataOffer *offer) {
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = target;
  ev.xclient.message_type = ctx->xdnd_enter;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)source;
  ev.xclient.data.l[1] = (long)(((unsigned long)version & 0xfful) << 24);
  if (offer->mime_count > 3u) {
    ev.xclient.data.l[1] |= 1L;
  }
  ev.xclient.data.l[2] = (offer->mime_count > 0u) ? (long)offer->mime_atoms[0] : 0L;
  ev.xclient.data.l[3] = (offer->mime_count > 1u) ? (long)offer->mime_atoms[1] : 0L;
  ev.xclient.data.l[4] = (offer->mime_count > 2u) ? (long)offer->mime_atoms[2] : 0L;
  ctx->x11_lib.XSendEvent(ctx->display, target, False, NoEventMask, &ev);
}

void _maru_x11_send_xdnd_leave_source(MARU_Context_X11 *ctx, Window target,
                                             Window source) {
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = target;
  ev.xclient.message_type = ctx->xdnd_leave;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)source;
  ctx->x11_lib.XSendEvent(ctx->display, target, False, NoEventMask, &ev);
}

void _maru_x11_send_xdnd_position_source(MARU_Context_X11 *ctx, Window target,
                                                Window source, int x_root,
                                                int y_root, Time time,
                                                Atom action_atom) {
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = target;
  ev.xclient.message_type = ctx->xdnd_position;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)source;
  ev.xclient.data.l[2] =
      (long)((((uint32_t)(uint16_t)x_root) << 16) | (uint32_t)(uint16_t)y_root);
  ev.xclient.data.l[3] = (long)time;
  ev.xclient.data.l[4] = (long)action_atom;
  ctx->x11_lib.XSendEvent(ctx->display, target, False, NoEventMask, &ev);
}

void _maru_x11_send_xdnd_drop_source(MARU_Context_X11 *ctx, Window target,
                                            Window source, Time time) {
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = target;
  ev.xclient.message_type = ctx->xdnd_drop;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)source;
  ev.xclient.data.l[2] = (long)time;
  ctx->x11_lib.XSendEvent(ctx->display, target, False, NoEventMask, &ev);
}

static bool _maru_x11_get_xdnd_aware_version(MARU_Context_X11 *ctx, Window window,
                                             uint32_t *out_version) {
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char *prop = NULL;
  const int xres = ctx->x11_lib.XGetWindowProperty(
      ctx->display, window, ctx->xdnd_aware, 0, 1, False, AnyPropertyType,
      &actual_type, &actual_format, &item_count, &bytes_after, &prop);
  (void)bytes_after;
  if (xres != Success || actual_format != 32 || item_count < 1 || !prop) {
    if (prop) {
      ctx->x11_lib.XFree(prop);
    }
    return false;
  }

  const unsigned long *vals = (const unsigned long *)prop;
  *out_version = (uint32_t)vals[0];
  ctx->x11_lib.XFree(prop);
  return true;
}

static Window _maru_x11_query_pointer_target(MARU_Context_X11 *ctx, int *out_x_root,
                                             int *out_y_root) {
  Window root_ret = None;
  Window child_ret = None;
  int root_x = 0;
  int root_y = 0;
  int win_x = 0;
  int win_y = 0;
  unsigned int mask = 0;
  if (!ctx->x11_lib.XQueryPointer(ctx->display, ctx->root, &root_ret, &child_ret,
                                  &root_x, &root_y, &win_x, &win_y, &mask)) {
    return None;
  }
  if (out_x_root) *out_x_root = root_x;
  if (out_y_root) *out_y_root = root_y;
  return child_ret;
}

void _maru_x11_clear_dnd_source_session(MARU_Context_X11 *ctx,
                                                bool emit_finished,
                                                MARU_DropAction finished_action) {
  MARU_X11DnDSourceSession *src = &ctx->dnd_source;
  MARU_Window_X11 *source_window = src->source_window_ref;

  if (src->active) {
    ctx->x11_lib.XUngrabPointer(ctx->display, CurrentTime);
  }
  if (src->source_window != None) {
    ctx->x11_lib.XDeleteProperty(ctx->display, src->source_window,
                                 ctx->xdnd_type_list);
  }

  if (emit_finished && source_window) {
    MARU_Event evt = {0};
    evt.drag_finished.action = finished_action;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_DRAG_FINISHED,
                         (MARU_Window *)source_window, &evt);
  }

  memset(src, 0, sizeof(*src));
}

void _maru_x11_update_dnd_source_target(MARU_Context_X11 *ctx, Time event_time,
                                                int x_root_hint, int y_root_hint,
                                                bool has_root_hint) {
  MARU_X11DnDSourceSession *src = &ctx->dnd_source;
  if (!src->active || src->awaiting_finish || !src->source_window_ref) {
    return;
  }

  int x_root = 0;
  int y_root = 0;
  Window target = _maru_x11_query_pointer_target(ctx, &x_root, &y_root);
  if (has_root_hint) {
    x_root = x_root_hint;
    y_root = y_root_hint;
  }
  if (target == src->source_window) {
    target = None;
  }

  uint32_t target_version = 0;
  if (target != None && !_maru_x11_get_xdnd_aware_version(ctx, target, &target_version)) {
    target = None;
  }

  if (target != src->current_target_window) {
    if (src->current_target_window != None) {
      _maru_x11_send_xdnd_leave_source(ctx, src->current_target_window,
                                       src->source_window);
    }
    src->current_target_window = target;
    src->current_target_accepts = false;
    src->current_target_action = MARU_DROP_ACTION_NONE;
    src->current_target_version = target_version;
    if (target != None) {
      _maru_x11_send_xdnd_enter_source(ctx, target, src->source_window, 5u,
                                       &ctx->dnd_offer);
    }
  }

  if (src->current_target_window != None) {
    _maru_x11_send_xdnd_position_source(ctx, src->current_target_window,
                                        src->source_window, x_root, y_root,
                                        event_time, src->proposed_action_atom);
  }
}

static bool _maru_x11_is_text_target_atom(const MARU_Context_X11 *ctx, Atom atom) {
  return atom == ctx->utf8_string || atom == ctx->text_atom ||
         atom == ctx->compound_text || atom == XA_STRING;
}

bool _maru_x11_is_text_mime(const char *mime) {
  if (!mime || mime[0] == '\0') {
    return false;
  }
  return strncmp(mime, "text/", 5u) == 0 || strcmp(mime, "UTF8_STRING") == 0 ||
         strcmp(mime, "TEXT") == 0 || strcmp(mime, "STRING") == 0 ||
         strcmp(mime, "COMPOUND_TEXT") == 0;
}

uint32_t _maru_x11_find_offer_mime_index(MARU_Context_X11 *ctx,
                                                 const MARU_X11DataOffer *offer,
                                                 Atom requested_target) {
  for (uint32_t i = 0; i < offer->mime_count; ++i) {
    if (offer->mime_atoms[i] == requested_target) {
      return i;
    }
  }

  if (_maru_x11_is_text_target_atom(ctx, requested_target)) {
    for (uint32_t i = 0; i < offer->mime_count; ++i) {
      if (_maru_x11_is_text_mime(offer->mime_types[i])) {
        return i;
      }
    }
  }

  return UINT32_MAX;
}

void _maru_x11_send_selection_notify(MARU_Context_X11 *ctx,
                                            const XSelectionRequestEvent *request,
                                            Atom property_atom) {
  XEvent notify;
  memset(&notify, 0, sizeof(notify));
  notify.xselection.type = SelectionNotify;
  notify.xselection.display = request->display;
  notify.xselection.requestor = request->requestor;
  notify.xselection.selection = request->selection;
  notify.xselection.target = request->target;
  notify.xselection.property = property_atom;
  notify.xselection.time = request->time;
  ctx->x11_lib.XSendEvent(ctx->display, request->requestor, False, 0, &notify);
}

static void _maru_x11_dispatch_data_received(MARU_Context_X11 *ctx,
                                             MARU_X11DataRequestPending *request,
                                             MARU_DataExchangeTarget target,
                                             const void *data, size_t size,
                                             MARU_Status status) {
  MARU_Event evt = {0};
  evt.data_received.user_tag = request->user_tag;
  evt.data_received.status = status;
  evt.data_received.target = target;
  evt.data_received.mime_type = request->mime_type ? request->mime_type : "";
  evt.data_received.data = data;
  evt.data_received.size = size;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_DATA_RECEIVED,
                       (MARU_Window *)request->window, &evt);
}

MARU_Status _maru_x11_announceData(MARU_Window *window,
                                   MARU_DataExchangeTarget target,
                                   const char **mime_types, uint32_t count,
                                   MARU_DropActionMask allowed_actions) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;

  MARU_X11DataOffer *offer = _maru_x11_get_offer(ctx, target);
  Atom selection_atom = _maru_x11_target_to_selection_atom(ctx, target);
  if (!offer || selection_atom == None) {
    return MARU_FAILURE;
  }

  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    _maru_x11_clear_dnd_source_session(ctx, false, MARU_DROP_ACTION_NONE);
  }

  _maru_x11_clear_offer(ctx, offer);
  if (count > 0u) {
    offer->mime_types = (char **)maru_context_alloc(&ctx->base, count * sizeof(char *));
    offer->mime_atoms = (Atom *)maru_context_alloc(&ctx->base, count * sizeof(Atom));
    if (!offer->mime_types || !offer->mime_atoms) {
      _maru_x11_clear_offer(ctx, offer);
      return MARU_FAILURE;
    }
    memset(offer->mime_types, 0, count * sizeof(char *));

    for (uint32_t i = 0; i < count; ++i) {
      if (!mime_types[i] || mime_types[i][0] == '\0') {
        _maru_x11_clear_offer(ctx, offer);
        return MARU_FAILURE;
      }
      if (!_maru_x11_copy_string(ctx, mime_types[i], &offer->mime_types[i])) {
        _maru_x11_clear_offer(ctx, offer);
        return MARU_FAILURE;
      }
      offer->mime_atoms[i] = ctx->x11_lib.XInternAtom(ctx->display, mime_types[i], False);
    }
    offer->mime_count = count;
  }

  offer->owner_window = win;
  ctx->x11_lib.XSetSelectionOwner(ctx->display, selection_atom, win->handle, CurrentTime);
  if (ctx->x11_lib.XGetSelectionOwner(ctx->display, selection_atom) != win->handle) {
    _maru_x11_clear_offer(ctx, offer);
    return MARU_FAILURE;
  }

  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    if (count == 0u) {
      return MARU_SUCCESS;
    }
    if (allowed_actions == 0u) {
      allowed_actions = MARU_DROP_ACTION_COPY;
    }
    ctx->dnd_source.active = true;
    ctx->dnd_source.awaiting_finish = false;
    ctx->dnd_source.source_window = win->handle;
    ctx->dnd_source.source_window_ref = win;
    ctx->dnd_source.allowed_actions = allowed_actions;
    ctx->dnd_source.proposed_action_atom =
        _maru_x11_drop_action_to_atom(ctx, _maru_x11_pick_allowed_action(allowed_actions));
    ctx->dnd_source.current_target_action = MARU_DROP_ACTION_NONE;

    if (offer->mime_count > 0u) {
      ctx->x11_lib.XChangeProperty(ctx->display, win->handle, ctx->xdnd_type_list,
                                   XA_ATOM, 32, PropModeReplace,
                                   (const unsigned char *)offer->mime_atoms,
                                   (int)offer->mime_count);
    } else {
      ctx->x11_lib.XDeleteProperty(ctx->display, win->handle, ctx->xdnd_type_list);
    }

    const int grab_result = ctx->x11_lib.XGrabPointer(
        ctx->display, win->handle, False,
        (unsigned int)(PointerMotionMask | ButtonReleaseMask), GrabModeAsync,
        GrabModeAsync, None, None, CurrentTime);
    if (grab_result != GrabSuccess) {
      _maru_x11_clear_offer(ctx, offer);
      _maru_x11_clear_dnd_source_session(ctx, false, MARU_DROP_ACTION_NONE);
      return MARU_FAILURE;
    }
    _maru_x11_update_dnd_source_target(ctx, CurrentTime, 0, 0, false);
  }

  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

MARU_Status _maru_x11_requestData(MARU_Window *window,
                                  MARU_DataExchangeTarget target,
                                  const char *mime_type, void *user_tag) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    if (!ctx->dnd_session.active || ctx->dnd_session.target_window != win) {
      return MARU_FAILURE;
    }
  }

  MARU_X11DataRequestPending *request = _maru_x11_get_pending_request(ctx, target);
  Atom selection_atom = _maru_x11_target_to_selection_atom(ctx, target);
  if (!request || selection_atom == None) {
    return MARU_FAILURE;
  }
  if (request->pending) {
    return MARU_FAILURE;
  }

  Atom target_atom = ctx->x11_lib.XInternAtom(ctx->display, mime_type, False);
  if (target_atom == None) {
    return MARU_FAILURE;
  }

  if (!_maru_x11_copy_string(ctx, mime_type, &request->mime_type)) {
    return MARU_FAILURE;
  }
  request->pending = true;
  request->window = win;
  request->target_atom = target_atom;
  request->user_tag = user_tag;
  Time request_time = CurrentTime;
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP &&
      ctx->dnd_session.last_position_time != CurrentTime) {
    request_time = ctx->dnd_session.last_position_time;
  }

  ctx->x11_lib.XConvertSelection(ctx->display, selection_atom, target_atom,
                                 ctx->maru_selection_property, win->handle,
                                 request_time);
  ctx->x11_lib.XFlush(ctx->display);
  return MARU_SUCCESS;
}

MARU_Status _maru_x11_getAvailableMIMETypes(MARU_Window *window,
                                            MARU_DataExchangeTarget target,
                                            MARU_MIMETypeList *out_list) {
  MARU_Window_X11 *win = (MARU_Window_X11 *)window;
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)win->base.ctx_base;
  (void)win;

  _maru_x11_clear_mime_query_cache(ctx);
  if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) {
    if (!ctx->dnd_session.active || !ctx->dnd_session.target_window ||
        ctx->dnd_session.target_window != win ||
        ctx->dnd_session.offered_count == 0 || !ctx->dnd_session.offered_mimes) {
      out_list->mime_types = NULL;
      out_list->count = 0;
      return MARU_FAILURE;
    }

    const uint32_t count = ctx->dnd_session.offered_count;
    const char **query =
        (const char **)maru_context_alloc(&ctx->base, count * sizeof(char *));
    if (!query) {
      out_list->mime_types = NULL;
      out_list->count = 0;
      return MARU_FAILURE;
    }
    for (uint32_t i = 0; i < count; ++i) {
      query[i] = ctx->dnd_session.offered_mimes[i] ? ctx->dnd_session.offered_mimes[i] : "";
    }
    ctx->dnd_mime_query_ptr = query;
    ctx->dnd_mime_query_count = count;
    out_list->mime_types = ctx->dnd_mime_query_ptr;
    out_list->count = ctx->dnd_mime_query_count;
    return MARU_SUCCESS;
  }

  MARU_X11DataOffer *offer = _maru_x11_get_offer(ctx, target);
  if (!offer) {
    out_list->mime_types = NULL;
    out_list->count = 0;
    return MARU_FAILURE;
  }

  Atom selection_atom = _maru_x11_target_to_selection_atom(ctx, target);
  const Window owner = ctx->x11_lib.XGetSelectionOwner(ctx->display, selection_atom);
  if (owner != None && offer->owner_window && owner == offer->owner_window->handle &&
      offer->mime_count > 0) {
    const char **query = (const char **)maru_context_alloc(
        &ctx->base, offer->mime_count * sizeof(char *));
    if (!query) {
      out_list->mime_types = NULL;
      out_list->count = 0;
      return MARU_FAILURE;
    }
    for (uint32_t i = 0; i < offer->mime_count; ++i) {
      query[i] = offer->mime_types[i];
    }
    if (target == MARU_DATA_EXCHANGE_TARGET_CLIPBOARD) {
      ctx->clipboard_mime_query_ptr = query;
      ctx->clipboard_mime_query_count = offer->mime_count;
      out_list->mime_types = ctx->clipboard_mime_query_ptr;
      out_list->count = ctx->clipboard_mime_query_count;
    } else {
      ctx->primary_mime_query_ptr = query;
      ctx->primary_mime_query_count = offer->mime_count;
      out_list->mime_types = ctx->primary_mime_query_ptr;
      out_list->count = ctx->primary_mime_query_count;
    }
    return MARU_SUCCESS;
  }

  out_list->mime_types = NULL;
  out_list->count = 0;
  return MARU_FAILURE;
}

MARU_Status _maru_x11_provideData(const MARU_DataRequestEvent *request_event,
                                  const void *data, size_t size,
                                  MARU_DataProvideFlags flags) {
  (void)flags;
  if (!request_event || !request_event->internal_handle) {
    return MARU_FAILURE;
  }
  MARU_X11DataRequestHandle *handle =
      (MARU_X11DataRequestHandle *)request_event->internal_handle;
  if (!handle->base.ctx_base || handle->magic != MARU_X11_DATA_REQUEST_MAGIC ||
      handle->consumed) {
    return MARU_FAILURE;
  }
  MARU_Context_X11 *ctx = (MARU_Context_X11 *)handle->base.ctx_base;

  if (size > 0 && !data) {
    handle->consumed = true;
    XSelectionRequestEvent req;
    memset(&req, 0, sizeof(req));
    req.display = ctx->display;
    req.requestor = handle->requestor;
    req.selection = handle->selection_atom;
    req.target = handle->target_atom;
    req.property = handle->property_atom;
    req.time = CurrentTime;
    _maru_x11_send_selection_notify(ctx, &req, None);
    return MARU_FAILURE;
  }

  if (size > (size_t)INT_MAX) {
    handle->consumed = true;
    XSelectionRequestEvent req;
    memset(&req, 0, sizeof(req));
    req.display = ctx->display;
    req.requestor = handle->requestor;
    req.selection = handle->selection_atom;
    req.target = handle->target_atom;
    req.property = handle->property_atom;
    req.time = CurrentTime;
    _maru_x11_send_selection_notify(ctx, &req, None);
    return MARU_FAILURE;
  }

  const int count32 = (int)size;
  ctx->x11_lib.XChangeProperty(
      ctx->display, handle->requestor, handle->property_atom, handle->target_atom,
      8, PropModeReplace, (const unsigned char *)data, count32);

  XSelectionRequestEvent req;
  memset(&req, 0, sizeof(req));
  req.display = ctx->display;
  req.requestor = handle->requestor;
  req.selection = handle->selection_atom;
  req.target = handle->target_atom;
  req.property = handle->property_atom;
  req.time = CurrentTime;
  _maru_x11_send_selection_notify(ctx, &req, handle->property_atom);
  ctx->x11_lib.XFlush(ctx->display);
  handle->consumed = true;
  return MARU_SUCCESS;
}

void _maru_x11_clear_window_dataexchange_refs(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (!ctx || !win || !win->handle) {
    return;
  }

  if (ctx->clipboard_offer.owner_window == win) {
    ctx->x11_lib.XSetSelectionOwner(ctx->display, ctx->selection_clipboard, None,
                                    CurrentTime);
    _maru_x11_clear_offer(ctx, &ctx->clipboard_offer);
  }
  if (ctx->primary_offer.owner_window == win) {
    ctx->x11_lib.XSetSelectionOwner(ctx->display, ctx->selection_primary, None,
                                    CurrentTime);
    _maru_x11_clear_offer(ctx, &ctx->primary_offer);
  }
  if (ctx->dnd_offer.owner_window == win) {
    ctx->x11_lib.XSetSelectionOwner(ctx->display, ctx->xdnd_selection, None,
                                    CurrentTime);
    _maru_x11_clear_offer(ctx, &ctx->dnd_offer);
  }
  if (ctx->clipboard_request.pending && ctx->clipboard_request.window == win) {
    _maru_x11_clear_pending_request(ctx, &ctx->clipboard_request);
  }
  if (ctx->primary_request.pending && ctx->primary_request.window == win) {
    _maru_x11_clear_pending_request(ctx, &ctx->primary_request);
  }
  if (ctx->dnd_request.pending && ctx->dnd_request.window == win) {
    _maru_x11_clear_pending_request(ctx, &ctx->dnd_request);
  }
  if (ctx->dnd_session.active && ctx->dnd_session.target_window == win) {
    _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, false);
    _maru_x11_clear_dnd_session(ctx);
  }
  if (ctx->dnd_source.active && ctx->dnd_source.source_window_ref == win) {
    _maru_x11_clear_dnd_source_session(ctx, true, MARU_DROP_ACTION_NONE);
  }
}

bool _maru_x11_apply_window_drop_target(MARU_Context_X11 *ctx, MARU_Window_X11 *win) {
  if (!ctx || !win || !win->handle || ctx->xdnd_aware == None) {
    return false;
  }

  if (win->base.attrs_effective.accept_drop) {
    unsigned long version = 5ul;
    ctx->x11_lib.XChangeProperty(ctx->display, win->handle, ctx->xdnd_aware,
                                 XA_ATOM, 32, PropModeReplace,
                                 (const unsigned char *)&version, 1);
  } else {
    ctx->x11_lib.XDeleteProperty(ctx->display, win->handle, ctx->xdnd_aware);
  }
  return true;
}

void _maru_x11_process_selection_notify(MARU_Context_X11 *ctx, XEvent *ev) {
  const XSelectionEvent *notify = &ev->xselection;
  MARU_DataExchangeTarget target =
      _maru_x11_selection_atom_to_target(ctx, notify->selection);
  MARU_X11DataRequestPending *request =
      _maru_x11_get_pending_request(ctx, target);
  if (!request || !request->pending || !request->window ||
      request->window->handle != notify->requestor) {
    return;
  }
  const bool is_internal_dnd_prefetch =
      (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP) &&
      (request->user_tag == (void *)1);

  if (notify->property == None) {
    if (is_internal_dnd_prefetch && ctx->dnd_session.active &&
        ctx->dnd_session.drop_pending && ctx->dnd_session.target_window) {
      MARU_Event drop_evt = {0};
      drop_evt.drop.position = ctx->dnd_session.position;
      drop_evt.drop.session_userdata = &ctx->dnd_session.session_userdata;
      drop_evt.drop.available_types.mime_types =
          (const char *const *)ctx->dnd_session.offered_mimes;
      drop_evt.drop.available_types.count = ctx->dnd_session.offered_count;
      drop_evt.drop.modifiers = 0;
      drop_evt.drop.paths = NULL;
      drop_evt.drop.path_count = 0;
      _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED,
                           (MARU_Window *)ctx->dnd_session.target_window,
                           &drop_evt);
      _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, true);
      _maru_x11_clear_dnd_session(ctx);
    } else {
      _maru_x11_dispatch_data_received(ctx, request, target, NULL, 0,
                                       MARU_FAILURE);
      if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP &&
          ctx->dnd_session.active) {
        _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, false);
        _maru_x11_clear_dnd_session(ctx);
      }
    }
    _maru_x11_clear_pending_request(ctx, request);
    return;
  }

  Atom actual_type = None;
  int actual_format = 0;
  unsigned long item_count = 0;
  unsigned long bytes_after = 0;
  unsigned char *property_data = NULL;
  const int xres = ctx->x11_lib.XGetWindowProperty(
      ctx->display, request->window->handle, notify->property, 0,
      0x1FFFFFFF, True, AnyPropertyType, &actual_type, &actual_format,
      &item_count, &bytes_after, &property_data);
  (void)bytes_after;
  if (xres != Success || actual_type == None || actual_type == ctx->selection_incr) {
    if (property_data) {
      ctx->x11_lib.XFree(property_data);
    }
    if (is_internal_dnd_prefetch && ctx->dnd_session.active &&
        ctx->dnd_session.drop_pending && ctx->dnd_session.target_window) {
      MARU_Event drop_evt = {0};
      drop_evt.drop.position = ctx->dnd_session.position;
      drop_evt.drop.session_userdata = &ctx->dnd_session.session_userdata;
      drop_evt.drop.available_types.mime_types =
          (const char *const *)ctx->dnd_session.offered_mimes;
      drop_evt.drop.available_types.count = ctx->dnd_session.offered_count;
      drop_evt.drop.modifiers = 0;
      drop_evt.drop.paths = NULL;
      drop_evt.drop.path_count = 0;
      _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED,
                           (MARU_Window *)ctx->dnd_session.target_window,
                           &drop_evt);
      _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, true);
      _maru_x11_clear_dnd_session(ctx);
    } else {
      _maru_x11_dispatch_data_received(ctx, request, target, NULL, 0,
                                       MARU_FAILURE);
      if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP &&
          ctx->dnd_session.active) {
        _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, false);
        _maru_x11_clear_dnd_session(ctx);
      }
    }
    _maru_x11_clear_pending_request(ctx, request);
    return;
  }

  size_t size = 0;
  if (actual_format == 8) {
    size = (size_t)item_count;
  } else if (actual_format == 16) {
    size = (size_t)item_count * 2u;
  } else if (actual_format == 32) {
    size = (size_t)item_count * 4u;
  } else {
    size = 0;
  }

  if (is_internal_dnd_prefetch && ctx->dnd_session.active &&
      ctx->dnd_session.drop_pending && ctx->dnd_session.target_window) {
    const char **paths = NULL;
    uint32_t path_count = 0;
    if (property_data && size > 0) {
      path_count =
          _maru_x11_parse_uri_list(ctx, (const char *)property_data, size, &paths);
    }

    MARU_Event drop_evt = {0};
    drop_evt.drop.position = ctx->dnd_session.position;
    drop_evt.drop.session_userdata = &ctx->dnd_session.session_userdata;
    drop_evt.drop.available_types.mime_types =
        (const char *const *)ctx->dnd_session.offered_mimes;
    drop_evt.drop.available_types.count = ctx->dnd_session.offered_count;
    drop_evt.drop.modifiers = 0;
    drop_evt.drop.paths = paths;
    drop_evt.drop.path_count = (uint16_t)path_count;
    _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED,
                         (MARU_Window *)ctx->dnd_session.target_window, &drop_evt);

    if (paths) {
      for (uint32_t i = 0; i < path_count; ++i) {
        maru_context_free(&ctx->base, (void *)paths[i]);
      }
      maru_context_free(&ctx->base, (void *)paths);
    }

    _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, true);
    _maru_x11_clear_dnd_session(ctx);
  } else {
    _maru_x11_dispatch_data_received(ctx, request, target, property_data, size,
                                     MARU_SUCCESS);
    if (target == MARU_DATA_EXCHANGE_TARGET_DRAG_DROP &&
        ctx->dnd_session.active) {
      _maru_x11_send_xdnd_finished(ctx, &ctx->dnd_session, true);
      _maru_x11_clear_dnd_session(ctx);
    }
  }
  if (property_data) {
    ctx->x11_lib.XFree(property_data);
  }
  _maru_x11_clear_pending_request(ctx, request);
}

MARU_Status maru_announceData_X11(MARU_Window *window,
                                         MARU_DataExchangeTarget target,
                                         const char **mime_types, uint32_t count,
                                         MARU_DropActionMask allowed_actions) {
  return _maru_x11_announceData(window, target, mime_types, count,
                                allowed_actions);
}

MARU_Status
maru_provideData_X11(const MARU_DataRequestEvent *request_event, const void *data,
                     size_t size, MARU_DataProvideFlags flags) {
  return _maru_x11_provideData(request_event, data, size, flags);
}

MARU_Status maru_requestData_X11(MARU_Window *window,
                                        MARU_DataExchangeTarget target,
                                        const char *mime_type, void *user_tag) {
  return _maru_x11_requestData(window, target, mime_type, user_tag);
}

MARU_Status maru_getAvailableMIMETypes_X11(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
  return _maru_x11_getAvailableMIMETypes(window, target, out_list);
}

bool _maru_x11_process_dataexchange_event(MARU_Context_X11 *ctx, XEvent *ev) {
  switch (ev->type) {
    case SelectionRequest: {
      const XSelectionRequestEvent *req = &ev->xselectionrequest;
      MARU_DataExchangeTarget target =
          _maru_x11_selection_atom_to_target(ctx, req->selection);
      MARU_X11DataOffer *offer = _maru_x11_get_offer(ctx, target);
      if (!offer || !offer->owner_window || offer->owner_window->handle != req->owner) {
        _maru_x11_send_selection_notify(ctx, req, None);
        return true;
      }

      Atom property_atom = (req->property != None) ? req->property : req->target;
      if (req->target == ctx->selection_targets) {
        const uint32_t atom_capacity = offer->mime_count + 5u;
        Atom *atoms =
            (Atom *)maru_context_alloc(&ctx->base, atom_capacity * sizeof(Atom));
        if (!atoms) {
          _maru_x11_send_selection_notify(ctx, req, None);
          return true;
        }

        uint32_t atom_count = 0;
        atoms[atom_count++] = ctx->selection_targets;
        for (uint32_t i = 0; i < offer->mime_count; ++i) {
          atoms[atom_count++] = offer->mime_atoms[i];
        }
        bool has_text = false;
        for (uint32_t i = 0; i < offer->mime_count; ++i) {
          if (_maru_x11_is_text_mime(offer->mime_types[i])) {
            has_text = true;
            break;
          }
        }
        if (has_text) {
          atoms[atom_count++] = ctx->utf8_string;
          atoms[atom_count++] = ctx->text_atom;
          atoms[atom_count++] = XA_STRING;
          atoms[atom_count++] = ctx->compound_text;
        }
        ctx->x11_lib.XChangeProperty(ctx->display, req->requestor, property_atom,
                                     XA_ATOM, 32, PropModeReplace,
                                     (const unsigned char *)atoms, (int)atom_count);
        maru_context_free(&ctx->base, atoms);
        _maru_x11_send_selection_notify(ctx, req, property_atom);
        return true;
      }

      const uint32_t mime_idx =
          _maru_x11_find_offer_mime_index(ctx, offer, req->target);
      if (mime_idx == UINT32_MAX) {
        _maru_x11_send_selection_notify(ctx, req, None);
        return true;
      }

      MARU_X11DataRequestHandle handle;
      memset(&handle, 0, sizeof(handle));
      handle.base.ctx_base = &ctx->base;
      handle.magic = MARU_X11_DATA_REQUEST_MAGIC;
      handle.target = target;
      handle.requestor = req->requestor;
      handle.selection_atom = req->selection;
      handle.target_atom = req->target;
      handle.property_atom = property_atom;
      handle.window = offer->owner_window;
      if (!_maru_x11_copy_string(ctx, offer->mime_types[mime_idx], &handle.mime_type)) {
        _maru_x11_send_selection_notify(ctx, req, None);
        return true;
      }

      MARU_Event req_evt = {0};
      req_evt.data_requested.target = target;
      req_evt.data_requested.mime_type = handle.mime_type;
      req_evt.data_requested.internal_handle = &handle;
      _maru_dispatch_event(&ctx->base, MARU_EVENT_DATA_REQUESTED,
                           (MARU_Window *)offer->owner_window, &req_evt);

      if (!handle.consumed) {
        _maru_x11_send_selection_notify(ctx, req, None);
      }
      maru_context_free(&ctx->base, handle.mime_type);
      return true;
    }
    case SelectionNotify: {
      _maru_x11_process_selection_notify(ctx, ev);
      return true;
    }
    case SelectionClear: {
      const XSelectionClearEvent *cleared = &ev->xselectionclear;
      if (cleared->selection == ctx->selection_clipboard &&
          ctx->clipboard_offer.owner_window &&
          ctx->clipboard_offer.owner_window->handle == cleared->window) {
        _maru_x11_clear_offer(ctx, &ctx->clipboard_offer);
      } else if (cleared->selection == ctx->selection_primary &&
                 ctx->primary_offer.owner_window &&
                 ctx->primary_offer.owner_window->handle == cleared->window) {
        _maru_x11_clear_offer(ctx, &ctx->primary_offer);
      } else if (cleared->selection == ctx->xdnd_selection &&
                 ctx->dnd_offer.owner_window &&
                 ctx->dnd_offer.owner_window->handle == cleared->window) {
        _maru_x11_clear_offer(ctx, &ctx->dnd_offer);
        if (ctx->dnd_source.active) {
          _maru_x11_clear_dnd_source_session(ctx, true, MARU_DROP_ACTION_NONE);
        }
      }
      return true;
    }
    case ClientMessage: {
      const Atom message_type = ev->xclient.message_type;
      if (message_type == ctx->xdnd_status) {
        MARU_X11DnDSourceSession *src = &ctx->dnd_source;
        if (!src->active || src->awaiting_finish ||
            ev->xclient.window != src->source_window) {
          return true;
        }
        const Window from_target = (Window)ev->xclient.data.l[0];
        if (from_target != src->current_target_window) {
          return true;
        }
        const bool accept = (ev->xclient.data.l[1] & 1L) != 0;
        src->current_target_accepts = accept;
        if (accept) {
          MARU_DropAction action =
              _maru_x11_atom_to_drop_action(ctx, (Atom)ev->xclient.data.l[4]);
          if (action == MARU_DROP_ACTION_NONE) {
            action = _maru_x11_pick_allowed_action(src->allowed_actions);
          }
          src->current_target_action = action;
        } else {
          src->current_target_action = MARU_DROP_ACTION_NONE;
        }
        return true;
      } else if (message_type == ctx->xdnd_finished) {
        MARU_X11DnDSourceSession *src = &ctx->dnd_source;
        if (!src->active || !src->awaiting_finish ||
            ev->xclient.window != src->source_window) {
          return true;
        }
        const Window from_target = (Window)ev->xclient.data.l[0];
        if (from_target != src->current_target_window) {
          return true;
        }
        const bool accepted = (ev->xclient.data.l[1] & 1L) != 0;
        MARU_DropAction action = MARU_DROP_ACTION_NONE;
        if (accepted) {
          action = _maru_x11_atom_to_drop_action(ctx, (Atom)ev->xclient.data.l[2]);
          if (action == MARU_DROP_ACTION_NONE) {
            action = src->current_target_action;
          }
        }
        _maru_x11_clear_dnd_source_session(ctx, true, action);
        return true;
      } else if (message_type == ctx->xdnd_enter) {
        MARU_Window_X11 *win = _maru_x11_find_window(ctx, ev->xclient.window);
        if (!win) {
          return true;
        }

        _maru_x11_clear_dnd_session(ctx);
        MARU_X11DnDSession *session = &ctx->dnd_session;
        session->active = true;
        session->source_window = (Window)ev->xclient.data.l[0];
        session->target_window = win;
        session->version =
            (uint32_t)(((unsigned long)ev->xclient.data.l[1] >> 24) & 0xfful);
        session->selected_action = MARU_DROP_ACTION_COPY;
        session->position = (MARU_Vec2Dip){0};
        session->last_position_time = CurrentTime;

        if (win->base.attrs_effective.accept_drop) {
          const bool has_type_list = (ev->xclient.data.l[1] & 1L) != 0;
          if (has_type_list) {
            (void)_maru_x11_dnd_load_type_list(ctx, session);
          } else {
            Atom direct_atoms[3];
            uint32_t direct_count = 0;
            for (uint32_t i = 0; i < 3; ++i) {
              Atom atom = (Atom)ev->xclient.data.l[2 + (long)i];
              if (atom != None) {
                direct_atoms[direct_count++] = atom;
              }
            }
            (void)_maru_x11_dnd_store_offer_atoms(ctx, session, direct_atoms,
                                                  direct_count);
          }
          Atom first_atom = None;
          const char *first_mime = _maru_x11_dnd_first_mime(session, &first_atom);
          (void)_maru_x11_set_dnd_selected_mime(ctx, session, first_mime, first_atom);
        }

        MARU_DropAction action = session->selected_mime ? MARU_DROP_ACTION_COPY
                                                        : MARU_DROP_ACTION_NONE;
        MARU_DropFeedback feedback = {
            .action = &action,
            .session_userdata = &session->session_userdata,
        };
        MARU_Event enter_evt = {0};
        enter_evt.drop_enter.position = session->position;
        enter_evt.drop_enter.feedback = &feedback;
        enter_evt.drop_enter.available_types.mime_types =
            (const char *const *)session->offered_mimes;
        enter_evt.drop_enter.available_types.count = session->offered_count;
        enter_evt.drop_enter.modifiers = 0;
        enter_evt.drop_enter.paths = NULL;
        enter_evt.drop_enter.path_count = 0;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_ENTERED, (MARU_Window *)win,
                             &enter_evt);

        session->selected_action = action;
        if (action == MARU_DROP_ACTION_NONE) {
          (void)_maru_x11_set_dnd_selected_mime(ctx, session, NULL, None);
        } else if (!session->selected_mime) {
          Atom first_atom = None;
          const char *first_mime = _maru_x11_dnd_first_mime(session, &first_atom);
          (void)_maru_x11_set_dnd_selected_mime(ctx, session, first_mime, first_atom);
        }
        return true;
      } else if (message_type == ctx->xdnd_position) {
        MARU_X11DnDSession *session = &ctx->dnd_session;
        if (!session->active || !session->target_window ||
            session->source_window != (Window)ev->xclient.data.l[0]) {
          return true;
        }

        _maru_x11_dnd_set_position_from_packed(ctx, session, ev->xclient.data.l[2]);
        session->last_position_time = (Time)ev->xclient.data.l[3];
        const MARU_DropAction source_action =
            _maru_x11_atom_to_drop_action(ctx, (Atom)ev->xclient.data.l[4]);
        if (source_action != MARU_DROP_ACTION_NONE) {
          session->selected_action = source_action;
        }

        MARU_DropAction action =
            session->selected_mime ? session->selected_action : MARU_DROP_ACTION_NONE;
        MARU_DropFeedback feedback = {
            .action = &action,
            .session_userdata = &session->session_userdata,
        };
        MARU_Event hover_evt = {0};
        hover_evt.drop_hover.position = session->position;
        hover_evt.drop_hover.feedback = &feedback;
        hover_evt.drop_hover.available_types.mime_types =
            (const char *const *)session->offered_mimes;
        hover_evt.drop_hover.available_types.count = session->offered_count;
        hover_evt.drop_hover.modifiers = 0;
        hover_evt.drop_hover.paths = NULL;
        hover_evt.drop_hover.path_count = 0;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_HOVERED,
                             (MARU_Window *)session->target_window, &hover_evt);

        session->selected_action = action;
        if (action == MARU_DROP_ACTION_NONE) {
          (void)_maru_x11_set_dnd_selected_mime(ctx, session, NULL, None);
        } else if (!session->selected_mime) {
          Atom first_atom = None;
          const char *first_mime = _maru_x11_dnd_first_mime(session, &first_atom);
          (void)_maru_x11_set_dnd_selected_mime(ctx, session, first_mime, first_atom);
        }

        const bool accept = (session->selected_action != MARU_DROP_ACTION_NONE) &&
                            (session->selected_mime != NULL) &&
                            session->target_window->base.attrs_effective.accept_drop;
        _maru_x11_send_xdnd_status(ctx, session, accept);
        return true;
      } else if (message_type == ctx->xdnd_leave) {
        MARU_X11DnDSession *session = &ctx->dnd_session;
        if (!session->active ||
            session->source_window != (Window)ev->xclient.data.l[0]) {
          return true;
        }
        if (session->target_window) {
          MARU_Event leave_evt = {0};
          leave_evt.drop_leave.session_userdata = &session->session_userdata;
          _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_EXITED,
                               (MARU_Window *)session->target_window, &leave_evt);
        }
        _maru_x11_clear_dnd_session(ctx);
        return true;
      } else if (message_type == ctx->xdnd_drop) {
        MARU_X11DnDSession *session = &ctx->dnd_session;
        if (!session->active || !session->target_window ||
            session->source_window != (Window)ev->xclient.data.l[0]) {
          return true;
        }
        const bool accepted = (session->selected_action != MARU_DROP_ACTION_NONE) &&
                              (session->selected_mime != NULL) &&
                              session->target_window->base.attrs_effective.accept_drop;
        if (!accepted) {
          _maru_x11_send_xdnd_finished(ctx, session, false);
          _maru_x11_clear_dnd_session(ctx);
          return true;
        }

        Atom uri_atom = None;
        if (_maru_x11_dnd_find_mime(session, "text/uri-list", &uri_atom)) {
          session->drop_pending = true;
          if (session->selected_target_atom != uri_atom ||
              !session->selected_mime ||
              strcmp(session->selected_mime, "text/uri-list") != 0) {
            (void)_maru_x11_set_dnd_selected_mime(ctx, session, "text/uri-list",
                                                  uri_atom);
          }
          if (session->selected_mime &&
              _maru_x11_requestData((MARU_Window *)session->target_window,
                                    MARU_DATA_EXCHANGE_TARGET_DRAG_DROP,
                                    session->selected_mime, (void *)1) ==
                  MARU_SUCCESS) {
            return true;
          }
          session->drop_pending = false;
        }

        MARU_Event drop_evt = {0};
        drop_evt.drop.position = session->position;
        drop_evt.drop.session_userdata = &ctx->dnd_session.session_userdata;
        drop_evt.drop.available_types.mime_types =
            (const char *const *)ctx->dnd_session.offered_mimes;
        drop_evt.drop.available_types.count = ctx->dnd_session.offered_count;
        drop_evt.drop.modifiers = 0;
        drop_evt.drop.paths = NULL;
        drop_evt.drop.path_count = 0;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_DROP_DROPPED,
                             (MARU_Window *)session->target_window, &drop_evt);

        _maru_x11_send_xdnd_finished(ctx, session, true);
        _maru_x11_clear_dnd_session(ctx);
        return true;
      }
      return false;
    }
    default: return false;
  }
}
