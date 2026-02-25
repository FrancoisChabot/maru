// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_INTERNAL_H_INCLUDED
#define MARU_LINUX_INTERNAL_H_INCLUDED

#include <poll.h>
#include "dlib/xkbcommon.h"
#include "dlib/udev.h"
#include "maru_internal.h"

#define MARU_CONTROLLER_PENDING_ADD_CAPACITY 16u
#define MARU_CONTROLLER_PENDING_PATH_MAX 512u

typedef struct MARU_ControllerContext_Linux {
  struct MARU_Controller_Linux *list_head;
  MARU_Controller **snapshot;
  struct pollfd *pollfds;
  uint32_t snapshot_count;
  uint32_t snapshot_capacity;
  uint32_t pollfds_capacity;
  uint32_t connected_count;
  int inotify_fd;
  int inotify_wd;
  char pending_add_paths[MARU_CONTROLLER_PENDING_ADD_CAPACITY]
                        [MARU_CONTROLLER_PENDING_PATH_MAX];
  uint32_t pending_add_head;
  uint32_t pending_add_count;

  MARU_Lib_Udev udev_lib;
  struct udev *udev;
  struct udev_monitor *udev_monitor;
  int udev_fd;
  bool use_udev;
} MARU_ControllerContext_Linux;

typedef struct MARU_Context_Linux_Common {
  struct {
    MARU_Window *focused_window;
    struct xkb_context *ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    struct {
      uint32_t shift;
      uint32_t ctrl;
      uint32_t alt;
      uint32_t meta;
      uint32_t caps;
      uint32_t num;
    } mod_indices;
  } xkb;

  struct {
    MARU_Window *focused_window;
    double x;
    double y;
    uint32_t enter_serial;
  } pointer;

  MARU_Lib_Xkb xkb_lib;
  MARU_ControllerContext_Linux controller;
} MARU_Context_Linux_Common;

#endif
