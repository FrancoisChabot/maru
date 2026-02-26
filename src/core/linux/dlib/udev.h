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
              (struct udev_device * udev_device))                       \
  MARU_LIB_FN(struct udev_enumerate *, udev_enumerate_new,              \
              (struct udev * udev))                                     \
  MARU_LIB_FN(int, udev_enumerate_add_match_subsystem,                  \
              (struct udev_enumerate * udev_enumerate,                  \
               const char *subsystem))                                  \
  MARU_LIB_FN(int, udev_enumerate_scan_devices,                         \
              (struct udev_enumerate * udev_enumerate))                 \
  MARU_LIB_FN(struct udev_list_entry *, udev_enumerate_get_list_entry,    \
              (struct udev_enumerate * udev_enumerate))                 \
  MARU_LIB_FN(struct udev_list_entry *, udev_list_entry_get_next,        \
              (struct udev_list_entry * list_entry))                    \
  MARU_LIB_FN(const char *, udev_list_entry_get_name,                   \
              (struct udev_list_entry * list_entry))                    \
  MARU_LIB_FN(struct udev_device *, udev_device_new_from_syspath,        \
              (struct udev * udev, const char *syspath))                \
  MARU_LIB_FN(struct udev_enumerate *, udev_enumerate_unref,            \
              (struct udev_enumerate * udev_enumerate))

typedef struct MARU_Lib_Udev {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(ret, name, args) ret (*name) args;
  MARU_UDEV_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Udev;

#endif
