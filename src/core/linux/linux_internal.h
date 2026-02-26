// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_INTERNAL_H_INCLUDED
#define MARU_LINUX_INTERNAL_H_INCLUDED

#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include "dlib/xkbcommon.h"
#include "dlib/udev.h"
#include "maru_internal.h"

typedef enum MARU_LinuxWorkerMessage {
  MARU_LINUX_WORKER_MSG_TERMINATE,
} MARU_LinuxWorkerMessage;

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

  struct {
    pthread_t thread;
    int event_fd;
    _Atomic MARU_LinuxWorkerMessage message;
    _Atomic bool has_message;

    MARU_Lib_Udev udev_lib;
    struct udev *udev;
    struct udev_monitor *udev_monitor;
    int udev_fd;
  } worker;

  MARU_Lib_Xkb xkb_lib;
} MARU_Context_Linux_Common;

bool _maru_linux_common_init(MARU_Context_Linux_Common* common);
void _maru_linux_common_cleanup(MARU_Context_Linux_Common* common);

#endif
