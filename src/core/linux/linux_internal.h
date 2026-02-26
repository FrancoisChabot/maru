// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_INTERNAL_H_INCLUDED
#define MARU_LINUX_INTERNAL_H_INCLUDED

#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <linux/input.h>
#include "dlib/xkbcommon.h"
#include "dlib/udev.h"
#include "maru_internal.h"

typedef enum MARU_LinuxWorkerMessage {
  MARU_LINUX_WORKER_MSG_TERMINATE,
} MARU_LinuxWorkerMessage;

typedef struct MARU_LinuxController {
  MARU_ControllerExposed base;
  int fd;
  char *syspath;
  char *devnode;
  char *name;

  _Atomic uint32_t ref_count;
  bool is_active;

  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  uint8_t guid[16];
  bool is_standardized;

  // Dynamically discovered capabilities
  MARU_ButtonChannelInfo *button_channels;
  MARU_ButtonState8 *button_states;
  MARU_AnalogChannelInfo *analog_channels;
  MARU_AnalogInputState *analog_states;

  MARU_HapticChannelInfo *haptic_channels;
  int effect_id; // -1 if no effect uploaded
  bool haptics_dirty;
  MARU_Scalar last_haptic_levels[MARU_CONTROLLER_HAPTIC_STANDARD_COUNT];

  char **allocated_names;
  uint32_t allocated_name_count;

  // Mapping from evdev code to the indices in the arrays above.
  // -1 indicates no mapping.
  int16_t evdev_to_button[KEY_CNT];
  int16_t evdev_to_analog[ABS_CNT];

  // Hardware info for normalization, indexed by analog channel id.
  struct input_absinfo *analog_abs_info;

  struct MARU_LinuxController *next;
} MARU_LinuxController;

typedef struct MARU_LinuxDeviceDiscovery {
  int fd;
  char *syspath;
  char *devnode;
} MARU_LinuxDeviceDiscovery;

typedef enum MARU_LinuxHotplugOpType {
  MARU_LINUX_HOTPLUG_OP_ADD = 0,
  MARU_LINUX_HOTPLUG_OP_REMOVE = 1,
} MARU_LinuxHotplugOpType;

#define MARU_LINUX_HOTPLUG_QUEUE_CAPACITY 8u
#define MARU_LINUX_MAX_SYSPATH_BYTES 512u
#define MARU_LINUX_MAX_DEVNODE_BYTES 512u

typedef struct MARU_LinuxHotplugOp {
  MARU_LinuxHotplugOpType type;
  int fd; // valid for ADD; -1 for REMOVE
  char syspath[MARU_LINUX_MAX_SYSPATH_BYTES];
  char devnode[MARU_LINUX_MAX_DEVNODE_BYTES];
} MARU_LinuxHotplugOp;

typedef struct MARU_Context_Linux_Common {
  MARU_Context_Base *ctx_base;
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
    bool thread_started;
    int event_fd;
    _Atomic MARU_LinuxWorkerMessage message;
    _Atomic bool has_message;

    MARU_Lib_Udev udev_lib;
    struct udev *udev;
    struct udev_monitor *udev_monitor;
    int udev_fd;

    MARU_LinuxHotplugOp hotplug_queue[MARU_LINUX_HOTPLUG_QUEUE_CAPACITY];
    _Atomic uint32_t hotplug_head;
    _Atomic uint32_t hotplug_tail;
    _Atomic bool hotplug_overflowed;
    _Atomic bool hotplug_resync_in_progress;
  } worker;

  MARU_LinuxController *controllers;
  uint32_t controller_count;

  MARU_Lib_Xkb xkb_lib;
} MARU_Context_Linux_Common;

bool _maru_linux_common_init(MARU_Context_Linux_Common* common, MARU_Context_Base* ctx_base);
bool _maru_linux_common_run(MARU_Context_Linux_Common* common);
void _maru_linux_common_cleanup(MARU_Context_Linux_Common* common);
void _maru_linux_common_handle_internal_event(MARU_Context_Linux_Common *common, MARU_InternalEventId type, MARU_Window *window, const MARU_Event *event);
void _maru_linux_common_drain_internal_events(MARU_Context_Linux_Common *common);

void _maru_linux_controller_destroy(MARU_Context_Base* ctx_base, MARU_LinuxController* ctrl);

/** @brief Fills a pollfd array with controller FDs. Returns number of FDs added. */
uint32_t _maru_linux_common_fill_pollfds(MARU_Context_Linux_Common *common, struct pollfd *fds, uint32_t max_fds);
/** @brief Processes any pending controller input from the poll result. */
void _maru_linux_common_process_pollfds(MARU_Context_Linux_Common *common, const struct pollfd *fds, uint32_t count);

MARU_Status _maru_linux_common_set_haptic_levels(MARU_Context_Linux_Common *common, MARU_LinuxController *ctrl, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities);

#endif
