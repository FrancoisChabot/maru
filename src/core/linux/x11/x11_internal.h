// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_INTERNAL_H_INCLUDED
#define MARU_X11_INTERNAL_H_INCLUDED

#include "linux_internal.h"
#include "dlib/x11.h"

typedef struct MARU_Context_X11 {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;

  MARU_Lib_X11 x11_lib;
  Display *display;
  int screen;
  Window root;
  
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

typedef struct MARU_Window_X11 {
  MARU_Window_Base base;
  Window handle;
  Colormap colormap;
  
  bool is_fullscreen;
  bool is_maximized;
} MARU_Window_X11;

#endif
