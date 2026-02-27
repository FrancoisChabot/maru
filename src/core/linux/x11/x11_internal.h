// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_INTERNAL_H_INCLUDED
#define MARU_X11_INTERNAL_H_INCLUDED

#include "linux_internal.h"
#include "dlib/x11.h"
#include "dlib/xcursor.h"
#include "dlib/xi2.h"
#include "dlib/xshape.h"
#include "dlib/xrandr.h"

typedef struct MARU_Cursor_X11 MARU_Cursor_X11;
typedef struct MARU_Window_X11 MARU_Window_X11;
typedef struct MARU_Monitor_X11 MARU_Monitor_X11;
typedef struct MARU_X11DataRequestHandle MARU_X11DataRequestHandle;

#define MARU_X11_DATA_REQUEST_MAGIC UINT64_C(0xD1A7A5E11BADC0DE)

typedef struct MARU_X11DataOffer {
  MARU_Window_X11 *owner_window;
  char **mime_types;
  Atom *mime_atoms;
  uint32_t mime_count;
} MARU_X11DataOffer;

typedef struct MARU_X11DataRequestPending {
  bool pending;
  MARU_Window_X11 *window;
  char *mime_type;
  Atom target_atom;
  void *user_tag;
} MARU_X11DataRequestPending;

typedef struct MARU_X11DnDSession {
  bool active;
  bool drop_pending;
  Window source_window;
  MARU_Window_X11 *target_window;
  uint32_t version;
  Time last_position_time;
  MARU_Vec2Dip position;
  MARU_DropAction selected_action;
  Atom selected_target_atom;
  char *selected_mime;
  void *session_userdata;
  char **offered_mimes;
  Atom *offered_atoms;
  uint32_t offered_count;
} MARU_X11DnDSession;

typedef struct MARU_X11DnDSourceSession {
  bool active;
  bool awaiting_finish;
  Window source_window;
  MARU_Window_X11 *source_window_ref;
  Window current_target_window;
  uint32_t current_target_version;
  bool current_target_accepts;
  MARU_DropAction current_target_action;
  MARU_DropActionMask allowed_actions;
  Atom proposed_action_atom;
} MARU_X11DnDSourceSession;

struct MARU_X11DataRequestHandle {
  MARU_DataRequestHandleBase base;
  uint64_t magic;
  bool consumed;
  MARU_DataExchangeTarget target;
  Window requestor;
  Atom selection_atom;
  Atom property_atom;
  Atom target_atom;
  char *mime_type;
  MARU_Window_X11 *window;
};

typedef struct MARU_Context_X11 {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;

  MARU_Lib_X11 x11_lib;
  MARU_Lib_Xcursor xcursor_lib;
  MARU_Lib_Xi2 xi2_lib;
  MARU_Lib_Xshape xshape_lib;
  MARU_Lib_Xrandr xrandr_lib;
  Display *display;
  int screen;
  Window root;
  XIM xim;
  MARU_Cursor_X11 *animated_cursors_head;
  Cursor hidden_cursor;
  MARU_Window_X11 *locked_window;
  bool xi2_raw_motion_enabled;
  int xi2_opcode;
  MARU_Scalar locked_raw_dx_accum;
  MARU_Scalar locked_raw_dy_accum;
  bool locked_raw_pending;
  
  Atom wm_protocols;
  Atom wm_delete_window;
  Atom net_wm_name;
  Atom net_wm_icon_name;
  Atom net_wm_icon;
  Atom net_wm_state;
  Atom net_wm_state_fullscreen;
  Atom net_wm_state_maximized_vert;
  Atom net_wm_state_maximized_horz;
  Atom net_wm_state_demands_attention;
  Atom net_active_window;
  Atom selection_clipboard;
  Atom selection_primary;
  Atom selection_targets;
  Atom selection_incr;
  Atom utf8_string;
  Atom text_atom;
  Atom compound_text;
  Atom maru_selection_property;
  Atom xdnd_aware;
  Atom xdnd_enter;
  Atom xdnd_position;
  Atom xdnd_status;
  Atom xdnd_leave;
  Atom xdnd_drop;
  Atom xdnd_finished;
  Atom xdnd_selection;
  Atom xdnd_type_list;
  Atom xdnd_action_copy;
  Atom xdnd_action_move;
  Atom xdnd_action_link;

  MARU_X11DataOffer clipboard_offer;
  MARU_X11DataOffer primary_offer;
  MARU_X11DataOffer dnd_offer;
  MARU_X11DataRequestPending clipboard_request;
  MARU_X11DataRequestPending primary_request;
  MARU_X11DataRequestPending dnd_request;
  MARU_X11DnDSession dnd_session;
  MARU_X11DnDSourceSession dnd_source;
  const char **clipboard_mime_query_ptr;
  uint32_t clipboard_mime_query_count;
  const char **primary_mime_query_ptr;
  uint32_t primary_mime_query_count;
  const char **dnd_mime_query_ptr;
  uint32_t dnd_mime_query_count;

  MARU_Controller **controller_list_storage;
  uint32_t controller_list_capacity;
} MARU_Context_X11;

struct MARU_Window_X11 {
  MARU_Window_Base base;
  Window handle;
  Colormap colormap;
  MARU_Vec2Dip server_logical_size;
  
  bool is_fullscreen;
  bool is_maximized;
  int lock_center_x;
  int lock_center_y;
  bool suppress_lock_warp_event;
  XIC xic;
  uint64_t text_input_session_id;
  bool text_input_session_active;
  bool ime_preedit_active;
  char *ime_preedit_storage;
};

struct MARU_Cursor_X11 {
  MARU_Cursor_Base base;
  Cursor handle;
  bool is_system;
  bool is_animated;
  Cursor *frame_handles;
  uint32_t *frame_delays_ms;
  uint32_t frame_count;
  uint32_t current_frame;
  uint64_t next_frame_deadline_ms;
  MARU_Cursor_X11 *anim_prev;
  MARU_Cursor_X11 *anim_next;
};

struct MARU_Monitor_X11 {
  MARU_Monitor_Base base;
  RROutput output;
  RRCrtc crtc;
  RRMode current_mode_id;
  Rotation current_rotation;
  char *name_storage;
  MARU_VideoMode *modes;
  uint32_t mode_count;
};

bool _maru_x11_apply_window_cursor_mode(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
bool _maru_x11_apply_window_drop_target(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_release_pointer_lock(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_refresh_text_input_state(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
MARU_Status _maru_x11_announceData(MARU_Window *window, MARU_DataExchangeTarget target,
                                   const char **mime_types, uint32_t count,
                                   MARU_DropActionMask allowed_actions);
MARU_Status _maru_x11_provideData(const MARU_DataRequestEvent *request_event, const void *data,
                                  size_t size, MARU_DataProvideFlags flags);
MARU_Status _maru_x11_requestData(MARU_Window *window, MARU_DataExchangeTarget target,
                                  const char *mime_type, void *user_tag);
MARU_Status _maru_x11_getAvailableMIMETypes(MARU_Window *window,
                                            MARU_DataExchangeTarget target,
                                            MARU_MIMETypeList *out_list);
void _maru_x11_clear_window_dataexchange_refs(MARU_Context_X11 *ctx, MARU_Window_X11 *win);

#endif
