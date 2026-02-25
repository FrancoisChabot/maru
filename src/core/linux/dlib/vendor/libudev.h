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

const char *udev_device_get_action(struct udev_device *udev_device);
const char *udev_device_get_devnode(struct udev_device *udev_device);
struct udev_device *udev_device_unref(struct udev_device *udev_device);

#ifdef __cplusplus
}
#endif

#endif
