// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_INTERNAL_H_INCLUDED
#define MARU_X11_INTERNAL_H_INCLUDED

#include "linux_internal.h"
#include "dlib/x11.h"
#include "dlib/xcursor.h"
#include "dlib/xi2.h"

typedef struct MARU_Cursor_X11 MARU_Cursor_X11;
typedef struct MARU_Window_X11 MARU_Window_X11;

typedef struct MARU_Context_X11 {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;

  MARU_Lib_X11 x11_lib;
  MARU_Lib_Xcursor xcursor_lib;
  MARU_Lib_Xi2 xi2_lib;
  Display *display;
  int screen;
  Window root;
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
  Atom net_wm_state;
  Atom net_wm_state_fullscreen;
  Atom net_wm_state_maximized_vert;
  Atom net_wm_state_maximized_horz;
  Atom net_wm_state_demands_attention;
  Atom net_active_window;

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

bool _maru_x11_apply_window_cursor_mode(MARU_Context_X11 *ctx, MARU_Window_X11 *win);
void _maru_x11_release_pointer_lock(MARU_Context_X11 *ctx, MARU_Window_X11 *win);

#endif
