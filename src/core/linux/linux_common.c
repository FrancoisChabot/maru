#define _GNU_SOURCE
#include "linux_internal.h"
#include "dlib/linux_loader.h"
#include "maru_mem_internal.h"
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <limits.h>

#define TEST_BIT(bit, array) ((array[(size_t)(bit) / (8 * sizeof(unsigned long))] >> ((size_t)(bit) % (8 * sizeof(unsigned long)))) & 1)

static bool _maru_linux_is_gamepad(int fd) {
  unsigned long ev_bits[EV_MAX / (8 * sizeof(unsigned long)) + 1] = {0};
  unsigned long key_bits[KEY_CNT / (8 * sizeof(unsigned long)) + 1] = {0};
  unsigned long abs_bits[ABS_CNT / (8 * sizeof(unsigned long)) + 1] = {0};

  if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) return false;

  if (!TEST_BIT(EV_KEY, ev_bits) || !TEST_BIT(EV_ABS, ev_bits)) {
    return false;
  }

  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) return false;
  if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0) return false;

  bool has_gamepad_button = TEST_BIT(BTN_GAMEPAD, key_bits) ||
                            TEST_BIT(BTN_SOUTH, key_bits);

  bool has_axes = TEST_BIT(ABS_X, abs_bits) &&
                  TEST_BIT(ABS_Y, abs_bits);

  return has_gamepad_button && has_axes;
}

static bool _maru_linux_hotplug_enqueue(MARU_Context_Linux_Common *common,
                                        MARU_LinuxHotplugOpType type, int fd,
                                        const char *syspath,
                                        const char *devnode) {
  if (!common || !syspath) {
    if (fd >= 0) close(fd);
    return false;
  }

  if (atomic_load_explicit(&common->worker.hotplug_resync_in_progress,
                           memory_order_acquire)) {
    if (fd >= 0) close(fd);
    atomic_store_explicit(&common->worker.hotplug_overflowed, true,
                          memory_order_release);
    return false;
  }

  const size_t syspath_len = strlen(syspath);
  const size_t devnode_len = devnode ? strlen(devnode) : 0u;
  if (syspath_len >= MARU_LINUX_MAX_SYSPATH_BYTES ||
      devnode_len >= MARU_LINUX_MAX_DEVNODE_BYTES) {
    if (fd >= 0) close(fd);
    atomic_store_explicit(&common->worker.hotplug_overflowed, true,
                          memory_order_release);
    return false;
  }

  const uint32_t capacity = MARU_LINUX_HOTPLUG_QUEUE_CAPACITY;
  uint32_t head = atomic_load_explicit(&common->worker.hotplug_head,
                                       memory_order_relaxed);
  const uint32_t tail = atomic_load_explicit(&common->worker.hotplug_tail,
                                             memory_order_acquire);

  if ((head - tail) >= capacity) {
    if (fd >= 0) close(fd);
    atomic_store_explicit(&common->worker.hotplug_overflowed, true,
                          memory_order_release);
    return false;
  }

  MARU_LinuxHotplugOp *slot = &common->worker.hotplug_queue[head % capacity];
  slot->type = type;
  slot->fd = fd;
  memcpy(slot->syspath, syspath, syspath_len + 1u);
  if (devnode) {
    memcpy(slot->devnode, devnode, devnode_len + 1u);
  } else {
    slot->devnode[0] = '\0';
  }

  head++;
  atomic_store_explicit(&common->worker.hotplug_head, head, memory_order_release);
  return true;
}

static bool _maru_linux_hotplug_try_dequeue(MARU_Context_Linux_Common *common,
                                            MARU_LinuxHotplugOp *out_op) {
  if (!common || !out_op) return false;

  const uint32_t tail = atomic_load_explicit(&common->worker.hotplug_tail,
                                             memory_order_relaxed);
  const uint32_t head = atomic_load_explicit(&common->worker.hotplug_head,
                                             memory_order_acquire);
  if (tail == head) return false;

  const uint32_t capacity = MARU_LINUX_HOTPLUG_QUEUE_CAPACITY;
  *out_op = common->worker.hotplug_queue[tail % capacity];
  atomic_store_explicit(&common->worker.hotplug_tail, tail + 1u,
                        memory_order_release);
  return true;
}

static void _maru_linux_hotplug_queue_cleanup(MARU_Context_Linux_Common *common) {
  if (!common) return;
  MARU_LinuxHotplugOp op;
  while (_maru_linux_hotplug_try_dequeue(common, &op)) {
    if (op.type == MARU_LINUX_HOTPLUG_OP_ADD && op.fd >= 0) {
      close(op.fd);
    }
  }
}

static void _maru_linux_worker_process_device(MARU_Context_Linux_Common* common, struct udev_device* dev, const char* action) {
  const char* devnode = common->worker.udev_lib.udev_device_get_devnode(dev);
  const char* syspath = common->worker.udev_lib.udev_device_get_syspath(dev);

  if (!devnode || !syspath) return;

  const char* joystick = common->worker.udev_lib.udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
  if (!joystick || strcmp(joystick, "1") != 0) return;

  if (!action || strcmp(action, "add") == 0) {
    int fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 && errno == EACCES) {
      fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
    
    if (fd < 0) return;

    if (!_maru_linux_is_gamepad(fd)) {
      close(fd);
      return;
    }

    (void)_maru_linux_hotplug_enqueue(common, MARU_LINUX_HOTPLUG_OP_ADD, fd,
                                      syspath, devnode);
    maru_wakeContext((MARU_Context*)common->ctx_base);

  } else if (action && strcmp(action, "remove") == 0) {
    (void)_maru_linux_hotplug_enqueue(common, MARU_LINUX_HOTPLUG_OP_REMOVE, -1,
                                      syspath, NULL);
    maru_wakeContext((MARU_Context*)common->ctx_base);
  }
}

static void _maru_linux_worker_process_udev_monitor(MARU_Context_Linux_Common* common) {
  struct udev_device* dev = common->worker.udev_lib.udev_monitor_receive_device(common->worker.udev_monitor);
  if (!dev) return;

  const char* action = common->worker.udev_lib.udev_device_get_action(dev);
  _maru_linux_worker_process_device(common, dev, action);

  common->worker.udev_lib.udev_device_unref(dev);
}

static void* _maru_linux_worker_main(void* arg) {
  MARU_Context_Linux_Common* common = (MARU_Context_Linux_Common*)arg;
  
  struct pollfd pfds[2];
  pfds[0].fd = common->worker.event_fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = common->worker.udev_fd;
  pfds[1].events = POLLIN;

  nfds_t nfds = (common->worker.udev_fd >= 0) ? 2 : 1;

  bool terminate = false;
  while (!terminate) {
    int ret = poll(pfds, nfds, -1);
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      break;
    }

    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      read(common->worker.event_fd, &val, sizeof(val));

      if (atomic_exchange(&common->worker.has_message, false)) {
        MARU_LinuxWorkerMessage msg = atomic_load(&common->worker.message);
        switch (msg) {
          case MARU_LINUX_WORKER_MSG_TERMINATE:
            terminate = true;
            break;
        }
      }
    }

    if (nfds > 1 && (pfds[1].revents & POLLIN)) {
      _maru_linux_worker_process_udev_monitor(common);
    }
  }

  return NULL;
}

bool _maru_linux_common_init(MARU_Context_Linux_Common* common, MARU_Context_Base* ctx_base) {
  common->ctx_base = ctx_base;
  common->worker.event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (common->worker.event_fd < 0) {
    return false;
  }

  common->worker.udev = NULL;
  common->worker.udev_monitor = NULL;
  common->worker.udev_fd = -1;
  atomic_init(&common->worker.hotplug_head, 0u);
  atomic_init(&common->worker.hotplug_tail, 0u);
  atomic_init(&common->worker.hotplug_overflowed, false);
  atomic_init(&common->worker.hotplug_resync_in_progress, false);
  atomic_init(&common->worker.has_message, false);
  common->worker.thread_started = false;

  if (maru_linux_udev_load(ctx_base, &common->worker.udev_lib)) {
    common->worker.udev = common->worker.udev_lib.udev_new();
    if (common->worker.udev) {
      common->worker.udev_monitor = common->worker.udev_lib.udev_monitor_new_from_netlink(common->worker.udev, "udev");
      if (common->worker.udev_monitor) {
        common->worker.udev_lib.udev_monitor_filter_add_match_subsystem_devtype(common->worker.udev_monitor, "input", NULL);
        common->worker.udev_lib.udev_monitor_enable_receiving(common->worker.udev_monitor);
        common->worker.udev_fd = common->worker.udev_lib.udev_monitor_get_fd(common->worker.udev_monitor);
      }
    }
  }

  return true;
}

bool _maru_linux_common_run(MARU_Context_Linux_Common* common) {
  if (common->worker.udev) {
    // Initial enumeration
    struct udev_enumerate* enumerate = common->worker.udev_lib.udev_enumerate_new(common->worker.udev);
    if (enumerate) {
      common->worker.udev_lib.udev_enumerate_add_match_subsystem(enumerate, "input");
      common->worker.udev_lib.udev_enumerate_scan_devices(enumerate);
      
      struct udev_list_entry* devices = common->worker.udev_lib.udev_enumerate_get_list_entry(enumerate);
      struct udev_list_entry* entry;

      for (entry = devices; entry != NULL; entry = common->worker.udev_lib.udev_list_entry_get_next(entry)) {
        const char* path = common->worker.udev_lib.udev_list_entry_get_name(entry);
        struct udev_device* dev = common->worker.udev_lib.udev_device_new_from_syspath(common->worker.udev, path);
        if (dev) {
          _maru_linux_worker_process_device(common, dev, NULL);
          common->worker.udev_lib.udev_device_unref(dev);
        }
      }
      common->worker.udev_lib.udev_enumerate_unref(enumerate);
    }
  }

  if (pthread_create(&common->worker.thread, NULL, _maru_linux_worker_main, common) != 0) {
    return false;
  }
  common->worker.thread_started = true;

  return true;
}

void _maru_linux_common_cleanup(MARU_Context_Linux_Common* common) {
  if (common->worker.thread_started) {
    atomic_store(&common->worker.message, MARU_LINUX_WORKER_MSG_TERMINATE);
    atomic_store(&common->worker.has_message, true);
    
    uint64_t val = 1;
    write(common->worker.event_fd, &val, sizeof(val));

    pthread_join(common->worker.thread, NULL);
    common->worker.thread_started = false;
  }

  if (common->worker.event_fd >= 0) {
    close(common->worker.event_fd);
    common->worker.event_fd = -1;
  }

  if (common->worker.udev_monitor) {
    common->worker.udev_lib.udev_monitor_unref(common->worker.udev_monitor);
    common->worker.udev_monitor = NULL;
  }
  if (common->worker.udev) {
    common->worker.udev_lib.udev_unref(common->worker.udev);
    common->worker.udev = NULL;
  }
  if (common->worker.udev_lib.base.available) {
    maru_linux_udev_unload(&common->worker.udev_lib);
  }
  _maru_linux_hotplug_queue_cleanup(common);
  atomic_store_explicit(&common->worker.hotplug_head, 0u, memory_order_relaxed);
  atomic_store_explicit(&common->worker.hotplug_tail, 0u, memory_order_relaxed);
  atomic_store_explicit(&common->worker.hotplug_overflowed, false, memory_order_relaxed);
  atomic_store_explicit(&common->worker.hotplug_resync_in_progress, false, memory_order_relaxed);

  while (common->controllers) {
    MARU_LinuxController* ctrl = common->controllers;
    common->controllers = ctrl->next;

    ctrl->is_active = false;
    ctrl->base.flags |= MARU_CONTROLLER_STATE_LOST;

    uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
    while (current > 0) {
      if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                                current - 1u, memory_order_acq_rel,
                                                memory_order_acquire)) {
        if (current == 1u) {
          _maru_linux_controller_destroy(common->ctx_base, ctrl);
        }
        break;
      }
    }
  }
}

void _maru_linux_controller_destroy(MARU_Context_Base* ctx_base, MARU_LinuxController* ctrl) {
  if (!ctx_base || !ctrl) return;

  if (ctrl->effect_id >= 0) {
    ioctl(ctrl->fd, (unsigned long)EVIOCRMFF, ctrl->effect_id);
  }

  if (ctrl->fd >= 0) {
    close(ctrl->fd);
  }
  maru_context_free(ctx_base, ctrl->syspath);
  maru_context_free(ctx_base, ctrl->devnode);
  maru_context_free(ctx_base, ctrl->name);
  for (uint32_t i = 0; i < ctrl->allocated_name_count; ++i) {
    maru_context_free(ctx_base, (void*)ctrl->allocated_names[i]);
  }
  maru_context_free(ctx_base, ctrl);
}

static char *_maru_linux_strdup_ctx(MARU_Context_Base *ctx_base, const char *src) {
  if (!ctx_base || !src) return NULL;
  const size_t len = strlen(src);
  char *dst = (char *)maru_context_alloc(ctx_base, len + 1u);
  if (!dst) return NULL;
  memcpy(dst, src, len + 1u);
  return dst;
}

static MARU_LinuxController* _maru_linux_controller_create(MARU_Context_Linux_Common* common,
                                                           int fd,
                                                           const char *syspath,
                                                           const char *devnode) {
  if (!common || fd < 0 || !syspath || !devnode) return NULL;

  _Static_assert(MARU_CONTROLLER_BUTTON_STANDARD_COUNT <= INT16_MAX,
                 "Controller button count must fit int16_t");
  _Static_assert(MARU_CONTROLLER_ANALOG_STANDARD_COUNT <= INT16_MAX,
                 "Controller analog count must fit int16_t");
  
  unsigned long ev_bits[EV_MAX / (8 * sizeof(unsigned long)) + 1] = {0};
  unsigned long key_bits[KEY_CNT / (8 * sizeof(unsigned long)) + 1] = {0};
  unsigned long abs_bits[ABS_CNT / (8 * sizeof(unsigned long)) + 1] = {0};
  unsigned long ff_bits[FF_MAX / (8 * sizeof(unsigned long)) + 1] = {0};

  if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) return NULL;
  ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
  ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
  ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits);

  // 1. Identify Standard Gamepad Buttons
  static const uint16_t std_buttons[] = {
    BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,
    BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE,
    BTN_THUMBL, BTN_THUMBR,
    BTN_DPAD_UP, BTN_DPAD_RIGHT, BTN_DPAD_DOWN, BTN_DPAD_LEFT
  };
  
  uint32_t btn_count = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;

  // 2. Identify Non-Standard Buttons
  for (int i = BTN_MISC; i < KEY_CNT; ++i) {
    if (TEST_BIT(i, key_bits)) {
      bool is_std = false;
      for (uint32_t j = 0; j < MARU_CONTROLLER_BUTTON_STANDARD_COUNT; ++j) {
        if (std_buttons[j] == i) { is_std = true; break; }
      }
      if (!is_std) {
        btn_count++;
      }
    }
  }

  // 3. Identify Standard Gamepad Axes
  static const uint16_t std_abs[] = {
    ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ
  };
  uint32_t abs_count = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;

  // 4. Identify Non-Standard Axes
  for (int i = 0; i < ABS_CNT; ++i) {
    if (TEST_BIT(i, abs_bits)) {
      bool is_std = false;
      for (uint32_t j = 0; j < MARU_CONTROLLER_ANALOG_STANDARD_COUNT; ++j) {
        if (std_abs[j] == i) { is_std = true; break; }
      }
      if (!is_std) {
        abs_count++;
      }
    }
  }

  // 5. Identify Haptics
  uint32_t haptic_count = 0;
  if (TEST_BIT(FF_RUMBLE, ff_bits)) {
    haptic_count = MARU_CONTROLLER_HAPTIC_STANDARD_COUNT;
  }

  // Calculate total bulk allocation size
  size_t total_size = sizeof(MARU_LinuxController);
  size_t align_mask = sizeof(void*) - 1;
  
  #define ALIGN_UP(size) (((size) + align_mask) & ~align_mask)
  
  total_size = ALIGN_UP(total_size);
  size_t btn_channels_offset = total_size;
  total_size += ALIGN_UP(btn_count * sizeof(MARU_ButtonChannelInfo));
  
  size_t btn_states_offset = total_size;
  total_size += ALIGN_UP(btn_count * sizeof(MARU_ButtonState8));
  
  size_t abs_channels_offset = total_size;
  total_size += ALIGN_UP(abs_count * sizeof(MARU_AnalogChannelInfo));
  
  size_t abs_states_offset = total_size;
  total_size += ALIGN_UP(abs_count * sizeof(MARU_AnalogInputState));

  size_t abs_info_offset = total_size;
  total_size += ALIGN_UP(abs_count * sizeof(struct input_absinfo));
  
  size_t haptic_channels_offset = total_size;
  total_size += ALIGN_UP(haptic_count * sizeof(MARU_HapticChannelInfo));
  
  uint32_t dynamic_name_capacity = 0u;
  if (btn_count > MARU_CONTROLLER_BUTTON_STANDARD_COUNT) {
    dynamic_name_capacity += (btn_count - MARU_CONTROLLER_BUTTON_STANDARD_COUNT);
  }
  if (abs_count > MARU_CONTROLLER_ANALOG_STANDARD_COUNT) {
    dynamic_name_capacity += (abs_count - MARU_CONTROLLER_ANALOG_STANDARD_COUNT);
  }

  size_t allocated_names_offset = total_size;
  total_size += ALIGN_UP(dynamic_name_capacity * sizeof(char*));

  MARU_LinuxController* ctrl = maru_context_alloc(common->ctx_base, total_size);
  if (!ctrl) return NULL;
  memset(ctrl, 0, total_size);

  uint8_t* base_ptr = (uint8_t*)ctrl;
  ctrl->button_channels = (MARU_ButtonChannelInfo*)(base_ptr + btn_channels_offset);
  ctrl->button_states = (MARU_ButtonState8*)(base_ptr + btn_states_offset);
  ctrl->analog_channels = (MARU_AnalogChannelInfo*)(base_ptr + abs_channels_offset);
  ctrl->analog_states = (MARU_AnalogInputState*)(base_ptr + abs_states_offset);
  ctrl->analog_abs_info = (struct input_absinfo *)(base_ptr + abs_info_offset);
  ctrl->haptic_channels = (MARU_HapticChannelInfo*)(base_ptr + haptic_channels_offset);
  ctrl->allocated_names = (char**)(base_ptr + allocated_names_offset);

  ctrl->fd = fd;
  ctrl->syspath = _maru_linux_strdup_ctx(common->ctx_base, syspath);
  ctrl->devnode = _maru_linux_strdup_ctx(common->ctx_base, devnode);
  if (!ctrl->syspath || !ctrl->devnode) {
    _maru_linux_controller_destroy(common->ctx_base, ctrl);
    return NULL;
  }
  ctrl->base.context = (MARU_Context*)common->ctx_base;
  ctrl->effect_id = -1;
  atomic_init(&ctrl->ref_count, 1u);
  ctrl->is_active = true;

  struct input_id id;
  if (ioctl(fd, (unsigned long)EVIOCGID, &id) == 0) {
    ctrl->vendor_id = id.vendor;
    ctrl->product_id = id.product;
    ctrl->version = id.version;
    // Basic Linux GUID (bus, vendor, product, version)
    memcpy(ctrl->guid, &id.bustype, 2);
    memcpy(ctrl->guid + 4, &id.vendor, 2);
    memcpy(ctrl->guid + 8, &id.product, 2);
    memcpy(ctrl->guid + 12, &id.version, 2);
  }

  // Initialize mappings
  for (int i = 0; i < KEY_CNT; ++i) ctrl->evdev_to_button[i] = -1;
  for (int i = 0; i < ABS_CNT; ++i) ctrl->evdev_to_analog[i] = -1;

  uint32_t current_btn = 0;
  for (uint32_t i = 0; i < MARU_CONTROLLER_BUTTON_STANDARD_COUNT; ++i) {
    if (TEST_BIT(std_buttons[i], key_bits)) {
      ctrl->evdev_to_button[std_buttons[i]] = (int16_t)i;
    }
  }
  current_btn = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;

  for (int i = BTN_MISC; i < KEY_CNT; ++i) {
    if (TEST_BIT(i, key_bits)) {
      bool is_std = false;
      for (uint32_t j = 0; j < MARU_CONTROLLER_BUTTON_STANDARD_COUNT; ++j) {
        if (std_buttons[j] == i) { is_std = true; break; }
      }
      if (!is_std) {
        ctrl->evdev_to_button[i] = (int16_t)current_btn++;
      }
    }
  }

  uint32_t current_abs = 0;
  for (uint32_t i = 0; i < MARU_CONTROLLER_ANALOG_STANDARD_COUNT; ++i) {
    if (TEST_BIT(std_abs[i], abs_bits)) {
      ctrl->evdev_to_analog[std_abs[i]] = (int16_t)i;
      ioctl(fd, (unsigned long)EVIOCGABS((unsigned int)std_abs[i]),
            &ctrl->analog_abs_info[i]);
    }
  }
  current_abs = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;

  for (int i = 0; i < ABS_CNT; ++i) {
    if (TEST_BIT(i, abs_bits)) {
      bool is_std = false;
      for (uint32_t j = 0; j < MARU_CONTROLLER_ANALOG_STANDARD_COUNT; ++j) {
        if (std_abs[j] == i) { is_std = true; break; }
      }
      if (!is_std) {
        const uint32_t idx = current_abs++;
        ctrl->evdev_to_analog[i] = (int16_t)idx;
        ioctl(fd, (unsigned long)EVIOCGABS((unsigned int)i),
              &ctrl->analog_abs_info[idx]);
      }
    }
  }

  // Final check for "Standardized" (at least 4 face buttons and 2 axes)
  ctrl->is_standardized = (TEST_BIT(BTN_SOUTH, key_bits) || TEST_BIT(BTN_GAMEPAD, key_bits)) &&
                          TEST_BIT(ABS_X, abs_bits) && TEST_BIT(ABS_Y, abs_bits);

  ctrl->base.button_count = btn_count;
  ctrl->base.buttons = ctrl->button_states;
  ctrl->base.button_channels = ctrl->button_channels;
  ctrl->base.analog_count = abs_count;
  ctrl->base.analogs = ctrl->analog_states;
  ctrl->base.analog_channels = ctrl->analog_channels;
  ctrl->base.haptic_count = haptic_count;
  ctrl->base.haptic_channels = ctrl->haptic_channels;

  // 5. Populate Labels
  static const char *std_button_names[] = {
    "South", "East", "West", "North", "LB", "RB", "Back", "Start", "Guide", "LThumb", "RThumb", "DpadUp", "DpadRight", "DpadDown", "DpadLeft"
  };
  static const char *std_analog_names[] = {
    "LeftX", "LeftY", "RightX", "RightY", "LTrigger", "RTrigger"
  };
  static const char *std_haptic_names[] = {
    "LowFreq", "HighFreq"
  };

  ctrl->allocated_name_count = 0;

  for (uint32_t i = 0; i < btn_count; ++i) {
    if (i < MARU_CONTROLLER_BUTTON_STANDARD_COUNT) {
      ctrl->button_channels[i].name = std_button_names[i];
    } else {
      // Find the evdev code for this non-standard button
      int code = -1;
      for (int c = 0; c < KEY_CNT; ++c) {
        if (ctrl->evdev_to_button[c] == (int)i) { code = c; break; }
      }
      char buf[32];
      snprintf(buf, sizeof(buf), "Button %d", code);
      char *n = _maru_linux_strdup_ctx(common->ctx_base, buf);
      if (!n) {
        _maru_linux_controller_destroy(common->ctx_base, ctrl);
        return NULL;
      }
      ctrl->button_channels[i].name = n;
      ctrl->allocated_names[ctrl->allocated_name_count++] = n;
    }
  }

  for (uint32_t i = 0; i < abs_count; ++i) {
    if (i < MARU_CONTROLLER_ANALOG_STANDARD_COUNT) {
      ctrl->analog_channels[i].name = std_analog_names[i];
    } else {
      int code = -1;
      for (int c = 0; c < ABS_CNT; ++c) {
        if (ctrl->evdev_to_analog[c] == (int)i) { code = c; break; }
      }
      char buf[32];
      snprintf(buf, sizeof(buf), "Axis %d", code);
      char *n = _maru_linux_strdup_ctx(common->ctx_base, buf);
      if (!n) {
        _maru_linux_controller_destroy(common->ctx_base, ctrl);
        return NULL;
      }
      ctrl->analog_channels[i].name = n;
      ctrl->allocated_names[ctrl->allocated_name_count++] = n;
    }
  }

  for (uint32_t i = 0; i < haptic_count; ++i) {
    ctrl->haptic_channels[i].name = std_haptic_names[i];
  }

  char name[256];
  if (ioctl(fd, (unsigned long)EVIOCGNAME((unsigned int)sizeof(name)), name) > 0) {
    ctrl->name = _maru_linux_strdup_ctx(common->ctx_base, name);
  } else {
    ctrl->name = _maru_linux_strdup_ctx(common->ctx_base, "Unknown Linux Controller");
  }

  if (!ctrl->name) {
    _maru_linux_controller_destroy(common->ctx_base, ctrl);
    return NULL;
  }

  return ctrl;
}

static MARU_LinuxController *_maru_linux_find_controller_by_syspath(
    MARU_Context_Linux_Common *common, const char *syspath) {
  if (!common || !syspath) return NULL;
  for (MARU_LinuxController *it = common->controllers; it; it = it->next) {
    if (it->syspath && strcmp(it->syspath, syspath) == 0) return it;
  }
  return NULL;
}

static void _maru_linux_emit_controller_connection_event(
    MARU_Context_Linux_Common *common, MARU_LinuxController *ctrl,
    bool connected) {
  if (!common || !ctrl) return;
  MARU_Event pub_evt = {0};
  pub_evt.controller_connection.controller = (MARU_Controller *)ctrl;
  pub_evt.controller_connection.connected = connected;
  _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_CONNECTION_CHANGED,
                       NULL, &pub_evt);
}

static void _maru_linux_remove_controller_by_syspath(
    MARU_Context_Linux_Common *common, const char *syspath) {
  if (!common || !syspath) return;

  MARU_LinuxController **curr = &common->controllers;
  while (*curr) {
    if ((*curr)->syspath && strcmp((*curr)->syspath, syspath) == 0) {
      MARU_LinuxController *to_remove = *curr;
      *curr = to_remove->next;
      common->controller_count--;

      _maru_linux_emit_controller_connection_event(common, to_remove, false);

      to_remove->is_active = false;
      to_remove->base.flags |= MARU_CONTROLLER_STATE_LOST;

      uint32_t current =
          atomic_load_explicit(&to_remove->ref_count, memory_order_acquire);
      while (current > 0) {
        if (atomic_compare_exchange_weak_explicit(
                &to_remove->ref_count, &current, current - 1u,
                memory_order_acq_rel, memory_order_acquire)) {
          if (current == 1u) {
            _maru_linux_controller_destroy(common->ctx_base, to_remove);
          }
          break;
        }
      }
      return;
    }
    curr = &((*curr)->next);
  }
}

static void _maru_linux_handle_hotplug_add(MARU_Context_Linux_Common *common,
                                           int fd, const char *syspath,
                                           const char *devnode) {
  if (!common || !syspath || !devnode) {
    if (fd >= 0) close(fd);
    return;
  }

  if (_maru_linux_find_controller_by_syspath(common, syspath)) {
    if (fd >= 0) close(fd);
    return;
  }

  MARU_LinuxController *ctrl =
      _maru_linux_controller_create(common, fd, syspath, devnode);
  if (!ctrl) {
    if (fd >= 0) close(fd);
    return;
  }

  ctrl->next = common->controllers;
  common->controllers = ctrl;
  common->controller_count++;
  _maru_linux_emit_controller_connection_event(common, ctrl, true);
}

static void _maru_linux_handle_hotplug_remove(MARU_Context_Linux_Common *common,
                                              const char *syspath) {
  _maru_linux_remove_controller_by_syspath(common, syspath);
}

typedef struct MARU_LinuxSnapshotDevice {
  int fd;
  char *syspath;
  char *devnode;
  struct MARU_LinuxSnapshotDevice *next;
} MARU_LinuxSnapshotDevice;

static void _maru_linux_snapshot_destroy(MARU_Context_Linux_Common *common,
                                         MARU_LinuxSnapshotDevice *head) {
  if (!common) return;
  while (head) {
    MARU_LinuxSnapshotDevice *next = head->next;
    if (head->fd >= 0) close(head->fd);
    maru_context_free(common->ctx_base, head->syspath);
    maru_context_free(common->ctx_base, head->devnode);
    maru_context_free(common->ctx_base, head);
    head = next;
  }
}

static MARU_LinuxSnapshotDevice *_maru_linux_snapshot_find(
    MARU_LinuxSnapshotDevice *head, const char *syspath) {
  for (MARU_LinuxSnapshotDevice *it = head; it; it = it->next) {
    if (it->syspath && syspath && strcmp(it->syspath, syspath) == 0) return it;
  }
  return NULL;
}

static bool _maru_linux_collect_snapshot(MARU_Context_Linux_Common *common,
                                         MARU_LinuxSnapshotDevice **out_head) {
  if (!common || !out_head) return false;
  *out_head = NULL;

  if (!common->worker.udev || !common->worker.udev_lib.base.available) {
    return false;
  }

  struct udev_enumerate *enumerate =
      common->worker.udev_lib.udev_enumerate_new(common->worker.udev);
  if (!enumerate) return false;

  common->worker.udev_lib.udev_enumerate_add_match_subsystem(enumerate, "input");
  common->worker.udev_lib.udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *devices =
      common->worker.udev_lib.udev_enumerate_get_list_entry(enumerate);
  for (struct udev_list_entry *entry = devices; entry != NULL;
       entry = common->worker.udev_lib.udev_list_entry_get_next(entry)) {
    const char *path = common->worker.udev_lib.udev_list_entry_get_name(entry);
    struct udev_device *dev =
        common->worker.udev_lib.udev_device_new_from_syspath(common->worker.udev,
                                                              path);
    if (!dev) continue;

    const char *devnode =
        common->worker.udev_lib.udev_device_get_devnode(dev);
    const char *syspath =
        common->worker.udev_lib.udev_device_get_syspath(dev);
    const char *joystick =
        common->worker.udev_lib.udev_device_get_property_value(dev,
                                                                "ID_INPUT_JOYSTICK");

    if (!devnode || !syspath || !joystick || strcmp(joystick, "1") != 0) {
      common->worker.udev_lib.udev_device_unref(dev);
      continue;
    }

    int fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 && errno == EACCES) {
      fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd < 0 || !_maru_linux_is_gamepad(fd)) {
      if (fd >= 0) close(fd);
      common->worker.udev_lib.udev_device_unref(dev);
      continue;
    }

    if (_maru_linux_snapshot_find(*out_head, syspath)) {
      close(fd);
      common->worker.udev_lib.udev_device_unref(dev);
      continue;
    }

    MARU_LinuxSnapshotDevice *snap =
        (MARU_LinuxSnapshotDevice *)maru_context_alloc(common->ctx_base,
                                                       sizeof(*snap));
    if (!snap) {
      close(fd);
      common->worker.udev_lib.udev_device_unref(dev);
      common->worker.udev_lib.udev_enumerate_unref(enumerate);
      _maru_linux_snapshot_destroy(common, *out_head);
      *out_head = NULL;
      return false;
    }
    memset(snap, 0, sizeof(*snap));

    snap->syspath = _maru_linux_strdup_ctx(common->ctx_base, syspath);
    snap->devnode = _maru_linux_strdup_ctx(common->ctx_base, devnode);
    if (!snap->syspath || !snap->devnode) {
      if (fd >= 0) close(fd);
      maru_context_free(common->ctx_base, snap->syspath);
      maru_context_free(common->ctx_base, snap->devnode);
      maru_context_free(common->ctx_base, snap);
      common->worker.udev_lib.udev_device_unref(dev);
      common->worker.udev_lib.udev_enumerate_unref(enumerate);
      _maru_linux_snapshot_destroy(common, *out_head);
      *out_head = NULL;
      return false;
    }

    snap->fd = fd;
    snap->next = *out_head;
    *out_head = snap;

    common->worker.udev_lib.udev_device_unref(dev);
  }

  common->worker.udev_lib.udev_enumerate_unref(enumerate);
  return true;
}

static void _maru_linux_common_resync_controllers(MARU_Context_Linux_Common *common) {
  if (!common) return;

  atomic_store_explicit(&common->worker.hotplug_resync_in_progress, true,
                        memory_order_release);

  MARU_LinuxSnapshotDevice *snapshot = NULL;
  if (!_maru_linux_collect_snapshot(common, &snapshot)) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)common->ctx_base,
                           MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Controller hotplug resync failed; will retry");
    atomic_store_explicit(&common->worker.hotplug_overflowed, true,
                          memory_order_release);
    atomic_store_explicit(&common->worker.hotplug_resync_in_progress, false,
                          memory_order_release);
    return;
  }

  for (MARU_LinuxSnapshotDevice *it = snapshot; it; it = it->next) {
    if (_maru_linux_find_controller_by_syspath(common, it->syspath)) {
      if (it->fd >= 0) {
        close(it->fd);
        it->fd = -1;
      }
      continue;
    }
    _maru_linux_handle_hotplug_add(common, it->fd, it->syspath, it->devnode);
    it->fd = -1;
  }

  MARU_LinuxController *it = common->controllers;
  while (it) {
    MARU_LinuxController *next = it->next;
    if (!_maru_linux_snapshot_find(snapshot, it->syspath)) {
      _maru_linux_handle_hotplug_remove(common, it->syspath);
    }
    it = next;
  }

  _maru_linux_snapshot_destroy(common, snapshot);
  atomic_store_explicit(&common->worker.hotplug_resync_in_progress, false,
                        memory_order_release);
}

void _maru_linux_common_handle_internal_event(MARU_Context_Linux_Common *common,
                                              MARU_InternalEventId type,
                                              MARU_Window *window,
                                              const MARU_Event *event) {
  (void)window;
  if (!common || !event) return;

  // Legacy compatibility path: internal queued events are no longer produced by
  // the worker thread, but we still accept them if present.
  if (type == MARU_EVENT_INTERNAL_LINUX_DEVICE_ADD) {
    const MARU_LinuxDeviceDiscovery *discovery =
        (const MARU_LinuxDeviceDiscovery *)event->user.userdata;
    if (!discovery) return;
    _maru_linux_handle_hotplug_add(common, discovery->fd, discovery->syspath,
                                   discovery->devnode);
    free(discovery->syspath);
    free(discovery->devnode);
    free((void *)discovery);
  } else if (type == MARU_EVENT_INTERNAL_LINUX_DEVICE_REMOVE) {
    const char *syspath = (const char *)event->user.userdata;
    _maru_linux_handle_hotplug_remove(common, syspath);
    free((void *)syspath);
  }
}

uint32_t _maru_linux_common_fill_pollfds(MARU_Context_Linux_Common *common, struct pollfd *fds, uint32_t max_fds) {
  uint32_t count = 0;
  for (MARU_LinuxController *it = common->controllers; it && count < max_fds; it = it->next) {
    fds[count].fd = it->fd;
    fds[count].events = POLLIN;
    fds[count].revents = 0;
    count++;
  }
  return count;
}

void _maru_linux_common_process_pollfds(MARU_Context_Linux_Common *common, const struct pollfd *fds, uint32_t count) {
  for (MARU_LinuxController *it = common->controllers; it; it = it->next) {
    const struct pollfd *pfd = NULL;
    for (uint32_t i = 0; i < count; ++i) {
      if (fds[i].fd == it->fd) {
        pfd = &fds[i];
        break;
      }
    }

    if (pfd && (pfd->revents & POLLIN)) {
      struct input_event ev;
      while (read(it->fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
          if (ev.code >= KEY_CNT) {
            continue;
          }
          int idx = it->evdev_to_button[ev.code];
          if (idx >= 0) {
            MARU_ButtonState8 new_state = (MARU_ButtonState8)((ev.value != 0) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED);
            if (it->button_states[idx] != new_state) {
              it->button_states[idx] = new_state;
              
              MARU_Event pub_evt = {0};
              pub_evt.controller_button_state_changed.controller = (MARU_Controller*)it;
              pub_evt.controller_button_state_changed.button_id = (uint32_t)idx;
              pub_evt.controller_button_state_changed.state = (MARU_ButtonState)new_state;
              _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &pub_evt);
            }
          }
        } else if (ev.type == EV_ABS) {
          if (ev.code >= ABS_CNT) {
            continue;
          }
          int idx = it->evdev_to_analog[ev.code];
          if (idx >= 0) {
            struct input_absinfo *info = &it->analog_abs_info[idx];
            MARU_Scalar value = (MARU_Scalar)0.0;
            if (info->maximum != info->minimum) {
              value = (MARU_Scalar)(ev.value - info->minimum) / (MARU_Scalar)(info->maximum - info->minimum);
              
              if (idx == MARU_CONTROLLER_ANALOG_LEFT_X || idx == MARU_CONTROLLER_ANALOG_RIGHT_X) {
                value = value * (MARU_Scalar)2.0 - (MARU_Scalar)1.0;
              } else if (idx == MARU_CONTROLLER_ANALOG_LEFT_Y || idx == MARU_CONTROLLER_ANALOG_RIGHT_Y) {
                // Invert Y: evdev is Up=Negative, Maru is Up=Positive
                value = (MARU_Scalar)1.0 - (value * (MARU_Scalar)2.0);
              }
              // Triggers (4, 5) and non-standard axes remain in 0..1 or their raw normalized range.
            }
            it->analog_states[idx].value = value;
          }
        }

        // Special handling for Hat -> DPAD
        if (ev.type == EV_ABS && (ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y)) {
          // Only map if the corresponding buttons were NOT found during discovery
          if (ev.code == ABS_HAT0X) {
            uint32_t left_id = MARU_CONTROLLER_BUTTON_DPAD_LEFT;
            uint32_t right_id = MARU_CONTROLLER_BUTTON_DPAD_RIGHT;
            
            if (it->evdev_to_button[BTN_DPAD_LEFT] == -1) {
              MARU_ButtonState8 left_state = (MARU_ButtonState8)((ev.value < 0) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED);
              MARU_ButtonState8 right_state = (MARU_ButtonState8)((ev.value > 0) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED);
              
              if (it->button_states[left_id] != left_state) {
                it->button_states[left_id] = left_state;
                MARU_Event pub_evt = {0};
                pub_evt.controller_button_state_changed.controller = (MARU_Controller*)it;
                pub_evt.controller_button_state_changed.button_id = left_id;
                pub_evt.controller_button_state_changed.state = (MARU_ButtonState)left_state;
                _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &pub_evt);
              }
              if (it->button_states[right_id] != right_state) {
                it->button_states[right_id] = right_state;
                MARU_Event pub_evt = {0};
                pub_evt.controller_button_state_changed.controller = (MARU_Controller*)it;
                pub_evt.controller_button_state_changed.button_id = right_id;
                pub_evt.controller_button_state_changed.state = (MARU_ButtonState)right_state;
                _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &pub_evt);
              }
            }
          } else if (ev.code == ABS_HAT0Y) {
            uint32_t up_id = MARU_CONTROLLER_BUTTON_DPAD_UP;
            uint32_t down_id = MARU_CONTROLLER_BUTTON_DPAD_DOWN;

            if (it->evdev_to_button[BTN_DPAD_UP] == -1) {
              MARU_ButtonState8 up_state = (MARU_ButtonState8)((ev.value < 0) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED);
              MARU_ButtonState8 down_state = (MARU_ButtonState8)((ev.value > 0) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED);

              if (it->button_states[up_id] != up_state) {
                it->button_states[up_id] = up_state;
                MARU_Event pub_evt = {0};
                pub_evt.controller_button_state_changed.controller = (MARU_Controller*)it;
                pub_evt.controller_button_state_changed.button_id = up_id;
                pub_evt.controller_button_state_changed.state = (MARU_ButtonState)up_state;
                _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &pub_evt);
              }
              if (it->button_states[down_id] != down_state) {
                it->button_states[down_id] = down_state;
                MARU_Event pub_evt = {0};
                pub_evt.controller_button_state_changed.controller = (MARU_Controller*)it;
                pub_evt.controller_button_state_changed.button_id = down_id;
                pub_evt.controller_button_state_changed.state = (MARU_ButtonState)down_state;
                _maru_dispatch_event(common->ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED, NULL, &pub_evt);
              }
            }
          }
        }
      }
    }
  }
}

void _maru_linux_common_drain_internal_events(MARU_Context_Linux_Common *common) {
  if (!common || !common->ctx_base) return;

  MARU_LinuxHotplugOp op;
  while (_maru_linux_hotplug_try_dequeue(common, &op)) {
    if (op.type == MARU_LINUX_HOTPLUG_OP_ADD) {
      _maru_linux_handle_hotplug_add(common, op.fd, op.syspath, op.devnode);
    } else {
      _maru_linux_handle_hotplug_remove(common, op.syspath);
    }
  }

  bool overflowed = atomic_exchange_explicit(&common->worker.hotplug_overflowed,
                                             false, memory_order_acq_rel);
  if (overflowed) {
    uint32_t guard = 0u;
    do {
      _maru_linux_common_resync_controllers(common);
      while (_maru_linux_hotplug_try_dequeue(common, &op)) {
        if (op.type == MARU_LINUX_HOTPLUG_OP_ADD) {
          _maru_linux_handle_hotplug_add(common, op.fd, op.syspath, op.devnode);
        } else {
          _maru_linux_handle_hotplug_remove(common, op.syspath);
        }
      }

      overflowed = atomic_exchange_explicit(&common->worker.hotplug_overflowed,
                                            false, memory_order_acq_rel);
      guard++;
    } while (overflowed && guard < 4u);
  }

  MARU_EventId type;
  MARU_Window *window;
  MARU_Event evt;

  while (_maru_event_queue_pop(&common->ctx_base->queued_events, &type, &window, &evt)) {
    if (type >= (MARU_EventId)1000) {
      _maru_linux_common_handle_internal_event(common, (MARU_InternalEventId)type, window, &evt);
    } else {
      _maru_event_queue_push(&common->ctx_base->queued_events, type, window, evt);
      break; // Stop at first non-internal event to avoid infinite loop
    }
  }
}

MARU_Status _maru_linux_common_set_haptic_levels(MARU_Context_Linux_Common *common, MARU_LinuxController *ctrl, uint32_t first_haptic, uint32_t count, const MARU_Scalar *intensities) {
  (void)common;
  if (!ctrl || ctrl->fd < 0 || ctrl->base.haptic_count == 0) return MARU_FAILURE;

  for (uint32_t i = 0; i < count; ++i) {
    uint32_t id = first_haptic + i;
    if (id < MARU_CONTROLLER_HAPTIC_STANDARD_COUNT) {
      if (ctrl->last_haptic_levels[id] != intensities[i]) {
        ctrl->last_haptic_levels[id] = intensities[i];
        ctrl->haptics_dirty = true;
      }
    }
  }

  if (!ctrl->haptics_dirty) return MARU_SUCCESS;

  struct ff_effect effect;
  memset(&effect, 0, sizeof(effect));
  effect.type = FF_RUMBLE;
  effect.id = (int16_t)ctrl->effect_id;
  effect.replay.length = 0; // Infinite
  effect.replay.delay = 0;

  // intensities are MARU_Scalar (usually 0.0 to 1.0)
  // ff_rumble expects 0 to 0xFFFF
  effect.u.rumble.strong_magnitude = (uint16_t)(ctrl->last_haptic_levels[MARU_CONTROLLER_HAPTIC_LOW_FREQ] * (MARU_Scalar)0xFFFF);
  effect.u.rumble.weak_magnitude = (uint16_t)(ctrl->last_haptic_levels[MARU_CONTROLLER_HAPTIC_HIGH_FREQ] * (MARU_Scalar)0xFFFF);

  if (ioctl(ctrl->fd, (unsigned long)EVIOCSFF, &effect) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)common->ctx_base, MARU_DIAGNOSTIC_BACKEND_FAILURE, "Failed to upload force-feedback effect");
    return MARU_FAILURE;
  }

  ctrl->effect_id = effect.id;

  // Play the effect
  struct input_event play;
  memset(&play, 0, sizeof(play));
  play.type = EV_FF;
  play.code = (uint16_t)ctrl->effect_id;
  play.value = 1;

  if (write(ctrl->fd, &play, sizeof(play)) < 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)common->ctx_base, MARU_DIAGNOSTIC_BACKEND_FAILURE, "Failed to play force-feedback effect");
    return MARU_FAILURE;
  }

  ctrl->haptics_dirty = false;
  return MARU_SUCCESS;
}
