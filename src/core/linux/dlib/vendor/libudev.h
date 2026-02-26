// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

// N.B.: 
// - This file is project-authored and intentionally minimal.
// - Do not copy upstream libudev.h text into-tree.
// - Always load those symbols dynamically.

#ifndef MARU_VENDOR_LIBUDEV_H_INCLUDED
#define MARU_VENDOR_LIBUDEV_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

struct udev;
struct udev_device;
struct udev_monitor;

struct udev *udev_new(void);
struct udev *udev_unref(struct udev *udev);

struct udev_monitor *udev_monitor_new_from_netlink(struct udev *udev,
                                                   const char *name);
int udev_monitor_filter_add_match_subsystem_devtype(
    struct udev_monitor *udev_monitor, const char *subsystem,
    const char *devtype);
int udev_monitor_enable_receiving(struct udev_monitor *udev_monitor);
int udev_monitor_get_fd(struct udev_monitor *udev_monitor);
struct udev_monitor *udev_monitor_unref(struct udev_monitor *udev_monitor);
struct udev_device *udev_monitor_receive_device(struct udev_monitor *udev_monitor);

struct udev_enumerate;
struct udev_list_entry;

struct udev_enumerate *udev_enumerate_new(struct udev *udev);
int udev_enumerate_add_match_subsystem(struct udev_enumerate *udev_enumerate,
                                       const char *subsystem);
int udev_enumerate_scan_devices(struct udev_enumerate *udev_enumerate);
struct udev_list_entry *
udev_enumerate_get_list_entry(struct udev_enumerate *udev_enumerate);
struct udev_enumerate *udev_enumerate_unref(struct udev_enumerate *udev_enumerate);

struct udev_list_entry *udev_list_entry_get_next(struct udev_list_entry *list_entry);
const char *udev_list_entry_get_name(struct udev_list_entry *list_entry);

struct udev_device *udev_device_new_from_syspath(struct udev *udev,
                                                 const char *syspath);

const char *udev_device_get_action(struct udev_device *udev_device);
const char *udev_device_get_devnode(struct udev_device *udev_device);
const char *udev_device_get_syspath(struct udev_device *udev_device);
const char *udev_device_get_property_value(struct udev_device *udev_device, const char *key);
struct udev_device *udev_device_unref(struct udev_device *udev_device);

#define udev_list_entry_foreach(entry, first)                                  \
  for (entry = first; entry != NULL; entry = udev_list_entry_get_next(entry))

#ifdef __cplusplus
}
#endif

#endif
