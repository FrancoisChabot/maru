// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_UDEV_DLIB_H_INCLUDED
#define MARU_UDEV_DLIB_H_INCLUDED

#include "maru_internal.h"
#include "vendor/libudev.h"

#define MARU_UDEV_FUNCTIONS_TABLE             \
  MARU_LIB_FN(udev_new)                       \
  MARU_LIB_FN(udev_unref)                     \
  MARU_LIB_FN(udev_monitor_new_from_netlink)  \
  MARU_LIB_FN(udev_monitor_filter_add_match_subsystem_devtype) \
  MARU_LIB_FN(udev_monitor_enable_receiving)  \
  MARU_LIB_FN(udev_monitor_get_fd)            \
  MARU_LIB_FN(udev_monitor_unref)             \
  MARU_LIB_FN(udev_monitor_receive_device)    \
  MARU_LIB_FN(udev_device_get_action)         \
  MARU_LIB_FN(udev_device_get_devnode)        \
  MARU_LIB_FN(udev_device_get_syspath)        \
  MARU_LIB_FN(udev_device_get_property_value) \
  MARU_LIB_FN(udev_device_unref)              \
  MARU_LIB_FN(udev_enumerate_new)             \
  MARU_LIB_FN(udev_enumerate_add_match_subsystem) \
  MARU_LIB_FN(udev_enumerate_scan_devices)    \
  MARU_LIB_FN(udev_enumerate_get_list_entry)  \
  MARU_LIB_FN(udev_list_entry_get_next)       \
  MARU_LIB_FN(udev_list_entry_get_name)       \
  MARU_LIB_FN(udev_device_new_from_syspath)   \
  MARU_LIB_FN(udev_enumerate_unref)

typedef struct MARU_Lib_Udev {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_UDEV_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Udev;

#endif
