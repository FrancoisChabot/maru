// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_UDEV_DLIB_H_INCLUDED
#define MARU_UDEV_DLIB_H_INCLUDED

#include "maru_internal.h"
#include "vendor/libudev.h"

#define MARU_UDEV_FUNCTIONS_TABLE                                       \
  MARU_LIB_FN(struct udev *, udev_new, (void))                          \
  MARU_LIB_FN(struct udev *, udev_unref, (struct udev * udev))          \
  MARU_LIB_FN(struct udev_monitor *, udev_monitor_new_from_netlink,     \
              (struct udev * udev, const char *name))                   \
  MARU_LIB_FN(int, udev_monitor_filter_add_match_subsystem_devtype,     \
              (struct udev_monitor * udev_monitor, const char *subsystem, \
               const char *devtype))                                    \
  MARU_LIB_FN(int, udev_monitor_enable_receiving,                       \
              (struct udev_monitor * udev_monitor))                     \
  MARU_LIB_FN(int, udev_monitor_get_fd, (struct udev_monitor * udev_monitor)) \
  MARU_LIB_FN(struct udev_monitor *, udev_monitor_unref,                \
              (struct udev_monitor * udev_monitor))                     \
  MARU_LIB_FN(struct udev_device *, udev_monitor_receive_device,        \
              (struct udev_monitor * udev_monitor))                     \
  MARU_LIB_FN(const char *, udev_device_get_action,                     \
              (struct udev_device * udev_device))                       \
  MARU_LIB_FN(const char *, udev_device_get_devnode,                    \
              (struct udev_device * udev_device))                       \
  MARU_LIB_FN(struct udev_device *, udev_device_unref,                  \
              (struct udev_device * udev_device))

typedef struct MARU_Lib_Udev {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(ret, name, args) ret (*name) args;
  MARU_UDEV_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Udev;

#endif
