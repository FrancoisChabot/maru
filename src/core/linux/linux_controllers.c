#include "linux_controllers.h"
#include "dlib/linux_loader.h"
#include "maru/c/details/controllers.h"
#include "maru_mem_internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef enum MARU_WL_ButtonSourceKind {
  MARU_WL_BUTTON_SOURCE_NONE = 0,
  MARU_WL_BUTTON_SOURCE_KEY = 1,
  MARU_WL_BUTTON_SOURCE_HAT_X_NEG = 2,
  MARU_WL_BUTTON_SOURCE_HAT_X_POS = 3,
  MARU_WL_BUTTON_SOURCE_HAT_Y_NEG = 4,
  MARU_WL_BUTTON_SOURCE_HAT_Y_POS = 5,
} MARU_WL_ButtonSourceKind;

typedef struct MARU_WL_ButtonSource {
  MARU_WL_ButtonSourceKind kind;
  uint16_t code;
} MARU_WL_ButtonSource;

typedef struct MARU_WL_AnalogSource {
  uint16_t code;
  bool available;
  bool is_trigger;
  int32_t min;
  int32_t max;
} MARU_WL_AnalogSource;

typedef struct MARU_Controller_Linux MARU_Controller_Linux;

struct MARU_Controller_Linux {
  MARU_ControllerExposed pub;
  MARU_ControllerContext_Linux *ctrl_ctx;
  _Atomic uint32_t ref_count;
  MARU_ControllerInfo info;
  MARU_ControllerMetrics metrics;

  int fd;
  int rumble_effect_id;
  bool connected;
  bool seen_in_scan;
  bool has_hat;
  int32_t hat_x;
  int32_t hat_y;

  char *path;
  char *name_storage;
  char **extra_analog_names;
  char **extra_button_names;

  MARU_AnalogChannelInfo *analog_channels;
  MARU_AnalogInputState *analog_states;
  MARU_WL_AnalogSource *analog_sources;

  MARU_ButtonChannelInfo *button_channels;
  MARU_ButtonState8 *button_states;
  MARU_WL_ButtonSource *button_sources;
  MARU_HapticChannelInfo haptic_channels[MARU_CONTROLLER_HAPTIC_STANDARD_COUNT];

  struct MARU_Controller_Linux *next;
};

static size_t _wl_bit_word(size_t bit) {
  return bit / (sizeof(unsigned long) * 8u);
}

static bool _wl_test_bit(const unsigned long *bits, size_t bit) {
  const size_t word = _wl_bit_word(bit);
  const size_t offset = bit % (sizeof(unsigned long) * 8u);
  const unsigned long mask = 1ul << offset;
  return (bits[word] & mask) != 0ul;
}

static MARU_Scalar _wl_normalize_axis(const MARU_WL_AnalogSource *src,
                                      int32_t value) {
  if (!src->available || src->max <= src->min) {
    return (MARU_Scalar)0;
  }

  const double min = (double)src->min;
  const double max = (double)src->max;
  const double v = (double)value;
  if (src->is_trigger) {
    const double t = (v - min) / (max - min);
    if (t <= 0.0) return (MARU_Scalar)0;
    if (t >= 1.0) return (MARU_Scalar)1;
    return (MARU_Scalar)t;
  }

  const double center = (min + max) * 0.5;
  const double half = (max - min) * 0.5;
  if (half <= 0.0) return (MARU_Scalar)0;
  const double n = (v - center) / half;
  if (n <= -1.0) return (MARU_Scalar)-1;
  if (n >= 1.0) return (MARU_Scalar)1;
  return (MARU_Scalar)n;
}

static void _wl_dispatch_controller_connection(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
                                               MARU_Controller_Linux *controller,
                                               bool connected) {
  MARU_Event evt = {0};
  evt.controller_connection.controller = (MARU_Controller *)controller;
  evt.controller_connection.connected = connected;
  _maru_dispatch_event(ctx_base, MARU_EVENT_CONTROLLER_CONNECTION_CHANGED,
                       NULL, &evt);
}

static void _wl_dispatch_controller_button(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
                                           MARU_Controller_Linux *controller,
                                           uint32_t button_id,
                                           MARU_ButtonState state) {
  MARU_Event evt = {0};
  evt.controller_button_state_changed.controller = (MARU_Controller *)controller;
  evt.controller_button_state_changed.button_id = button_id;
  evt.controller_button_state_changed.state = state;
  _maru_dispatch_event(ctx_base, MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED,
                       NULL, &evt);
}

static void _wl_free_controller(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, MARU_Controller_Linux *ctrl) {
  if (!ctrl) return;
  if (ctrl->fd >= 0) {
    if (ctrl->rumble_effect_id >= 0) {
      (void)ioctl(ctrl->fd, EVIOCRMFF, ctrl->rumble_effect_id);
      ctrl->rumble_effect_id = -1;
    }
    close(ctrl->fd);
    ctrl->fd = -1;
  }
  if (ctrl->name_storage) {
    maru_context_free(ctx_base, ctrl->name_storage);
  }
  if (ctrl->path) {
    maru_context_free(ctx_base, ctrl->path);
  }
  if (ctrl->extra_analog_names && ctrl->pub.analog_count > MARU_CONTROLLER_ANALOG_STANDARD_COUNT) {
    const uint32_t start = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;
    for (uint32_t i = start; i < ctrl->pub.analog_count; ++i) {
      const uint32_t idx = i - start;
      if (ctrl->extra_analog_names[idx]) {
        maru_context_free(ctx_base, ctrl->extra_analog_names[idx]);
      }
    }
    maru_context_free(ctx_base, ctrl->extra_analog_names);
  }
  if (ctrl->extra_button_names && ctrl->pub.button_count > MARU_CONTROLLER_BUTTON_STANDARD_COUNT) {
    const uint32_t start = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;
    for (uint32_t i = start; i < ctrl->pub.button_count; ++i) {
      const uint32_t idx = i - start;
      if (ctrl->extra_button_names[idx]) {
        maru_context_free(ctx_base, ctrl->extra_button_names[idx]);
      }
    }
    maru_context_free(ctx_base, ctrl->extra_button_names);
  }
  if (ctrl->analog_sources) maru_context_free(ctx_base, ctrl->analog_sources);
  if (ctrl->analog_states) maru_context_free(ctx_base, ctrl->analog_states);
  if (ctrl->analog_channels) maru_context_free(ctx_base, ctrl->analog_channels);
  if (ctrl->button_sources) maru_context_free(ctx_base, ctrl->button_sources);
  if (ctrl->button_states) maru_context_free(ctx_base, ctrl->button_states);
  if (ctrl->button_channels) maru_context_free(ctx_base, ctrl->button_channels);
  maru_context_free(ctx_base, ctrl);
}

static MARU_Controller_Linux *_wl_find_controller_by_path(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
                                                       const char *path) {
  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    if (it->path && strcmp(it->path, path) == 0) {
      return it;
    }
  }
  return NULL;
}

static bool _wl_device_is_controller(int fd) {
  unsigned long ev_bits[_wl_bit_word((size_t)EV_MAX + 1u) + 1u];
  unsigned long key_bits[_wl_bit_word((size_t)KEY_MAX + 1u) + 1u];
  memset(ev_bits, 0, sizeof(ev_bits));
  memset(key_bits, 0, sizeof(key_bits));

  if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
    return false;
  }
  if (!_wl_test_bit(ev_bits, EV_KEY)) {
    return false;
  }
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
    return false;
  }

  if (_wl_test_bit(key_bits, BTN_GAMEPAD) || _wl_test_bit(key_bits, BTN_SOUTH) ||
      _wl_test_bit(key_bits, BTN_A) || _wl_test_bit(key_bits, BTN_EAST) ||
      _wl_test_bit(key_bits, BTN_B)) {
    return true;
  }
  return false;
}

static bool _wl_probe_rumble_capable(int fd) {
  return false;
  // unsigned long ff_bits[(FF_MAX / (8 * sizeof(unsigned long))) + 1u];
  // memset(ff_bits, 0, sizeof(ff_bits));
  // if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits) == 0) {
  //   const bool has_rumble =
  //       (ff_bits[FF_RUMBLE / (8 * sizeof(unsigned long))] &
  //        (1ul << (FF_RUMBLE % (8 * sizeof(unsigned long))))) != 0ul;
  //   if (has_rumble) {
  //     return true;
  //   }
  // }

  // struct ff_effect probe;
  // memset(&probe, 0, sizeof(probe));
  // probe.type = FF_RUMBLE;
  // probe.id = -1;
  // probe.u.rumble.strong_magnitude = 0u;
  // probe.u.rumble.weak_magnitude = 0u;
  // probe.replay.length = 0u;
  // probe.replay.delay = 0u;
  // if (ioctl(fd, EVIOCSFF, &probe) < 0) {
  //   return false;
  // }
  // if (probe.id >= 0) {
  //   (void)ioctl(fd, EVIOCRMFF, probe.id);
  // }
  // return true;
}

static uint16_t _wl_pick_button_code(const unsigned long *key_bits,
                                     uint16_t primary, uint16_t fallback) {
  if (_wl_test_bit(key_bits, primary)) return primary;
  if (fallback != 0u && _wl_test_bit(key_bits, fallback)) return fallback;
  return 0u;
}

static bool _wl_code_in_list_u16(const uint16_t *list, uint32_t count, uint16_t value) {
  for (uint32_t i = 0; i < count; ++i) {
    if (list[i] == value) return true;
  }
  return false;
}

static bool _wl_set_extra_name(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, char ***name_array, uint32_t index,
                               const char *prefix, uint32_t code, const char **out_name) {
  char buffer[32];
  const int nw = snprintf(buffer, sizeof(buffer), "%s_%u", prefix, code);
  if (nw <= 0 || (size_t)nw >= sizeof(buffer)) return false;
  const size_t len = (size_t)nw + 1u;
  char *name = (char *)maru_context_alloc(ctx_base, len);
  if (!name) return false;
  memcpy(name, buffer, len);
  (*name_array)[index] = name;
  *out_name = name;
  return true;
}

static bool _wl_init_controller_sources(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, MARU_Controller_Linux *ctrl) {
  unsigned long abs_bits[_wl_bit_word((size_t)ABS_MAX + 1u) + 1u];
  unsigned long key_bits[_wl_bit_word((size_t)KEY_MAX + 1u) + 1u];
  memset(abs_bits, 0, sizeof(abs_bits));
  memset(key_bits, 0, sizeof(key_bits));
  (void)ioctl(ctrl->fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
  (void)ioctl(ctrl->fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);

  static const char *standard_analog_names[MARU_CONTROLLER_ANALOG_STANDARD_COUNT] = {
      "left_x", "left_y", "right_x", "right_y", "left_trigger", "right_trigger"};
  static const char *standard_button_names[MARU_CONTROLLER_BUTTON_STANDARD_COUNT] = {
      "south", "east", "west", "north", "left_bumper", "right_bumper",
      "back", "start", "guide", "left_stick", "right_stick",
      "dpad_up", "dpad_right", "dpad_down", "dpad_left"};

  const uint16_t axis_primary[MARU_CONTROLLER_ANALOG_STANDARD_COUNT] = {
      ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ};
  const uint16_t axis_fallback[MARU_CONTROLLER_ANALOG_STANDARD_COUNT] = {
      0u, 0u, 0u, 0u, ABS_BRAKE, ABS_GAS};
  uint16_t standard_axis_codes[MARU_CONTROLLER_ANALOG_STANDARD_COUNT];
  for (uint32_t i = 0; i < MARU_CONTROLLER_ANALOG_STANDARD_COUNT; ++i) {
    uint16_t code = axis_primary[i];
    if (!_wl_test_bit(abs_bits, code) && axis_fallback[i] != 0u &&
        _wl_test_bit(abs_bits, axis_fallback[i])) {
      code = axis_fallback[i];
    }
    standard_axis_codes[i] = code;
  }

  const uint16_t map_primary[MARU_CONTROLLER_BUTTON_STANDARD_COUNT] = {
      BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH, BTN_TL, BTN_TR, BTN_SELECT,
      BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, BTN_DPAD_UP, BTN_DPAD_RIGHT,
      BTN_DPAD_DOWN, BTN_DPAD_LEFT};
  const uint16_t map_fallback[MARU_CONTROLLER_BUTTON_STANDARD_COUNT] = {
      BTN_A, BTN_B, BTN_X, BTN_Y, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  uint16_t standard_button_codes[MARU_CONTROLLER_BUTTON_STANDARD_COUNT];
  for (uint32_t i = 0; i < MARU_CONTROLLER_BUTTON_STANDARD_COUNT; ++i) {
    standard_button_codes[i] = _wl_pick_button_code(key_bits, map_primary[i], map_fallback[i]);
  }

  uint32_t extra_analog_count = 0;
  for (uint32_t code = 0; code <= (uint32_t)ABS_MAX; ++code) {
    if (!_wl_test_bit(abs_bits, code)) continue;
    if (_wl_code_in_list_u16(standard_axis_codes, MARU_CONTROLLER_ANALOG_STANDARD_COUNT,
                             (uint16_t)code)) {
      continue;
    }
    extra_analog_count++;
  }

  uint32_t extra_button_count = 0;
  for (uint32_t code = BTN_MISC; code <= (uint32_t)KEY_MAX; ++code) {
    if (!_wl_test_bit(key_bits, code)) continue;
    if (_wl_code_in_list_u16(standard_button_codes, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
                             (uint16_t)code)) {
      continue;
    }
    extra_button_count++;
  }

  const uint32_t analog_count = MARU_CONTROLLER_ANALOG_STANDARD_COUNT + extra_analog_count;
  const uint32_t button_count = MARU_CONTROLLER_BUTTON_STANDARD_COUNT + extra_button_count;
  ctrl->analog_channels = (MARU_AnalogChannelInfo *)maru_context_alloc(
      ctx_base, sizeof(MARU_AnalogChannelInfo) * analog_count);
  ctrl->analog_states = (MARU_AnalogInputState *)maru_context_alloc(
      ctx_base, sizeof(MARU_AnalogInputState) * analog_count);
  ctrl->analog_sources = (MARU_WL_AnalogSource *)maru_context_alloc(
      ctx_base, sizeof(MARU_WL_AnalogSource) * analog_count);
  ctrl->button_channels = (MARU_ButtonChannelInfo *)maru_context_alloc(
      ctx_base, sizeof(MARU_ButtonChannelInfo) * button_count);
  ctrl->button_states = (MARU_ButtonState8 *)maru_context_alloc(
      ctx_base, sizeof(MARU_ButtonState8) * button_count);
  ctrl->button_sources = (MARU_WL_ButtonSource *)maru_context_alloc(
      ctx_base, sizeof(MARU_WL_ButtonSource) * button_count);
  if (!ctrl->analog_channels || !ctrl->analog_states || !ctrl->analog_sources ||
      !ctrl->button_channels || !ctrl->button_states || !ctrl->button_sources) {
    return false;
  }
  ctrl->pub.analog_channels = ctrl->analog_channels;
  ctrl->pub.analogs = ctrl->analog_states;
  ctrl->pub.analog_count = analog_count;
  ctrl->pub.button_channels = ctrl->button_channels;
  ctrl->pub.buttons = ctrl->button_states;
  ctrl->pub.button_count = button_count;
  memset(ctrl->analog_states, 0, sizeof(MARU_AnalogInputState) * analog_count);
  memset(ctrl->analog_sources, 0, sizeof(MARU_WL_AnalogSource) * analog_count);
  memset(ctrl->button_states, 0, sizeof(MARU_ButtonState8) * button_count);
  memset(ctrl->button_sources, 0, sizeof(MARU_WL_ButtonSource) * button_count);

  if (extra_analog_count > 0) {
    ctrl->extra_analog_names =
        (char **)maru_context_alloc(ctx_base, sizeof(char *) * extra_analog_count);
    if (!ctrl->extra_analog_names) return false;
    memset(ctrl->extra_analog_names, 0, sizeof(char *) * extra_analog_count);
  }
  if (extra_button_count > 0) {
    ctrl->extra_button_names =
        (char **)maru_context_alloc(ctx_base, sizeof(char *) * extra_button_count);
    if (!ctrl->extra_button_names) return false;
    memset(ctrl->extra_button_names, 0, sizeof(char *) * extra_button_count);
  }

  for (uint32_t i = 0; i < MARU_CONTROLLER_ANALOG_STANDARD_COUNT; ++i) {
    ctrl->analog_channels[i].name = standard_analog_names[i];
    MARU_WL_AnalogSource *src = &ctrl->analog_sources[i];
    const uint16_t code = standard_axis_codes[i];
    src->code = code;
    src->available = _wl_test_bit(abs_bits, code);
    src->is_trigger = (i >= MARU_CONTROLLER_ANALOG_LEFT_TRIGGER);
    src->min = 0;
    src->max = 0;
    ctrl->analog_states[i].value = 0;

    if (src->available) {
      struct input_absinfo abs_info;
      memset(&abs_info, 0, sizeof(abs_info));
      if (ioctl(ctrl->fd, EVIOCGABS((unsigned int)code), &abs_info) == 0) {
        src->min = abs_info.minimum;
        src->max = abs_info.maximum;
        ctrl->analog_states[i].value = _wl_normalize_axis(src, abs_info.value);
      } else {
        src->available = false;
      }
    }
  }

  for (uint32_t i = 0; i < MARU_CONTROLLER_BUTTON_STANDARD_COUNT; ++i) {
    ctrl->button_channels[i].name = standard_button_names[i];
    const uint16_t code = standard_button_codes[i];
    ctrl->button_sources[i].kind =
        (code != 0u) ? MARU_WL_BUTTON_SOURCE_KEY : MARU_WL_BUTTON_SOURCE_NONE;
    ctrl->button_sources[i].code = code;
    ctrl->button_states[i] = MARU_BUTTON_STATE_RELEASED;
  }

  ctrl->has_hat = _wl_test_bit(abs_bits, ABS_HAT0X) && _wl_test_bit(abs_bits, ABS_HAT0Y);
  ctrl->hat_x = 0;
  ctrl->hat_y = 0;

  if (ctrl->has_hat) {
    struct input_absinfo hat;
    memset(&hat, 0, sizeof(hat));
    if (ioctl(ctrl->fd, EVIOCGABS(ABS_HAT0X), &hat) == 0) {
      ctrl->hat_x = hat.value;
    }
    memset(&hat, 0, sizeof(hat));
    if (ioctl(ctrl->fd, EVIOCGABS(ABS_HAT0Y), &hat) == 0) {
      ctrl->hat_y = hat.value;
    }

    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_UP].kind ==
        MARU_WL_BUTTON_SOURCE_NONE) {
      ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_UP].kind =
          MARU_WL_BUTTON_SOURCE_HAT_Y_NEG;
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_DOWN].kind ==
        MARU_WL_BUTTON_SOURCE_NONE) {
      ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_DOWN].kind =
          MARU_WL_BUTTON_SOURCE_HAT_Y_POS;
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_LEFT].kind ==
        MARU_WL_BUTTON_SOURCE_NONE) {
      ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_LEFT].kind =
          MARU_WL_BUTTON_SOURCE_HAT_X_NEG;
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_RIGHT].kind ==
        MARU_WL_BUTTON_SOURCE_NONE) {
      ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_RIGHT].kind =
          MARU_WL_BUTTON_SOURCE_HAT_X_POS;
    }
  }

  uint32_t analog_cursor = MARU_CONTROLLER_ANALOG_STANDARD_COUNT;
  uint32_t extra_analog_index = 0;
  for (uint32_t code = 0; code <= (uint32_t)ABS_MAX; ++code) {
    if (!_wl_test_bit(abs_bits, code)) continue;
    if (_wl_code_in_list_u16(standard_axis_codes, MARU_CONTROLLER_ANALOG_STANDARD_COUNT,
                             (uint16_t)code)) {
      continue;
    }
    if (analog_cursor >= analog_count) break;
    if (!_wl_set_extra_name(ctx_base, ctrl_ctx, &ctrl->extra_analog_names, extra_analog_index, "abs", code,
                            &ctrl->analog_channels[analog_cursor].name)) {
      return false;
    }
    MARU_WL_AnalogSource *src = &ctrl->analog_sources[analog_cursor];
    src->code = (uint16_t)code;
    src->available = true;
    src->is_trigger = (code == ABS_Z || code == ABS_RZ || code == ABS_BRAKE || code == ABS_GAS);
    struct input_absinfo abs_info;
    memset(&abs_info, 0, sizeof(abs_info));
    if (ioctl(ctrl->fd, EVIOCGABS((unsigned int)code), &abs_info) == 0) {
      src->min = abs_info.minimum;
      src->max = abs_info.maximum;
      ctrl->analog_states[analog_cursor].value = _wl_normalize_axis(src, abs_info.value);
    } else {
      src->available = false;
      src->min = 0;
      src->max = 0;
    }
    analog_cursor++;
    extra_analog_index++;
  }

  uint32_t button_cursor = MARU_CONTROLLER_BUTTON_STANDARD_COUNT;
  uint32_t extra_button_index = 0;
  for (uint32_t code = BTN_MISC; code <= (uint32_t)KEY_MAX; ++code) {
    if (!_wl_test_bit(key_bits, code)) continue;
    if (_wl_code_in_list_u16(standard_button_codes, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
                             (uint16_t)code)) {
      continue;
    }
    if (button_cursor >= button_count) break;
    if (!_wl_set_extra_name(ctx_base, ctrl_ctx, &ctrl->extra_button_names, extra_button_index, "btn", code,
                            &ctrl->button_channels[button_cursor].name)) {
      return false;
    }
    ctrl->button_sources[button_cursor].kind = MARU_WL_BUTTON_SOURCE_KEY;
    ctrl->button_sources[button_cursor].code = (uint16_t)code;
    ctrl->button_states[button_cursor] = MARU_BUTTON_STATE_RELEASED;
    button_cursor++;
    extra_button_index++;
  }

  unsigned long key_state[_wl_bit_word((size_t)KEY_MAX + 1u) + 1u];
  memset(key_state, 0, sizeof(key_state));
  if (ioctl(ctrl->fd, EVIOCGKEY(sizeof(key_state)), key_state) == 0) {
    for (uint32_t i = 0; i < button_count; ++i) {
      if (ctrl->button_sources[i].kind == MARU_WL_BUTTON_SOURCE_KEY &&
          ctrl->button_sources[i].code != 0u &&
          _wl_test_bit(key_state, ctrl->button_sources[i].code)) {
        ctrl->button_states[i] = MARU_BUTTON_STATE_PRESSED;
      }
    }
  }

  if (ctrl->has_hat) {
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_UP].kind ==
        MARU_WL_BUTTON_SOURCE_HAT_Y_NEG) {
      ctrl->button_states[MARU_CONTROLLER_BUTTON_DPAD_UP] =
          (MARU_ButtonState8)((ctrl->hat_y < 0) ? MARU_BUTTON_STATE_PRESSED
                                                : MARU_BUTTON_STATE_RELEASED);
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_DOWN].kind ==
        MARU_WL_BUTTON_SOURCE_HAT_Y_POS) {
      ctrl->button_states[MARU_CONTROLLER_BUTTON_DPAD_DOWN] =
          (MARU_ButtonState8)((ctrl->hat_y > 0) ? MARU_BUTTON_STATE_PRESSED
                                                : MARU_BUTTON_STATE_RELEASED);
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_LEFT].kind ==
        MARU_WL_BUTTON_SOURCE_HAT_X_NEG) {
      ctrl->button_states[MARU_CONTROLLER_BUTTON_DPAD_LEFT] =
          (MARU_ButtonState8)((ctrl->hat_x < 0) ? MARU_BUTTON_STATE_PRESSED
                                                : MARU_BUTTON_STATE_RELEASED);
    }
    if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_RIGHT].kind ==
        MARU_WL_BUTTON_SOURCE_HAT_X_POS) {
        ctrl->button_states[MARU_CONTROLLER_BUTTON_DPAD_RIGHT] =
          (MARU_ButtonState8)((ctrl->hat_x > 0) ? MARU_BUTTON_STATE_PRESSED
                                                : MARU_BUTTON_STATE_RELEASED);
    }
  }

  ctrl->pub.analog_channels = ctrl->analog_channels;
  ctrl->pub.analogs = ctrl->analog_states;
  ctrl->pub.analog_count = analog_count;
  ctrl->pub.button_channels = ctrl->button_channels;
  ctrl->pub.buttons = ctrl->button_states;
  ctrl->pub.button_count = button_count;
  return true;
}

static MARU_Controller_Linux *_wl_create_controller(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, int fd,
                                                 const char *path) {
  return NULL;
  
  MARU_Controller_Linux *ctrl =
      (MARU_Controller_Linux *)maru_context_alloc(ctx_base, sizeof(*ctrl));
  if (!ctrl) {
    return NULL;
  }
  memset(ctrl, 0, sizeof(*ctrl));
  ctrl->ctrl_ctx = ctrl_ctx;
  ctrl->fd = fd;
  ctrl->connected = true;
  ctrl->seen_in_scan = true;
  ctrl->rumble_effect_id = -1;

  const size_t path_len = strlen(path) + 1u;
  ctrl->path = (char *)maru_context_alloc(ctx_base, path_len);
  if (!ctrl->path) {
    _wl_free_controller(ctx_base, ctrl_ctx, ctrl);
    return NULL;
  }
  memcpy(ctrl->path, path, path_len);

  char name_buf[256];
  memset(name_buf, 0, sizeof(name_buf));
  if (ioctl(fd, EVIOCGNAME(sizeof(name_buf)), name_buf) < 0 || name_buf[0] == '\0') {
    (void)strncpy(name_buf, "Unknown Controller", sizeof(name_buf) - 1u);
  }

  const size_t name_len = strlen(name_buf) + 1u;
  ctrl->name_storage = (char *)maru_context_alloc(ctx_base, name_len);
  if (!ctrl->name_storage) {
    _wl_free_controller(ctx_base, ctrl_ctx, ctrl);
    return NULL;
  }
  memcpy(ctrl->name_storage, name_buf, name_len);

  struct input_id ids;
  memset(&ids, 0, sizeof(ids));
  (void)ioctl(fd, EVIOCGID, &ids);

  ctrl->info.name = ctrl->name_storage;
  ctrl->info.vendor_id = ids.vendor;
  ctrl->info.product_id = ids.product;
  ctrl->info.version = ids.version;
  memset(ctrl->info.guid, 0, sizeof(ctrl->info.guid));
  ctrl->info.guid[0] = (uint8_t)(ids.bustype & 0xFFu);
  ctrl->info.guid[1] = (uint8_t)((ids.bustype >> 8) & 0xFFu);
  ctrl->info.guid[2] = (uint8_t)(ids.vendor & 0xFFu);
  ctrl->info.guid[3] = (uint8_t)((ids.vendor >> 8) & 0xFFu);
  ctrl->info.guid[4] = (uint8_t)(ids.product & 0xFFu);
  ctrl->info.guid[5] = (uint8_t)((ids.product >> 8) & 0xFFu);
  ctrl->info.guid[6] = (uint8_t)(ids.version & 0xFFu);
  ctrl->info.guid[7] = (uint8_t)((ids.version >> 8) & 0xFFu);

  if (!_wl_init_controller_sources(ctx_base, ctrl_ctx, ctrl)) {
    _wl_free_controller(ctx_base, ctrl_ctx, ctrl);
    return NULL;
  }

  bool standardized = true;
  for (uint32_t i = 0; i < 4u; ++i) {
    if (!ctrl->analog_sources[i].available) {
      standardized = false;
      break;
    }
  }
  for (uint32_t i = 0; i < 11u; ++i) {
    if (ctrl->button_sources[i].kind == MARU_WL_BUTTON_SOURCE_NONE) {
      standardized = false;
      break;
    }
  }
  ctrl->info.is_standardized = standardized;

  ctrl->pub.userdata = NULL;
  ctrl->pub.context = (MARU_Context *)ctx_base;
  ctrl->pub.flags = 0;
  ctrl->pub.metrics = &ctrl->metrics;

  ctrl->pub.haptic_channels = NULL;
  ctrl->pub.haptic_count = 0;
  if (_wl_probe_rumble_capable(fd)) {
    ctrl->haptic_channels[MARU_CONTROLLER_HAPTIC_LOW_FREQ].name = "Low Frequency";
    ctrl->haptic_channels[MARU_CONTROLLER_HAPTIC_HIGH_FREQ].name = "High Frequency";
    ctrl->pub.haptic_channels = ctrl->haptic_channels;
    ctrl->pub.haptic_count = 2u;
  }

  atomic_init(&ctrl->ref_count, 0u);
  ctrl->next = ctrl_ctx->list_head;
  ctrl_ctx->list_head = ctrl;
  ctrl_ctx->connected_count++;
  _wl_dispatch_controller_connection(ctx_base, ctrl_ctx, ctrl, true);
  return ctrl;
}

static void _wl_mark_controller_disconnected(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
                                             MARU_Controller_Linux *ctrl) {
  if (!ctrl->connected) return;
  ctrl->connected = false;
  ctrl->pub.flags |= MARU_CONTROLLER_STATE_LOST;
  if (ctrl->fd >= 0) {
    if (ctrl->rumble_effect_id >= 0) {
      (void)ioctl(ctrl->fd, EVIOCRMFF, ctrl->rumble_effect_id);
      ctrl->rumble_effect_id = -1;
    }
    close(ctrl->fd);
    ctrl->fd = -1;
  }
  if (ctrl_ctx->connected_count > 0u) {
    ctrl_ctx->connected_count--;
  }
  _wl_dispatch_controller_connection(ctx_base, ctrl_ctx, ctrl, false);
}

static void _wl_try_remove_unretained_disconnected(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  MARU_Controller_Linux *prev = NULL;
  MARU_Controller_Linux *it = ctrl_ctx->list_head;
  while (it) {
    MARU_Controller_Linux *next = it->next;
    const uint32_t refs = atomic_load_explicit(&it->ref_count, memory_order_acquire);
    if (!it->connected && refs == 0u) {
      if (prev) {
        prev->next = next;
      } else {
        ctrl_ctx->list_head = next;
      }
      _wl_free_controller(ctx_base, ctrl_ctx, it);
    } else {
      prev = it;
    }
    it = next;
  }
}

static void _wl_set_button_state(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, MARU_Controller_Linux *ctrl,
                                 uint32_t button_id, MARU_ButtonState new_state) {
  if (button_id >= ctrl->pub.button_count) return;
  if (ctrl->button_states[button_id] == (MARU_ButtonState8)new_state) return;
  ctrl->button_states[button_id] = (MARU_ButtonState8)new_state;
  _wl_dispatch_controller_button(ctx_base, ctrl_ctx, ctrl, button_id, new_state);
}

static void _wl_update_hat_buttons(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, MARU_Controller_Linux *ctrl) {
  if (!ctrl->has_hat) return;
  if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_UP].kind ==
      MARU_WL_BUTTON_SOURCE_HAT_Y_NEG) {
    _wl_set_button_state(ctx_base, ctrl_ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_UP,
                         (ctrl->hat_y < 0) ? MARU_BUTTON_STATE_PRESSED
                                           : MARU_BUTTON_STATE_RELEASED);
  }
  if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_DOWN].kind ==
      MARU_WL_BUTTON_SOURCE_HAT_Y_POS) {
    _wl_set_button_state(ctx_base, ctrl_ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_DOWN,
                         (ctrl->hat_y > 0) ? MARU_BUTTON_STATE_PRESSED
                                           : MARU_BUTTON_STATE_RELEASED);
  }
  if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_LEFT].kind ==
      MARU_WL_BUTTON_SOURCE_HAT_X_NEG) {
    _wl_set_button_state(ctx_base, ctrl_ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_LEFT,
                         (ctrl->hat_x < 0) ? MARU_BUTTON_STATE_PRESSED
                                           : MARU_BUTTON_STATE_RELEASED);
  }
  if (ctrl->button_sources[MARU_CONTROLLER_BUTTON_DPAD_RIGHT].kind ==
      MARU_WL_BUTTON_SOURCE_HAT_X_POS) {
    _wl_set_button_state(ctx_base, ctrl_ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_RIGHT,
                         (ctrl->hat_x > 0) ? MARU_BUTTON_STATE_PRESSED
                                           : MARU_BUTTON_STATE_RELEASED);
  }
}

static void _wl_poll_controller_events(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, MARU_Controller_Linux *ctrl) {
  if (!ctrl->connected || ctrl->fd < 0) return;

  struct input_event ev;
  for (;;) {
    const ssize_t n = read(ctrl->fd, &ev, sizeof(ev));
    if (n == (ssize_t)sizeof(ev)) {
      if (ev.type == EV_KEY) {
        for (uint32_t i = 0; i < ctrl->pub.button_count; ++i) {
          if (ctrl->button_sources[i].kind == MARU_WL_BUTTON_SOURCE_KEY &&
              ctrl->button_sources[i].code == ev.code) {
            _wl_set_button_state(ctx_base, ctrl_ctx, ctrl, i,
                                 (ev.value != 0) ? MARU_BUTTON_STATE_PRESSED
                                                 : MARU_BUTTON_STATE_RELEASED);
          }
        }
      } else if (ev.type == EV_ABS) {
        for (uint32_t i = 0; i < ctrl->pub.analog_count; ++i) {
          if (ctrl->analog_sources[i].available &&
              ctrl->analog_sources[i].code == ev.code) {
            ctrl->analog_states[i].value =
                _wl_normalize_axis(&ctrl->analog_sources[i], ev.value);
          }
        }
        if (ctrl->has_hat) {
          if (ev.code == ABS_HAT0X) {
            ctrl->hat_x = ev.value;
            _wl_update_hat_buttons(ctx_base, ctrl_ctx, ctrl);
          } else if (ev.code == ABS_HAT0Y) {
            ctrl->hat_y = ev.value;
            _wl_update_hat_buttons(ctx_base, ctrl_ctx, ctrl);
          }
        }
      }
      continue;
    }

    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }
    _wl_mark_controller_disconnected(ctx_base, ctrl_ctx, ctrl);
    return;
  }
}

static bool _wl_is_retryable_open_errno(int err) {
  return err == ENOENT || err == ENODEV || err == EBUSY || err == EAGAIN;
}

static int _wl_try_open_device_fd(const char *path) {
  int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  }
  return fd;
}

static bool _wl_is_evdev_path(const char *path) {
  if (!path) {
    return false;
  }
  return strncmp(path, "/dev/input/event", 16) == 0;
}

static bool _wl_pending_add_queue_contains(
    const MARU_ControllerContext_Linux *ctrl_ctx, const char *path) {
  for (uint32_t i = 0; i < ctrl_ctx->pending_add_count; ++i) {
    const uint32_t idx =
        (ctrl_ctx->pending_add_head + i) % MARU_CONTROLLER_PENDING_ADD_CAPACITY;
    if (strncmp(ctrl_ctx->pending_add_paths[idx], path,
                MARU_CONTROLLER_PENDING_PATH_MAX) == 0) {
      return true;
    }
  }
  return false;
}

static void _wl_enqueue_controller_add_path(MARU_ControllerContext_Linux *ctrl_ctx,
                                            const char *path) {
  if (!path || !_wl_is_evdev_path(path)) {
    return;
  }
  if (_wl_pending_add_queue_contains(ctrl_ctx, path)) {
    return;
  }

  const uint32_t tail =
      (ctrl_ctx->pending_add_head + ctrl_ctx->pending_add_count) %
      MARU_CONTROLLER_PENDING_ADD_CAPACITY;
  const int nw = snprintf(ctrl_ctx->pending_add_paths[tail],
                          MARU_CONTROLLER_PENDING_PATH_MAX, "%s", path);
  if (nw <= 0 || (size_t)nw >= MARU_CONTROLLER_PENDING_PATH_MAX) {
    return;
  }

  if (ctrl_ctx->pending_add_count < MARU_CONTROLLER_PENDING_ADD_CAPACITY) {
    ctrl_ctx->pending_add_count++;
    return;
  }

  // Queue full: drop oldest and keep latest path so progress continues.
  ctrl_ctx->pending_add_head =
      (ctrl_ctx->pending_add_head + 1u) % MARU_CONTROLLER_PENDING_ADD_CAPACITY;
}

static bool _wl_dequeue_controller_add_path(MARU_ControllerContext_Linux *ctrl_ctx,
                                            char *out_path,
                                            size_t out_path_size) {
  if (ctrl_ctx->pending_add_count == 0u || !out_path || out_path_size == 0u) {
    return false;
  }

  const uint32_t idx = ctrl_ctx->pending_add_head;
  const int nw = snprintf(out_path, out_path_size, "%s",
                          ctrl_ctx->pending_add_paths[idx]);
  ctrl_ctx->pending_add_head =
      (ctrl_ctx->pending_add_head + 1u) % MARU_CONTROLLER_PENDING_ADD_CAPACITY;
  ctrl_ctx->pending_add_count--;
  return nw > 0 && (size_t)nw < out_path_size;
}

static void _wl_try_add_controller_path(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, const char *path) {

  if (!path || _wl_find_controller_by_path(ctx_base, ctrl_ctx, path)) {
    return;
  }
  const int fd = _wl_try_open_device_fd(path);

  if (fd < 0) {
    if (_wl_is_retryable_open_errno(errno)) {
      _wl_enqueue_controller_add_path(ctrl_ctx, path);
    }
    return;
  }
  if (!_wl_device_is_controller(fd)) {
    close(fd);
    return;
  }
  if (!_wl_create_controller(ctx_base, ctrl_ctx, fd, path)) {
    close(fd);
    return;
  }
}

static void _wl_process_one_pending_add(MARU_Context_Base *ctx_base,
                                        MARU_ControllerContext_Linux *ctrl_ctx) {
  char path[MARU_CONTROLLER_PENDING_PATH_MAX];
  if (!_wl_dequeue_controller_add_path(ctrl_ctx, path, sizeof(path))) {
    return;
  }
  _wl_try_add_controller_path(ctx_base, ctrl_ctx, path);
}

static void _wl_remove_controller_path(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, const char *path) {
  MARU_Controller_Linux *ctrl = _wl_find_controller_by_path(ctx_base, ctrl_ctx, path);
  if (!ctrl) return;
  _wl_mark_controller_disconnected(ctx_base, ctrl_ctx, ctrl);
  _wl_try_remove_unretained_disconnected(ctx_base, ctrl_ctx);
}

static void _wl_scan_controllers_startup(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  DIR *dir = opendir("/dev/input");
  if (!dir) return;
  for (;;) {
    struct dirent *entry = readdir(dir);
    if (!entry) break;
    if (strncmp(entry->d_name, "event", 5) != 0) continue;
    char path[512];
    const int nw = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
    if (nw <= 0 || (size_t)nw >= sizeof(path)) continue;
    _wl_try_add_controller_path(ctx_base, ctrl_ctx, path);
  }
  closedir(dir);
}

static void _wl_reset_udev_state(MARU_ControllerContext_Linux *ctrl_ctx) {
  if (ctrl_ctx->udev_monitor && ctrl_ctx->udev_lib.udev_monitor_unref) {
    (void)ctrl_ctx->udev_lib.udev_monitor_unref(ctrl_ctx->udev_monitor);
  }
  ctrl_ctx->udev_monitor = NULL;
  if (ctrl_ctx->udev && ctrl_ctx->udev_lib.udev_unref) {
    (void)ctrl_ctx->udev_lib.udev_unref(ctrl_ctx->udev);
  }
  ctrl_ctx->udev = NULL;
  ctrl_ctx->udev_fd = -1;
  ctrl_ctx->use_udev = false;
}

static void _wl_unload_udev_lib(MARU_ControllerContext_Linux *ctrl_ctx) {
  _wl_reset_udev_state(ctrl_ctx);
  if (ctrl_ctx->udev_lib.base.available) {
    maru_linux_udev_unload(&ctrl_ctx->udev_lib);
  }
}

static bool _wl_init_udev_hotplug(MARU_Context_Base *ctx_base,
                                  MARU_ControllerContext_Linux *ctrl_ctx) {
  if (ctrl_ctx->use_udev && ctrl_ctx->udev_monitor && ctrl_ctx->udev_fd >= 0) {
    return true;
  }

  if (!ctrl_ctx->udev_lib.base.available) {
    if (!maru_linux_udev_load(ctx_base, &ctrl_ctx->udev_lib)) {
      return false;
    }
  }

  ctrl_ctx->udev = ctrl_ctx->udev_lib.udev_new();
  if (!ctrl_ctx->udev) {
    _wl_unload_udev_lib(ctrl_ctx);
    return false;
  }

  ctrl_ctx->udev_monitor =
      ctrl_ctx->udev_lib.udev_monitor_new_from_netlink(ctrl_ctx->udev, "udev");
  if (!ctrl_ctx->udev_monitor) {
    _wl_unload_udev_lib(ctrl_ctx);
    return false;
  }

  if (ctrl_ctx->udev_lib.udev_monitor_filter_add_match_subsystem_devtype(
          ctrl_ctx->udev_monitor, "input", NULL) < 0 ||
      ctrl_ctx->udev_lib.udev_monitor_enable_receiving(ctrl_ctx->udev_monitor) < 0) {
    _wl_unload_udev_lib(ctrl_ctx);
    return false;
  }

  ctrl_ctx->udev_fd = ctrl_ctx->udev_lib.udev_monitor_get_fd(ctrl_ctx->udev_monitor);
  if (ctrl_ctx->udev_fd < 0) {
    _wl_unload_udev_lib(ctrl_ctx);
    return false;
  }

  ctrl_ctx->use_udev = true;
  return true;
}

static void _wl_drain_udev_hotplug_events(MARU_Context_Base *ctx_base,
                                          MARU_ControllerContext_Linux *ctrl_ctx) {
  if (!ctrl_ctx->use_udev || !ctrl_ctx->udev_monitor) {
    return;
  }

  for (;;) {
    errno = 0;
    struct udev_device *device =
        ctrl_ctx->udev_lib.udev_monitor_receive_device(ctrl_ctx->udev_monitor);
    if (!device) {
      break;
    }

    const char *devnode = ctrl_ctx->udev_lib.udev_device_get_devnode(device);
    const char *action = ctrl_ctx->udev_lib.udev_device_get_action(device);
    if (_wl_is_evdev_path(devnode)) {
      if (action && strcmp(action, "remove") == 0) {
        _wl_remove_controller_path(ctx_base, ctrl_ctx, devnode);
      } else {
        _wl_enqueue_controller_add_path(ctrl_ctx, devnode);
      }
    }
    (void)ctrl_ctx->udev_lib.udev_device_unref(device);
  }
}

static void _wl_init_hotplug_watch(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  if (_wl_init_udev_hotplug(ctx_base, ctrl_ctx)) {
    if (ctrl_ctx->inotify_fd >= 0) {
      if (ctrl_ctx->inotify_wd >= 0) {
        (void)inotify_rm_watch(ctrl_ctx->inotify_fd, ctrl_ctx->inotify_wd);
      }
      close(ctrl_ctx->inotify_fd);
      ctrl_ctx->inotify_fd = -1;
      ctrl_ctx->inotify_wd = -1;
    }
    return;
  }

  if (ctrl_ctx->inotify_fd == -2) {
    return;
  }

  int fd = ctrl_ctx->inotify_fd;
  if (fd < 0) {
    fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
      ctrl_ctx->inotify_fd = -2;
      MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                             "Failed to initialize inotify for controller hotplug");
      return;
    }
  }
  if (ctrl_ctx->inotify_wd >= 0) {
    ctrl_ctx->inotify_fd = fd;
    return;
  }

  const uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM |
                        IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF;
  const int wd = inotify_add_watch(fd, "/dev/input", mask);
  if (wd < 0) {
    if (ctrl_ctx->inotify_fd < 0) {
      close(fd);
    }
    ctrl_ctx->inotify_fd = -1;
    ctrl_ctx->inotify_wd = -1;
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
                           "Failed to watch /dev/input for controller hotplug");
    return;
  }

  ctrl_ctx->inotify_fd = fd;
  ctrl_ctx->inotify_wd = wd;
}

static bool _wl_event_targets_evdev(const struct inotify_event *ev) {
  if (!ev) return false;
  if ((ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0u) return true;
  if (ev->len == 0u) return false;
  return strncmp(ev->name, "event", 5) == 0;
}

static void _wl_drain_hotplug_events(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  if (ctrl_ctx->inotify_fd < 0) return;

  char buffer[4096];
  for (;;) {
    const ssize_t n = read(ctrl_ctx->inotify_fd, buffer, sizeof(buffer));
    if (n <= 0) {
      break;
    }

    ssize_t offset = 0;
    while (offset + (ssize_t)sizeof(struct inotify_event) <= n) {
      const struct inotify_event *ev =
          (const struct inotify_event *)(buffer + offset);
      if ((ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) != 0u) {
        if ((ev->mask & IN_IGNORED) != 0u) {
          ctrl_ctx->inotify_wd = -1;
        }
      } else if (_wl_event_targets_evdev(ev)) {
        char path[512];
        const int nw = snprintf(path, sizeof(path), "/dev/input/%s", ev->name);
        if (nw > 0 && (size_t)nw < sizeof(path)) {
          if ((ev->mask & (IN_CREATE | IN_MOVED_TO | IN_ATTRIB)) != 0u) {
            _wl_enqueue_controller_add_path(ctrl_ctx, path);
          }
          if ((ev->mask & (IN_DELETE | IN_MOVED_FROM)) != 0u) {
            _wl_remove_controller_path(ctx_base, ctrl_ctx, path);
          }
        }
      }
      offset += (ssize_t)sizeof(struct inotify_event) + (ssize_t)ev->len;
    }
  }
}

static void _wl_rebuild_snapshot(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  uint32_t count = 0;
  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    if (it->connected) {
      count++;
    }
  }

  if (count > ctrl_ctx->snapshot_capacity) {
    const size_t old_size =
        (size_t)ctrl_ctx->snapshot_capacity * sizeof(MARU_Controller *);
    const size_t new_size = (size_t)count * sizeof(MARU_Controller *);
    MARU_Controller **new_snapshot = (MARU_Controller **)maru_context_realloc(
        ctx_base, ctrl_ctx->snapshot, old_size, new_size);
    if (!new_snapshot) {
      ctrl_ctx->snapshot_count = 0;
      return;
    }
    ctrl_ctx->snapshot = new_snapshot;
    ctrl_ctx->snapshot_capacity = count;
  }

  uint32_t index = 0;
  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    if (it->connected && index < ctrl_ctx->snapshot_capacity) {
      ctrl_ctx->snapshot[index++] = (MARU_Controller *)it;
    }
  }
  ctrl_ctx->snapshot_count = index;
}

void _maru_linux_controllers_poll(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx, bool allow_scan) {
  if (allow_scan) {
    _wl_init_hotplug_watch(ctx_base, ctrl_ctx);
    if (ctrl_ctx->use_udev) {
      _wl_drain_udev_hotplug_events(ctx_base, ctrl_ctx);
    } else {
      _wl_drain_hotplug_events(ctx_base, ctrl_ctx);
    }
    _wl_process_one_pending_add(ctx_base, ctrl_ctx);
  }

  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    _wl_poll_controller_events(ctx_base, ctrl_ctx, it);
  }
  _wl_try_remove_unretained_disconnected(ctx_base, ctrl_ctx);
  _wl_rebuild_snapshot(ctx_base, ctrl_ctx);
}

void _maru_linux_controllers_get_pollfds(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx,
                                          const struct pollfd **out_fds,
                                          uint32_t *out_count) {
  uint32_t count = (ctrl_ctx->inotify_fd >= 0) ? 1u : 0u;
  if (ctrl_ctx->use_udev && ctrl_ctx->udev_fd >= 0) {
    count++;
  }
  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    if (it->connected && it->fd >= 0) {
      count++;
    }
  }

  if (count > ctrl_ctx->pollfds_capacity) {
    const size_t old_size =
        (size_t)ctrl_ctx->pollfds_capacity * sizeof(struct pollfd);
    const size_t new_size = (size_t)count * sizeof(struct pollfd);
    struct pollfd *new_fds = (struct pollfd *)maru_context_realloc(
        ctx_base, ctrl_ctx->pollfds, old_size, new_size);
    if (!new_fds) {
      *out_fds = NULL;
      *out_count = 0;
      return;
    }
    ctrl_ctx->pollfds = new_fds;
    ctrl_ctx->pollfds_capacity = count;
  }

  uint32_t index = 0;
  if (ctrl_ctx->use_udev && ctrl_ctx->udev_fd >= 0 &&
      index < ctrl_ctx->pollfds_capacity) {
    ctrl_ctx->pollfds[index].fd = ctrl_ctx->udev_fd;
    ctrl_ctx->pollfds[index].events = POLLIN;
    ctrl_ctx->pollfds[index].revents = 0;
    index++;
  }
  if (ctrl_ctx->inotify_fd >= 0 && index < ctrl_ctx->pollfds_capacity) {
    ctrl_ctx->pollfds[index].fd = ctrl_ctx->inotify_fd;
    ctrl_ctx->pollfds[index].events = POLLIN;
    ctrl_ctx->pollfds[index].revents = 0;
    index++;
  }
  for (MARU_Controller_Linux *it = ctrl_ctx->list_head; it; it = it->next) {
    if (it->connected && it->fd >= 0 && index < ctrl_ctx->pollfds_capacity) {
      ctrl_ctx->pollfds[index].fd = it->fd;
      ctrl_ctx->pollfds[index].events = POLLIN;
      ctrl_ctx->pollfds[index].revents = 0;
      index++;
    }
  }

  *out_fds = ctrl_ctx->pollfds;
  *out_count = index;
}

void _maru_linux_controllers_cleanup(MARU_Context_Base *ctx_base, MARU_ControllerContext_Linux *ctrl_ctx) {
  MARU_Controller_Linux *it = ctrl_ctx->list_head;
  while (it) {
    MARU_Controller_Linux *next = it->next;
    _wl_free_controller(ctx_base, ctrl_ctx, it);
    it = next;
  }
  ctrl_ctx->list_head = NULL;
  ctrl_ctx->connected_count = 0;

  if (ctrl_ctx->snapshot) {
    maru_context_free(ctx_base, ctrl_ctx->snapshot);
    ctrl_ctx->snapshot = NULL;
  }
  ctrl_ctx->snapshot_count = 0;
  ctrl_ctx->snapshot_capacity = 0;

  if (ctrl_ctx->pollfds) {
    maru_context_free(ctx_base, ctrl_ctx->pollfds);
    ctrl_ctx->pollfds = NULL;
  }
  ctrl_ctx->pollfds_capacity = 0;

  if (ctrl_ctx->inotify_fd >= 0) {
    if (ctrl_ctx->inotify_wd >= 0) {
      (void)inotify_rm_watch(ctrl_ctx->inotify_fd, ctrl_ctx->inotify_wd);
    }
    close(ctrl_ctx->inotify_fd);
  }
  ctrl_ctx->inotify_fd = -1;
  ctrl_ctx->inotify_wd = -1;
  _wl_unload_udev_lib(ctrl_ctx);
  ctrl_ctx->pending_add_head = 0;
  ctrl_ctx->pending_add_count = 0;
}

void _maru_linux_controllers_init(MARU_Context_Base *ctx_base,
                                  MARU_ControllerContext_Linux *ctrl_ctx) {
  (void)ctx_base;
  if (!ctrl_ctx) {
    return;
  }
  ctrl_ctx->list_head = NULL;
  ctrl_ctx->snapshot = NULL;
  ctrl_ctx->pollfds = NULL;
  ctrl_ctx->snapshot_count = 0;
  ctrl_ctx->snapshot_capacity = 0;
  ctrl_ctx->pollfds_capacity = 0;
  ctrl_ctx->connected_count = 0;
  ctrl_ctx->inotify_fd = -1;
  ctrl_ctx->inotify_wd = -1;
  ctrl_ctx->pending_add_head = 0;
  ctrl_ctx->pending_add_count = 0;
  memset(&ctrl_ctx->udev_lib, 0, sizeof(ctrl_ctx->udev_lib));
  ctrl_ctx->udev = NULL;
  ctrl_ctx->udev_monitor = NULL;
  ctrl_ctx->udev_fd = -1;
  ctrl_ctx->use_udev = false;

  // Full /dev/input scan is startup-only. Runtime discovery is hotplug-driven.
  _wl_scan_controllers_startup(ctx_base, ctrl_ctx);
}

MARU_Status _maru_linux_getControllers(MARU_Context_Base *ctx_base,
                                       MARU_ControllerContext_Linux *ctrl_ctx,
                                       MARU_ControllerList *out_list) {
  _wl_rebuild_snapshot(ctx_base, ctrl_ctx);
  out_list->controllers = (MARU_Controller *const *)ctrl_ctx->snapshot;
  out_list->count = ctrl_ctx->snapshot_count;
  return MARU_SUCCESS;
}

MARU_Status _maru_linux_retainController(MARU_Controller *controller) {
  MARU_Controller_Linux *ctrl = (MARU_Controller_Linux *)controller;
  atomic_fetch_add_explicit(&ctrl->ref_count, 1u, memory_order_relaxed);
  return MARU_SUCCESS;
}

MARU_Status _maru_linux_releaseController(MARU_Controller *controller) {
  MARU_Controller_Linux *ctrl = (MARU_Controller_Linux *)controller;
  uint32_t current = atomic_load_explicit(&ctrl->ref_count, memory_order_acquire);
  while (current > 0u) {
    if (atomic_compare_exchange_weak_explicit(&ctrl->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      break;
    }
  }

  MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctrl->pub.context;
  MARU_ControllerContext_Linux *ctrl_ctx = ctrl->ctrl_ctx;
  if (ctx_base && ctrl_ctx) {
    _wl_try_remove_unretained_disconnected(ctx_base, ctrl_ctx);
  }
  return MARU_SUCCESS;
}

MARU_Status _maru_linux_resetControllerMetrics(MARU_Controller *controller) {
  MARU_Controller_Linux *ctrl = (MARU_Controller_Linux *)controller;
  memset(&ctrl->metrics, 0, sizeof(ctrl->metrics));
  return MARU_SUCCESS;
}

MARU_Status _maru_linux_getControllerInfo(MARU_Controller *controller,
                                          MARU_ControllerInfo *out_info) {
  MARU_Controller_Linux *ctrl = (MARU_Controller_Linux *)controller;
  *out_info = ctrl->info;
  return MARU_SUCCESS;
}

MARU_Status _maru_linux_setControllerHapticLevels(MARU_Controller *controller,
                                                  uint32_t first_haptic,
                                                  uint32_t count,
                                                  const MARU_Scalar *intensities) {
  MARU_Controller_Linux *ctrl = (MARU_Controller_Linux *)controller;
  if (count == 0u) {
    return MARU_SUCCESS;
  }
  if (!ctrl->connected || ctrl->fd < 0 || ctrl->pub.haptic_count < 2u) {
    return MARU_FAILURE;
  }

  MARU_Scalar low = 0;
  MARU_Scalar high = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t channel = first_haptic + i;
    if (channel == MARU_CONTROLLER_HAPTIC_LOW_FREQ) {
      low = intensities[i];
    } else if (channel == MARU_CONTROLLER_HAPTIC_HIGH_FREQ) {
      high = intensities[i];
    }
  }

  if (low < 0) low = 0;
  if (low > 1) low = 1;
  if (high < 0) high = 0;
  if (high > 1) high = 1;

  struct ff_effect effect;
  memset(&effect, 0, sizeof(effect));
  effect.type = FF_RUMBLE;
  effect.id = (short)ctrl->rumble_effect_id;
  effect.u.rumble.strong_magnitude = (uint16_t)(low * (MARU_Scalar)65535);
  effect.u.rumble.weak_magnitude = (uint16_t)(high * (MARU_Scalar)65535);
  effect.replay.length = 0u;
  effect.replay.delay = 0u;

  if (ioctl(ctrl->fd, EVIOCSFF, &effect) < 0) {
    return MARU_FAILURE;
  }
  ctrl->rumble_effect_id = effect.id;

  struct input_event play;
  memset(&play, 0, sizeof(play));
  play.type = EV_FF;
  play.code = (uint16_t)effect.id;
  play.value = 1;
  if (write(ctrl->fd, &play, sizeof(play)) != (ssize_t)sizeof(play)) {
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}
