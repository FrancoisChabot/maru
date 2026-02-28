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
#include "dlib/xfixes.h"
#include "maru/c/vulkan.h"

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
  MARU_Lib_Xfixes xfixes_lib;
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
  Atom motif_wm_hints;
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
  bool pending_maximize_request;
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

// Internal helpers shared across X11 modules
bool _maru_x11_copy_string(MARU_Context_X11 *ctx, const char *src, char **out_str);
MARU_Window_X11 *_maru_x11_find_window(MARU_Context_X11 *ctx, Window handle);
void _maru_x11_refresh_monitors(MARU_Context_X11 *ctx);
bool _maru_x11_advance_animated_cursors(MARU_Context_X11 *ctx, uint64_t now_ms);
uint64_t _maru_x11_get_monotonic_time_ms(void);
uint64_t _maru_x11_get_monotonic_time_ns(void);
void _maru_x11_record_pump_duration_ns(MARU_Context_X11 *ctx, uint64_t duration_ns);
void _maru_x11_clear_mime_query_cache(MARU_Context_X11 *ctx);
void _maru_x11_process_event(MARU_Context_X11 *ctx, XEvent *ev);
bool _maru_x11_process_window_event(MARU_Context_X11 *ctx, XEvent *ev);
bool _maru_x11_process_input_event(MARU_Context_X11 *ctx, XEvent *ev);
bool _maru_x11_process_dataexchange_event(MARU_Context_X11 *ctx, XEvent *ev);
MARU_DataExchangeTarget _maru_x11_selection_atom_to_target(const MARU_Context_X11 *ctx, Atom selection_atom);
MARU_X11DataOffer *_maru_x11_get_offer(MARU_Context_X11 *ctx, MARU_DataExchangeTarget target);
MARU_X11DataRequestPending *_maru_x11_get_pending_request(MARU_Context_X11 *ctx, MARU_DataExchangeTarget target);
void _maru_x11_send_selection_notify(MARU_Context_X11 *ctx, const XSelectionRequestEvent *request, Atom property_atom);
bool _maru_x11_is_text_mime(const char *mime);
uint32_t _maru_x11_find_offer_mime_index(MARU_Context_X11 *ctx, const MARU_X11DataOffer *offer, Atom requested_target);
bool _maru_x11_init_context_mouse_channels(MARU_Context_X11 *ctx);
bool _maru_x11_enable_xi2_raw_motion(MARU_Context_X11 *ctx);
MARU_ModifierFlags _maru_x11_get_modifiers(unsigned int state);
MARU_Key _maru_x11_map_keysym(KeySym keysym);
uint32_t _maru_x11_button_to_native_code(unsigned int x_button);
uint32_t _maru_x11_find_mouse_button_id(const MARU_Context_X11 *ctx, uint32_t native_code);
void _maru_x11_xim_preedit_draw(XIC xic, XPointer client_data, XPointer call_data);
void _maru_x11_clear_pending_request(MARU_Context_X11 *ctx, MARU_X11DataRequestPending *request);
void _maru_x11_clear_offer(MARU_Context_X11 *ctx, MARU_X11DataOffer *offer);
void _maru_x11_clear_dnd_session(MARU_Context_X11 *ctx);
void _maru_x11_clear_dnd_source_session(MARU_Context_X11 *ctx, bool emit_finished, MARU_DropAction finished_action);
void _maru_x11_update_dnd_source_target(MARU_Context_X11 *ctx, Time event_time, int x_root_hint, int y_root_hint, bool has_root_hint);
void _maru_x11_send_xdnd_finished(MARU_Context_X11 *ctx, const MARU_X11DnDSession *session, bool accepted);
void _maru_x11_recenter_locked_pointer(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_clear_locked_raw_accum(MARU_Context_X11 *ctx);
void _maru_x11_send_net_wm_state_local(MARU_Context_X11 *ctx, MARU_Window_X11 *win, long action, Atom atom1, Atom atom2);
void _maru_x11_apply_size_hints_local(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_process_selection_notify(MARU_Context_X11 *ctx, XEvent *ev);

// DnD internal helpers shared between modules
void _maru_x11_send_xdnd_status(MARU_Context_X11 *ctx, const MARU_X11DnDSession *session, bool accept);
MARU_DropAction _maru_x11_atom_to_drop_action(const MARU_Context_X11 *ctx, Atom atom);
void _maru_x11_send_xdnd_enter_source(MARU_Context_X11 *ctx, Window target, Window source, uint32_t version, const MARU_X11DataOffer *offer);
void _maru_x11_send_xdnd_leave_source(MARU_Context_X11 *ctx, Window target, Window source);
void _maru_x11_send_xdnd_position_source(MARU_Context_X11 *ctx, Window target, Window source, int x_root, int y_root, Time time, Atom action_atom);
void _maru_x11_send_xdnd_drop_source(MARU_Context_X11 *ctx, Window target, Window source, Time time);
MARU_DropAction _maru_x11_pick_allowed_action(MARU_DropActionMask mask);
void _maru_x11_dnd_set_position_from_packed(MARU_Context_X11 *ctx, MARU_X11DnDSession *session, long packed_xy);
bool _maru_x11_set_dnd_selected_mime(MARU_Context_X11 *ctx, MARU_X11DnDSession *session, const char *mime, Atom atom);
const char *_maru_x11_dnd_first_mime(const MARU_X11DnDSession *session, Atom *out_atom);
bool _maru_x11_dnd_load_type_list(MARU_Context_X11 *ctx, MARU_X11DnDSession *session);
bool _maru_x11_dnd_store_offer_atoms(MARU_Context_X11 *ctx, MARU_X11DnDSession *session, const Atom *atoms, uint32_t count);
bool _maru_x11_dnd_find_mime(const MARU_X11DnDSession *session, const char *mime, Atom *out_atom);
uint32_t _maru_x11_parse_uri_list(MARU_Context_X11 *ctx, const char *data, size_t size, const char ***out_paths);

// Internal API implementations
MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info, MARU_Context **out_context);
MARU_Status maru_destroyContext_X11(MARU_Context *context);
MARU_Status maru_updateContext_X11(MARU_Context *context, uint64_t field_mask, const MARU_ContextAttributes *attributes);
MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);
MARU_Status maru_wakeContext_X11(MARU_Context *context);
void *maru_getContextNativeHandle_X11(MARU_Context *context);

MARU_Status maru_createWindow_X11(MARU_Context *context, const MARU_WindowCreateInfo *create_info, MARU_Window **out_window);
MARU_Status maru_destroyWindow_X11(MARU_Window *window);
void maru_getWindowGeometry_X11(MARU_Window *window, MARU_WindowGeometry *out_geometry);
void *maru_getWindowNativeHandle_X11(MARU_Window *window);
MARU_Status maru_updateWindow_X11(MARU_Window *window, uint64_t field_mask, const MARU_WindowAttributes *attributes);
MARU_Status maru_requestWindowFocus_X11(MARU_Window *window);
MARU_Status maru_requestWindowFrame_X11(MARU_Window *window);
MARU_Status maru_requestWindowAttention_X11(MARU_Window *window);

MARU_Status maru_createCursor_X11(MARU_Context *context, const MARU_CursorCreateInfo *create_info, MARU_Cursor **out_cursor);
MARU_Status maru_destroyCursor_X11(MARU_Cursor *cursor);
MARU_Status maru_resetCursorMetrics_X11(MARU_Cursor *cursor);
MARU_Status maru_createImage_X11(MARU_Context *context, const MARU_ImageCreateInfo *create_info, MARU_Image **out_image);
MARU_Status maru_destroyImage_X11(MARU_Image *image);

MARU_Monitor *const *maru_getMonitors_X11(MARU_Context *context, uint32_t *out_count);
void maru_retainMonitor_X11(MARU_Monitor *monitor);
void maru_releaseMonitor_X11(MARU_Monitor *monitor);
const MARU_VideoMode *maru_getMonitorModes_X11(const MARU_Monitor *monitor, uint32_t *out_count);
MARU_Status maru_setMonitorMode_X11(const MARU_Monitor *monitor, MARU_VideoMode mode);
void maru_resetMonitorMetrics_X11(MARU_Monitor *monitor);

MARU_Status maru_announceData_X11(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions);
MARU_Status maru_provideData_X11(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags);
MARU_Status maru_requestData_X11(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag);
MARU_Status maru_getAvailableMIMETypes_X11(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list);

const char **maru_getVkExtensions_X11(const MARU_Context *context, uint32_t *out_count);
MARU_Status maru_createVkSurface_X11(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

bool _maru_x11_apply_window_cursor_mode(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
bool _maru_x11_apply_window_drop_target(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_release_pointer_lock(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_refresh_text_input_state(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_end_text_session(MARU_Context_X11 *ctx, MARU_Window_X11 *win, bool canceled);
void _maru_x11_clear_window_dataexchange_refs(MARU_Context_X11 *ctx, MARU_Window_X11 *win);

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

#ifdef MARU_INDIRECT_BACKEND
extern const struct MARU_Backend maru_backend_X11;
#endif

#endif
