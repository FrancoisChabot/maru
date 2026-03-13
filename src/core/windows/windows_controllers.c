// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 Francois Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"

#include <hidsdi.h>
#include <hidusage.h>
#include <roapi.h>
#include <stdio.h>
#include <string.h>
#include <windows.gaming.input.h>
#include <xinput.h>

typedef __x_ABI_CWindows_CGaming_CInput_CIGamepadStatics IGamepadStatics;
typedef __x_ABI_CWindows_CGaming_CInput_CIGamepad IGamepad;
typedef __x_ABI_CWindows_CGaming_CInput_CGamepadReading GamepadReading;
typedef __x_ABI_CWindows_CGaming_CInput_CGamepadVibration GamepadVibration;
typedef __FIVectorView_1_Windows__CGaming__CInput__CGamepad IGamepadVectorView;
typedef __x_ABI_CWindows_CGaming_CInput_CGamepadButtons GamepadButtons;

typedef struct MARU_WindowsHidButtonMapping {
  USAGE usage_page;
  USAGE usage;
} MARU_WindowsHidButtonMapping;

typedef struct MARU_WindowsHidValueMapping {
  USAGE usage_page;
  USAGE usage;
  LONG logical_min;
  LONG logical_max;
  bool is_centered;
} MARU_WindowsHidValueMapping;

static const uint64_t MARU_CONTROLLER_FLAG_PENDING_REMOVAL = 1ULL << 63;
static const uint16_t MARU_SONY_VENDOR_ID = 0x054Cu;
static const char *MARU_STANDARD_BUTTON_NAMES[] = {
    "South", "East",      "West",  "North", "LB",
    "RB",    "Back",      "Start", "Guide", "LThumb",
    "RThumb","DpadUp",    "DpadRight", "DpadDown", "DpadLeft"};
static const char *MARU_STANDARD_ANALOG_NAMES[] = {
    "LeftX", "LeftY", "RightX", "RightY", "LTrigger", "RTrigger"};
static const char *MARU_STANDARD_HAPTIC_NAMES[] = {"LowFreq", "HighFreq"};

static void _maru_windows_emit_controller_button_event(
    MARU_Context_Windows *ctx, MARU_Controller_Windows *ctrl,
    uint32_t button_id, MARU_ButtonState8 new_state);

static bool _maru_windows_is_game_controller_usage(USHORT usage_page,
                                                   USHORT usage) {
  if (usage_page != HID_USAGE_PAGE_GENERIC) {
    return false;
  }
  return usage == HID_USAGE_GENERIC_JOYSTICK ||
         usage == HID_USAGE_GENERIC_GAMEPAD ||
         usage == HID_USAGE_GENERIC_MULTI_AXIS_CONTROLLER;
}

static bool _maru_windows_is_xinput_hid_path(const wchar_t *path) {
  return path && wcsstr(path, L"IG_") != NULL;
}

static bool _maru_windows_is_dualshock4_product(uint16_t vendor_id,
                                                uint16_t product_id) {
  if (vendor_id != MARU_SONY_VENDOR_ID) {
    return false;
  }

  switch (product_id) {
  case 0x05C4:
  case 0x09CC:
    return true;
  default:
    return false;
  }
}

static bool _maru_windows_is_dualsense_product(uint16_t vendor_id,
                                               uint16_t product_id) {
  if (vendor_id != MARU_SONY_VENDOR_ID) {
    return false;
  }

  switch (product_id) {
  case 0x0CE6:
    return true;
  default:
    return false;
  }
}

static MARU_WindowsHidControllerKind
_maru_windows_detect_hid_controller_kind(uint16_t vendor_id,
                                         uint16_t product_id) {
  if (_maru_windows_is_dualshock4_product(vendor_id, product_id)) {
    return MARU_WINDOWS_HID_KIND_DUALSHOCK4;
  }
  if (_maru_windows_is_dualsense_product(vendor_id, product_id)) {
    return MARU_WINDOWS_HID_KIND_DUALSENSE;
  }
  return MARU_WINDOWS_HID_KIND_GENERIC;
}

static const char *_maru_windows_generic_usage_name(USAGE usage) {
  switch (usage) {
  case HID_USAGE_GENERIC_X:
    return "X";
  case HID_USAGE_GENERIC_Y:
    return "Y";
  case HID_USAGE_GENERIC_Z:
    return "Z";
  case HID_USAGE_GENERIC_RX:
    return "Rx";
  case HID_USAGE_GENERIC_RY:
    return "Ry";
  case HID_USAGE_GENERIC_RZ:
    return "Rz";
  case HID_USAGE_GENERIC_SLIDER:
    return "Slider";
  case HID_USAGE_GENERIC_DIAL:
    return "Dial";
  case HID_USAGE_GENERIC_WHEEL:
    return "Wheel";
  default:
    return NULL;
  }
}

static bool _maru_windows_hid_usage_is_centered(USAGE usage) {
  switch (usage) {
  case HID_USAGE_GENERIC_X:
  case HID_USAGE_GENERIC_Y:
  case HID_USAGE_GENERIC_Z:
  case HID_USAGE_GENERIC_RX:
  case HID_USAGE_GENERIC_RY:
  case HID_USAGE_GENERIC_RZ:
    return true;
  default:
    return false;
  }
}

static char *_maru_windows_dup_utf8(MARU_Context_Base *ctx_base,
                                    const char *src) {
  size_t len;
  char *dst;

  if (!src) {
    return NULL;
  }
  len = strlen(src);
  dst = (char *)maru_context_alloc(ctx_base, len + 1u);
  if (!dst) {
    return NULL;
  }
  memcpy(dst, src, len + 1u);
  return dst;
}

static void _maru_windows_controller_sync_pub(MARU_Controller_Windows *ctrl) {
  ctrl->base.pub.name = ctrl->name;
  ctrl->base.pub.vendor_id = ctrl->vendor_id;
  ctrl->base.pub.product_id = ctrl->product_id;
  ctrl->base.pub.version = ctrl->version;
  memcpy((void *)ctrl->base.pub.guid, ctrl->guid, 16);
  ctrl->base.pub.analog_channels = ctrl->analog_channels;
  ctrl->base.pub.analogs = ctrl->analog_states;
  ctrl->base.pub.analog_count = ctrl->analog_count;
  ctrl->base.pub.button_channels = ctrl->button_channels;
  ctrl->base.pub.buttons = ctrl->button_states;
  ctrl->base.pub.button_count = ctrl->button_count;
  ctrl->base.pub.haptic_channels = ctrl->haptic_channels;
  ctrl->base.pub.haptic_count = ctrl->haptic_count;
}

static void _maru_windows_fill_standard_controller_layout(
    MARU_Controller_Windows *ctrl) {
  uint32_t i;

  for (i = 0u; i < ctrl->button_count && i < MARU_CONTROLLER_BUTTON_STANDARD_COUNT;
       ++i) {
    ctrl->button_channels[i].name = MARU_STANDARD_BUTTON_NAMES[i];
    ctrl->button_channels[i].min_value = 0.0f;
    ctrl->button_channels[i].max_value = 1.0f;
  }
  for (i = 0u; i < ctrl->analog_count && i < MARU_CONTROLLER_ANALOG_STANDARD_COUNT;
       ++i) {
    ctrl->analog_channels[i].name = MARU_STANDARD_ANALOG_NAMES[i];
    if (i < MARU_CONTROLLER_ANALOG_LEFT_TRIGGER) {
      ctrl->analog_channels[i].min_value = -1.0f;
      ctrl->analog_channels[i].max_value = 1.0f;
    } else {
      ctrl->analog_channels[i].min_value = 0.0f;
      ctrl->analog_channels[i].max_value = 1.0f;
    }
  }
  for (i = 0u; i < ctrl->haptic_count && i < MARU_CONTROLLER_HAPTIC_STANDARD_COUNT;
       ++i) {
    ctrl->haptic_channels[i].name = MARU_STANDARD_HAPTIC_NAMES[i];
    ctrl->haptic_channels[i].min_value = 0.0f;
    ctrl->haptic_channels[i].max_value = 1.0f;
  }
}

static MARU_Scalar _maru_windows_normalize_u8_centered(uint8_t value,
                                                       bool invert) {
  MARU_Scalar normalized =
      (MARU_Scalar)(((double)value / 255.0) * 2.0 - 1.0);
  if (invert) {
    normalized = -normalized;
  }
  if (normalized < -1.0f) {
    return -1.0f;
  }
  if (normalized > 1.0f) {
    return 1.0f;
  }
  return normalized;
}

static MARU_Scalar _maru_windows_normalize_u8_trigger(uint8_t value) {
  return (MARU_Scalar)((double)value / 255.0);
}

static void _maru_windows_set_button_state(MARU_Context_Windows *ctx,
                                           MARU_Controller_Windows *ctrl,
                                           uint32_t button_id,
                                           bool pressed) {
  MARU_ButtonState8 new_state = pressed ? MARU_BUTTON_STATE_PRESSED
                                        : MARU_BUTTON_STATE_RELEASED;
  if (button_id >= ctrl->button_count ||
      ctrl->button_states[button_id] == new_state) {
    return;
  }
  ctrl->button_states[button_id] = new_state;
  _maru_windows_emit_controller_button_event(ctx, ctrl, button_id, new_state);
}

static void _maru_windows_update_standard_dpad(MARU_Context_Windows *ctx,
                                               MARU_Controller_Windows *ctrl,
                                               uint8_t dpad_value) {
  bool up = false;
  bool right = false;
  bool down = false;
  bool left = false;

  switch (dpad_value & 0x0Fu) {
  case 0:
    up = true;
    break;
  case 1:
    up = true;
    right = true;
    break;
  case 2:
    right = true;
    break;
  case 3:
    right = true;
    down = true;
    break;
  case 4:
    down = true;
    break;
  case 5:
    down = true;
    left = true;
    break;
  case 6:
    left = true;
    break;
  case 7:
    left = true;
    up = true;
    break;
  default:
    break;
  }

  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_UP, up);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_RIGHT,
                                 right);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_DOWN,
                                 down);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_DPAD_LEFT,
                                 left);
}

static char *_maru_windows_utf8_from_wide(MARU_Context_Base *ctx_base,
                                          const wchar_t *src) {
  int required;
  char *dst;

  if (!src) {
    return NULL;
  }

  required = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
  if (required <= 0) {
    return NULL;
  }

  dst = (char *)maru_context_alloc(ctx_base, (size_t)required);
  if (!dst) {
    return NULL;
  }

  if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, required, NULL, NULL) <=
      0) {
    maru_context_free(ctx_base, dst);
    return NULL;
  }

  return dst;
}

static wchar_t *_maru_windows_dup_wide(MARU_Context_Base *ctx_base,
                                       const wchar_t *src) {
  size_t len;
  wchar_t *dst;

  if (!src) {
    return NULL;
  }
  len = wcslen(src);
  dst = (wchar_t *)maru_context_alloc(ctx_base, (len + 1u) * sizeof(wchar_t));
  if (!dst) {
    return NULL;
  }
  memcpy(dst, src, (len + 1u) * sizeof(wchar_t));
  return dst;
}

static void _maru_windows_init_runtime(MARU_Context_Windows *ctx) {
  if (ctx->runtime_object_module) {
    return;
  }

  ctx->runtime_object_module = LoadLibraryW(L"combase.dll");
  if (ctx->runtime_object_module) {
    ctx->RoInitialize = (HRESULT(WINAPI *)(int))GetProcAddress(
        ctx->runtime_object_module, "RoInitialize");
    ctx->RoUninitialize = (void(WINAPI *)(void))GetProcAddress(
        ctx->runtime_object_module, "RoUninitialize");
    ctx->RoGetActivationFactory =
        (HRESULT(WINAPI *)(void *, REFIID, void **))GetProcAddress(
            ctx->runtime_object_module, "RoGetActivationFactory");
    ctx->WindowsCreateString =
        (HRESULT(WINAPI *)(LPCWSTR, UINT32, void **))GetProcAddress(
            ctx->runtime_object_module, "WindowsCreateString");
    ctx->WindowsDeleteString =
        (HRESULT(WINAPI *)(void *))GetProcAddress(ctx->runtime_object_module,
                                                  "WindowsDeleteString");
  }

  if (!ctx->RoInitialize) {
    HMODULE ro_mod = LoadLibraryW(L"api-ms-win-core-winrt-l1-1-0.dll");
    if (ro_mod) {
      if (!ctx->RoInitialize) {
        ctx->RoInitialize = (HRESULT(WINAPI *)(int))GetProcAddress(
            ro_mod, "RoInitialize");
      }
      if (!ctx->RoUninitialize) {
        ctx->RoUninitialize = (void(WINAPI *)(void))GetProcAddress(
            ro_mod, "RoUninitialize");
      }
      if (!ctx->RoGetActivationFactory) {
        ctx->RoGetActivationFactory =
            (HRESULT(WINAPI *)(void *, REFIID, void **))GetProcAddress(
                ro_mod, "RoGetActivationFactory");
      }
    }
  }

  if (ctx->RoInitialize) {
    ctx->RoInitialize(RO_INIT_MULTITHREADED);
  }
}

static IGamepadStatics *_maru_windows_get_wgi_statics(MARU_Context_Windows *ctx) {
  void *class_name = NULL;
  IGamepadStatics *statics = NULL;
  HRESULT hr;
  static const IID iid_IGamepadStatics = {0x8BBCE529,
                                          0xD49C,
                                          0x39E9,
                                          {0x95, 0x60, 0xE4, 0x7D, 0xDE,
                                           0x96, 0xB7, 0xC8}};

  if (!ctx->RoGetActivationFactory || !ctx->WindowsCreateString) {
    return NULL;
  }

  hr = ctx->WindowsCreateString(L"Windows.Gaming.Input.Gamepad", 28,
                                &class_name);
  if (FAILED(hr)) {
    return NULL;
  }

  hr = ctx->RoGetActivationFactory(class_name, &iid_IGamepadStatics,
                                   (void **)&statics);
  ctx->WindowsDeleteString(class_name);
  if (FAILED(hr)) {
    return NULL;
  }

  return statics;
}

static void _maru_windows_init_xinput(MARU_Context_Windows *ctx) {
  if (ctx->xinput_module) {
    return;
  }

  ctx->xinput_module = LoadLibraryW(L"xinput1_4.dll");
  if (!ctx->xinput_module) {
    ctx->xinput_module = LoadLibraryW(L"xinput1_3.dll");
  }
  if (!ctx->xinput_module) {
    ctx->xinput_module = LoadLibraryW(L"xinput9_1_0.dll");
  }

  if (ctx->xinput_module) {
    ctx->XInputGetState = (DWORD(WINAPI *)(DWORD, void *))GetProcAddress(
        ctx->xinput_module, "XInputGetState");
    ctx->XInputSetState = (DWORD(WINAPI *)(DWORD, void *))GetProcAddress(
        ctx->xinput_module, "XInputSetState");
  }
}

static void _maru_windows_emit_controller_changed_event(
    MARU_Context_Windows *ctx, MARU_Controller_Windows *ctrl, bool connected) {
  MARU_Event evt = {0};
  evt.controller_changed.controller = (MARU_Controller *)ctrl;
  evt.controller_changed.connected = connected;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_CONTROLLER_CHANGED,
                       NULL, &evt);
}

static void _maru_windows_emit_controller_button_event(
    MARU_Context_Windows *ctx, MARU_Controller_Windows *ctrl, uint32_t button_id,
    MARU_ButtonState8 new_state) {
  MARU_Event evt = {0};
  evt.controller_button_changed.controller = (MARU_Controller *)ctrl;
  evt.controller_button_changed.button_id = button_id;
  evt.controller_button_changed.state = (MARU_ButtonState)new_state;
  _maru_dispatch_event(&ctx->base, MARU_EVENT_CONTROLLER_BUTTON_CHANGED,
                       NULL, &evt);
}

static void _maru_windows_controller_free(MARU_Context_Base *ctx_base,
                                          MARU_Controller_Windows *ctrl) {
  if (!ctrl) {
    return;
  }

  if (ctrl->wgi_gamepad) {
    IGamepad *gamepad = (IGamepad *)ctrl->wgi_gamepad;
    gamepad->lpVtbl->Release(gamepad);
  }
  if (ctrl->hid_preparsed_data) {
    HidD_FreePreparsedData(ctrl->hid_preparsed_data);
  }
  if (ctrl->hid_handle && ctrl->hid_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(ctrl->hid_handle);
  }

  maru_context_free(ctx_base, ctrl->name);
  maru_context_free(ctx_base, ctrl->raw_device_path);
  maru_context_free(ctx_base, ctrl->button_states);
  maru_context_free(ctx_base, ctrl->button_channels);
  maru_context_free(ctx_base, ctrl->analog_states);
  maru_context_free(ctx_base, ctrl->analog_channels);
  maru_context_free(ctx_base, ctrl->haptic_channels);
  maru_context_free(ctx_base, ctrl->hid_button_mappings);
  maru_context_free(ctx_base, ctrl->hid_value_mappings);
  maru_context_free(ctx_base, ctrl->hid_pressed_usages);
  maru_context_free(ctx_base, ctrl);
}

void _maru_controller_free(MARU_Controller_Base *controller) {
  MARU_Controller_Windows *ctrl = (MARU_Controller_Windows *)controller;
  _maru_windows_controller_free(ctrl->base.ctx_base, ctrl);
}

static MARU_Controller_Windows *_maru_windows_controller_create_common(
    MARU_Context_Windows *ctx, uint32_t button_count, uint32_t analog_count,
    uint32_t haptic_count) {
  MARU_Controller_Windows *ctrl =
      (MARU_Controller_Windows *)maru_context_alloc(&ctx->base,
                                                    sizeof(MARU_Controller_Windows));
  if (!ctrl) {
    return NULL;
  }
  memset(ctrl, 0, sizeof(MARU_Controller_Windows));

  ctrl->base.ctx_base = &ctx->base;
  ctrl->base.is_active = true;
  atomic_init(&ctrl->base.ref_count, 1u);
  ctrl->xinput_index = 0xFFFFFFFFu;
  ctrl->hid_handle = INVALID_HANDLE_VALUE;
  ctrl->button_count = button_count;
  ctrl->analog_count = analog_count;
  ctrl->haptic_count = haptic_count;

  if (button_count > 0u) {
    ctrl->button_states = (MARU_ButtonState8 *)maru_context_alloc(
        &ctx->base, sizeof(MARU_ButtonState8) * button_count);
    ctrl->button_channels = (MARU_ChannelInfo *)maru_context_alloc(
        &ctx->base, sizeof(MARU_ChannelInfo) * button_count);
    if (!ctrl->button_states || !ctrl->button_channels) {
      _maru_windows_controller_free(&ctx->base, ctrl);
      return NULL;
    }
    memset(ctrl->button_states, 0, sizeof(MARU_ButtonState8) * button_count);
    memset(ctrl->button_channels, 0,
           sizeof(MARU_ChannelInfo) * button_count);
    for (uint32_t i = 0; i < button_count; ++i) {
      ctrl->button_channels[i].min_value = 0.0f;
      ctrl->button_channels[i].max_value = 1.0f;
    }
  }

  if (analog_count > 0u) {
    ctrl->analog_states = (MARU_AnalogInputState *)maru_context_alloc(
        &ctx->base, sizeof(MARU_AnalogInputState) * analog_count);
    ctrl->analog_channels = (MARU_ChannelInfo *)maru_context_alloc(
        &ctx->base, sizeof(MARU_ChannelInfo) * analog_count);
    if (!ctrl->analog_states || !ctrl->analog_channels) {
      _maru_windows_controller_free(&ctx->base, ctrl);
      return NULL;
    }
    memset(ctrl->analog_states, 0,
           sizeof(MARU_AnalogInputState) * analog_count);
    memset(ctrl->analog_channels, 0,
           sizeof(MARU_ChannelInfo) * analog_count);
    for (uint32_t i = 0; i < analog_count; ++i) {
      ctrl->analog_channels[i].min_value = 0.0f;
      ctrl->analog_channels[i].max_value = 1.0f;
    }
  }

  if (haptic_count > 0u) {
    ctrl->haptic_channels = (MARU_ChannelInfo *)maru_context_alloc(
        &ctx->base, sizeof(MARU_ChannelInfo) * haptic_count);
    if (!ctrl->haptic_channels) {
      _maru_windows_controller_free(&ctx->base, ctrl);
      return NULL;
    }
    memset(ctrl->haptic_channels, 0,
           sizeof(MARU_ChannelInfo) * haptic_count);
    for (uint32_t i = 0; i < haptic_count; ++i) {
      ctrl->haptic_channels[i].min_value = 0.0f;
      ctrl->haptic_channels[i].max_value = 1.0f;
    }
  }

  ctrl->base.pub.context = (MARU_Context *)ctx;

  return ctrl;
}

static MARU_Controller_Windows *_maru_windows_controller_create_xinput(
    MARU_Context_Windows *ctx, uint32_t index) {
  MARU_Controller_Windows *ctrl;
  char name_buf[64];

  ctrl = _maru_windows_controller_create_common(
      ctx, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
      MARU_CONTROLLER_ANALOG_STANDARD_COUNT,
      MARU_CONTROLLER_HAPTIC_STANDARD_COUNT);
  if (!ctrl) {
    return NULL;
  }
  ctrl->xinput_index = index;

  snprintf(name_buf, sizeof(name_buf), "Xbox Controller (XInput #%u)", index);
  ctrl->name = _maru_windows_dup_utf8(&ctx->base, name_buf);
  if (!ctrl->name) {
    _maru_windows_controller_free(&ctx->base, ctrl);
    return NULL;
  }

  _maru_windows_fill_standard_controller_layout(ctrl);
  ctrl->is_standardized = true;
  ctrl->vendor_id = 0x045E;
  ctrl->product_id = 0x028E;
  memset(ctrl->guid, 0, sizeof(ctrl->guid));
  memcpy(ctrl->guid, "XINPUT", 6u);
  ctrl->guid[6] = (uint8_t)index;
  _maru_windows_controller_sync_pub(ctrl);
  return ctrl;
}

static MARU_Controller_Windows *_maru_windows_controller_create_wgi(
    MARU_Context_Windows *ctx, IGamepad *gamepad) {
  MARU_Controller_Windows *ctrl;

  ctrl = _maru_windows_controller_create_common(
      ctx, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
      MARU_CONTROLLER_ANALOG_STANDARD_COUNT,
      MARU_CONTROLLER_HAPTIC_STANDARD_COUNT);
  if (!ctrl) {
    return NULL;
  }

  ctrl->wgi_gamepad = gamepad;
  gamepad->lpVtbl->AddRef(gamepad);
  ctrl->name = _maru_windows_dup_utf8(&ctx->base, "Standard Gamepad (WGI)");
  if (!ctrl->name) {
    _maru_windows_controller_free(&ctx->base, ctrl);
    return NULL;
  }

  _maru_windows_fill_standard_controller_layout(ctrl);
  ctrl->is_standardized = true;
  memset(ctrl->guid, 0, sizeof(ctrl->guid));
  memcpy(ctrl->guid, "WGI", 3u);
  _maru_windows_controller_sync_pub(ctrl);
  return ctrl;
}

static MARU_Controller_Windows *_maru_windows_controller_create_dualshock4(
    MARU_Context_Windows *ctx, HANDLE raw_device, HANDLE hid_handle,
    PHIDP_PREPARSED_DATA preparsed, const HIDD_ATTRIBUTES *attributes,
    const wchar_t *raw_path, const wchar_t *product_name) {
  MARU_Controller_Windows *ctrl;
  char fallback_name[96];

  ctrl = _maru_windows_controller_create_common(
      ctx, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
      MARU_CONTROLLER_ANALOG_STANDARD_COUNT, 0u);
  if (!ctrl) {
    return NULL;
  }

  ctrl->raw_input_device = raw_device;
  ctrl->hid_handle = hid_handle;
  ctrl->hid_preparsed_data = preparsed;
  ctrl->hid_kind = MARU_WINDOWS_HID_KIND_DUALSHOCK4;
  ctrl->raw_device_path = _maru_windows_dup_wide(&ctx->base, raw_path);
  ctrl->vendor_id = attributes->VendorID;
  ctrl->product_id = attributes->ProductID;
  ctrl->version = attributes->VersionNumber;
  ctrl->is_standardized = true;
  memset(ctrl->guid, 0, sizeof(ctrl->guid));
  memcpy(ctrl->guid, "DS4HID", 6u);

  if (product_name) {
    ctrl->name = _maru_windows_utf8_from_wide(&ctx->base, product_name);
  }
  if (!ctrl->name) {
    snprintf(fallback_name, sizeof(fallback_name), "DualShock 4 %04X:%04X",
             (unsigned)ctrl->vendor_id, (unsigned)ctrl->product_id);
    ctrl->name = _maru_windows_dup_utf8(&ctx->base, fallback_name);
  }
  if (!ctrl->name || !ctrl->raw_device_path) {
    _maru_windows_controller_free(&ctx->base, ctrl);
    return NULL;
  }

  _maru_windows_fill_standard_controller_layout(ctrl);
  _maru_windows_controller_sync_pub(ctrl);
  return ctrl;
}

static MARU_Controller_Windows *_maru_windows_controller_create_dualsense(
    MARU_Context_Windows *ctx, HANDLE raw_device, HANDLE hid_handle,
    PHIDP_PREPARSED_DATA preparsed, const HIDD_ATTRIBUTES *attributes,
    const wchar_t *raw_path, const wchar_t *product_name) {
  MARU_Controller_Windows *ctrl;
  char fallback_name[96];

  ctrl = _maru_windows_controller_create_common(
      ctx, MARU_CONTROLLER_BUTTON_STANDARD_COUNT,
      MARU_CONTROLLER_ANALOG_STANDARD_COUNT, 0u);
  if (!ctrl) {
    return NULL;
  }

  ctrl->raw_input_device = raw_device;
  ctrl->hid_handle = hid_handle;
  ctrl->hid_preparsed_data = preparsed;
  ctrl->hid_kind = MARU_WINDOWS_HID_KIND_DUALSENSE;
  ctrl->raw_device_path = _maru_windows_dup_wide(&ctx->base, raw_path);
  ctrl->vendor_id = attributes->VendorID;
  ctrl->product_id = attributes->ProductID;
  ctrl->version = attributes->VersionNumber;
  ctrl->is_standardized = true;
  memset(ctrl->guid, 0, sizeof(ctrl->guid));
  memcpy(ctrl->guid, "DS5HID", 6u);

  if (product_name) {
    ctrl->name = _maru_windows_utf8_from_wide(&ctx->base, product_name);
  }
  if (!ctrl->name) {
    snprintf(fallback_name, sizeof(fallback_name), "DualSense %04X:%04X",
             (unsigned)ctrl->vendor_id, (unsigned)ctrl->product_id);
    ctrl->name = _maru_windows_dup_utf8(&ctx->base, fallback_name);
  }
  if (!ctrl->name || !ctrl->raw_device_path) {
    _maru_windows_controller_free(&ctx->base, ctrl);
    return NULL;
  }

  _maru_windows_fill_standard_controller_layout(ctrl);
  _maru_windows_controller_sync_pub(ctrl);
  return ctrl;
}

static bool _maru_windows_hid_expand_button_caps(
    const HIDP_BUTTON_CAPS *caps, uint16_t *out_count) {
  USHORT count;

  if (caps->UsagePage != HID_USAGE_PAGE_BUTTON) {
    return false;
  }

  if (caps->IsRange) {
    if (caps->Range.UsageMax < caps->Range.UsageMin) {
      return false;
    }
    count = (USHORT)(caps->Range.UsageMax - caps->Range.UsageMin + 1u);
  } else {
    count = 1u;
  }

  if (out_count) {
    *out_count = (uint16_t)count;
  }
  return count > 0u;
}

static bool _maru_windows_hid_expand_value_caps(
    const HIDP_VALUE_CAPS *caps, uint16_t *out_count) {
  USHORT count;

  if (caps->UsagePage != HID_USAGE_PAGE_GENERIC) {
    return false;
  }

  if (caps->IsRange) {
    if (caps->Range.UsageMax < caps->Range.UsageMin) {
      return false;
    }
    count = (USHORT)(caps->Range.UsageMax - caps->Range.UsageMin + 1u);
  } else {
    count = 1u;
  }

  if (out_count) {
    *out_count = (uint16_t)count;
  }
  return count > 0u;
}

static MARU_Controller_Windows *_maru_windows_controller_create_raw_hid(
    MARU_Context_Windows *ctx, HANDLE raw_device, const wchar_t *raw_path,
    uint16_t usage_page, uint16_t usage) {
  HANDLE hid_handle = INVALID_HANDLE_VALUE;
  PHIDP_PREPARSED_DATA preparsed = NULL;
  HIDP_CAPS caps;
  HIDD_ATTRIBUTES attributes;
  wchar_t product_name[128];
  USHORT button_caps_len = 0u;
  USHORT value_caps_len = 0u;
  HIDP_BUTTON_CAPS *button_caps = NULL;
  HIDP_VALUE_CAPS *value_caps = NULL;
  uint32_t button_count = 0u;
  uint32_t analog_count = 0u;
  uint32_t button_index = 0u;
  uint32_t analog_index = 0u;
  uint32_t i;
  MARU_Controller_Windows *ctrl = NULL;
  char fallback_name[96];
  MARU_WindowsHidControllerKind hid_kind;

  hid_handle = CreateFileW(raw_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
  if (hid_handle == INVALID_HANDLE_VALUE) {
    return NULL;
  }

  memset(&attributes, 0, sizeof(attributes));
  attributes.Size = sizeof(attributes);
  if (!HidD_GetAttributes(hid_handle, &attributes)) {
    CloseHandle(hid_handle);
    return NULL;
  }

  if (!HidD_GetPreparsedData(hid_handle, &preparsed)) {
    CloseHandle(hid_handle);
    return NULL;
  }
  if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS) {
    HidD_FreePreparsedData(preparsed);
    CloseHandle(hid_handle);
    return NULL;
  }

  hid_kind = _maru_windows_detect_hid_controller_kind(attributes.VendorID,
                                                      attributes.ProductID);
  if (hid_kind == MARU_WINDOWS_HID_KIND_DUALSHOCK4) {
    ctrl = _maru_windows_controller_create_dualshock4(
        ctx, raw_device, hid_handle, preparsed, &attributes, raw_path,
        HidD_GetProductString(hid_handle, product_name, sizeof(product_name))
            ? product_name
            : NULL);
    if (!ctrl) {
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
    }
    return ctrl;
  }
  if (hid_kind == MARU_WINDOWS_HID_KIND_DUALSENSE) {
    ctrl = _maru_windows_controller_create_dualsense(
        ctx, raw_device, hid_handle, preparsed, &attributes, raw_path,
        HidD_GetProductString(hid_handle, product_name, sizeof(product_name))
            ? product_name
            : NULL);
    if (!ctrl) {
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
    }
    return ctrl;
  }

  button_caps_len = caps.NumberInputButtonCaps;
  value_caps_len = caps.NumberInputValueCaps;
  if (button_caps_len > 0u) {
    button_caps = (HIDP_BUTTON_CAPS *)maru_context_alloc(
        &ctx->base, sizeof(HIDP_BUTTON_CAPS) * button_caps_len);
    if (!button_caps) {
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
      return NULL;
    }
    if (HidP_GetButtonCaps(HidP_Input, button_caps, &button_caps_len,
                           preparsed) != HIDP_STATUS_SUCCESS) {
      maru_context_free(&ctx->base, button_caps);
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
      return NULL;
    }
  }
  if (value_caps_len > 0u) {
    value_caps = (HIDP_VALUE_CAPS *)maru_context_alloc(
        &ctx->base, sizeof(HIDP_VALUE_CAPS) * value_caps_len);
    if (!value_caps) {
      maru_context_free(&ctx->base, button_caps);
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
      return NULL;
    }
    if (HidP_GetValueCaps(HidP_Input, value_caps, &value_caps_len,
                          preparsed) != HIDP_STATUS_SUCCESS) {
      maru_context_free(&ctx->base, value_caps);
      maru_context_free(&ctx->base, button_caps);
      HidD_FreePreparsedData(preparsed);
      CloseHandle(hid_handle);
      return NULL;
    }
  }

  for (i = 0; i < button_caps_len; ++i) {
    uint16_t cap_count = 0u;
    if (_maru_windows_hid_expand_button_caps(&button_caps[i], &cap_count)) {
      button_count += cap_count;
    }
  }
  for (i = 0; i < value_caps_len; ++i) {
    if (!_maru_windows_hid_expand_value_caps(&value_caps[i], NULL)) {
      continue;
    }
    if (value_caps[i].IsRange) {
      USAGE u;
      for (u = value_caps[i].Range.UsageMin; u <= value_caps[i].Range.UsageMax;
           ++u) {
        if (u == HID_USAGE_GENERIC_HATSWITCH) {
          button_count += 4u;
        } else {
          analog_count += 1u;
        }
      }
    } else if (value_caps[i].NotRange.Usage == HID_USAGE_GENERIC_HATSWITCH) {
      button_count += 4u;
    } else {
      analog_count += 1u;
    }
  }

  if (button_count == 0u && analog_count == 0u) {
    maru_context_free(&ctx->base, value_caps);
    maru_context_free(&ctx->base, button_caps);
    HidD_FreePreparsedData(preparsed);
    CloseHandle(hid_handle);
    return NULL;
  }

  ctrl =
      _maru_windows_controller_create_common(ctx, button_count, analog_count, 0);
  if (!ctrl) {
    maru_context_free(&ctx->base, value_caps);
    maru_context_free(&ctx->base, button_caps);
    HidD_FreePreparsedData(preparsed);
    CloseHandle(hid_handle);
    return NULL;
  }

  ctrl->raw_input_device = raw_device;
  ctrl->hid_handle = hid_handle;
  ctrl->hid_preparsed_data = preparsed;
  ctrl->hid_kind = MARU_WINDOWS_HID_KIND_GENERIC;
  ctrl->raw_device_path = _maru_windows_dup_wide(&ctx->base, raw_path);
  ctrl->vendor_id = attributes.VendorID;
  ctrl->product_id = attributes.ProductID;
  ctrl->version = attributes.VersionNumber;
  ctrl->is_standardized = false;
  memset(ctrl->guid, 0, sizeof(ctrl->guid));
  memcpy(ctrl->guid, "RAWHID", 6u);

  if (HidD_GetProductString(hid_handle, product_name, sizeof(product_name))) {
    ctrl->name = _maru_windows_utf8_from_wide(&ctx->base, product_name);
  }
  if (!ctrl->name) {
    snprintf(fallback_name, sizeof(fallback_name),
             "HID Controller %04X:%04X", (unsigned)ctrl->vendor_id,
             (unsigned)ctrl->product_id);
    ctrl->name = _maru_windows_dup_utf8(&ctx->base, fallback_name);
  }
  if (!ctrl->name || !ctrl->raw_device_path) {
    _maru_windows_controller_free(&ctx->base, ctrl);
    maru_context_free(&ctx->base, value_caps);
    maru_context_free(&ctx->base, button_caps);
    return NULL;
  }

  if (button_count > 0u) {
    ctrl->hid_button_mappings = (MARU_WindowsHidButtonMapping *)
        maru_context_alloc(&ctx->base,
                           sizeof(MARU_WindowsHidButtonMapping) * button_count);
    ctrl->hid_pressed_usages = (USAGE_AND_PAGE *)maru_context_alloc(
        &ctx->base, sizeof(USAGE_AND_PAGE) * button_count);
    if (!ctrl->hid_button_mappings || !ctrl->hid_pressed_usages) {
      _maru_windows_controller_free(&ctx->base, ctrl);
      maru_context_free(&ctx->base, value_caps);
      maru_context_free(&ctx->base, button_caps);
      return NULL;
    }
    ctrl->hid_pressed_usage_capacity = (ULONG)button_count;
  }

  if (analog_count > 0u) {
    ctrl->hid_value_mappings = (MARU_WindowsHidValueMapping *)maru_context_alloc(
        &ctx->base,
        sizeof(MARU_WindowsHidValueMapping) * analog_count);
    if (!ctrl->hid_value_mappings) {
      _maru_windows_controller_free(&ctx->base, ctrl);
      maru_context_free(&ctx->base, value_caps);
      maru_context_free(&ctx->base, button_caps);
      return NULL;
    }
  }

  for (i = 0; i < button_caps_len; ++i) {
    if (button_caps[i].UsagePage != HID_USAGE_PAGE_BUTTON) {
      continue;
    }

    if (button_caps[i].IsRange) {
      USAGE usage_iter;
      for (usage_iter = button_caps[i].Range.UsageMin;
           usage_iter <= button_caps[i].Range.UsageMax; ++usage_iter) {
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "Button %u", (unsigned)usage_iter);
        ctrl->hid_button_mappings[button_index].usage_page = HID_USAGE_PAGE_BUTTON;
        ctrl->hid_button_mappings[button_index].usage = usage_iter;
        ctrl->button_channels[button_index].name =
            _maru_windows_dup_utf8(&ctx->base, name_buf);
        if (!ctrl->button_channels[button_index].name) {
          _maru_windows_controller_free(&ctx->base, ctrl);
          maru_context_free(&ctx->base, value_caps);
          maru_context_free(&ctx->base, button_caps);
          return NULL;
        }
        button_index++;
      }
    } else {
      char name_buf[32];
      USAGE single_usage = button_caps[i].NotRange.Usage;
      snprintf(name_buf, sizeof(name_buf), "Button %u", (unsigned)single_usage);
      ctrl->hid_button_mappings[button_index].usage_page = HID_USAGE_PAGE_BUTTON;
      ctrl->hid_button_mappings[button_index].usage = single_usage;
      ctrl->button_channels[button_index].name =
          _maru_windows_dup_utf8(&ctx->base, name_buf);
      if (!ctrl->button_channels[button_index].name) {
        _maru_windows_controller_free(&ctx->base, ctrl);
        maru_context_free(&ctx->base, value_caps);
        maru_context_free(&ctx->base, button_caps);
        return NULL;
      }
      button_index++;
    }
  }

  for (i = 0; i < value_caps_len; ++i) {
    USAGE usage_min;
    USAGE usage_max;
    USAGE usage_iter;

    if (value_caps[i].UsagePage != HID_USAGE_PAGE_GENERIC) {
      continue;
    }

    usage_min =
        value_caps[i].IsRange ? value_caps[i].Range.UsageMin
                              : value_caps[i].NotRange.Usage;
    usage_max =
        value_caps[i].IsRange ? value_caps[i].Range.UsageMax
                              : value_caps[i].NotRange.Usage;

    for (usage_iter = usage_min; usage_iter <= usage_max; ++usage_iter) {
      if (usage_iter == HID_USAGE_GENERIC_HATSWITCH) {
        ctrl->hid_has_hat_switch = true;
        ctrl->hid_hat_usage = usage_iter;
        ctrl->hid_hat_logical_min = value_caps[i].LogicalMin;
        ctrl->hid_hat_logical_max = value_caps[i].LogicalMax;
        ctrl->hid_hat_button_base = button_index;
        ctrl->button_channels[button_index + 0u].name = "DpadUp";
        ctrl->button_channels[button_index + 1u].name = "DpadRight";
        ctrl->button_channels[button_index + 2u].name = "DpadDown";
        ctrl->button_channels[button_index + 3u].name = "DpadLeft";
        button_index += 4u;
      } else {
        const char *usage_name = _maru_windows_generic_usage_name(usage_iter);
        char name_buf[32];
        ctrl->hid_value_mappings[analog_index].usage_page = HID_USAGE_PAGE_GENERIC;
        ctrl->hid_value_mappings[analog_index].usage = usage_iter;
        ctrl->hid_value_mappings[analog_index].logical_min =
            value_caps[i].LogicalMin;
        ctrl->hid_value_mappings[analog_index].logical_max =
            value_caps[i].LogicalMax;
        ctrl->hid_value_mappings[analog_index].is_centered =
            _maru_windows_hid_usage_is_centered(usage_iter);
        if (ctrl->hid_value_mappings[analog_index].is_centered) {
          ctrl->analog_channels[analog_index].min_value = -1.0f;
          ctrl->analog_channels[analog_index].max_value = 1.0f;
        } else {
          ctrl->analog_channels[analog_index].min_value = 0.0f;
          ctrl->analog_channels[analog_index].max_value = 1.0f;
        }
        if (!usage_name) {
          snprintf(name_buf, sizeof(name_buf), "Usage 0x%02X",
                   (unsigned)usage_iter);
          usage_name = name_buf;
        }
        ctrl->analog_channels[analog_index].name =
            _maru_windows_dup_utf8(&ctx->base, usage_name);
        if (!ctrl->analog_channels[analog_index].name) {
          _maru_windows_controller_free(&ctx->base, ctrl);
          maru_context_free(&ctx->base, value_caps);
          maru_context_free(&ctx->base, button_caps);
          return NULL;
        }
        analog_index++;
      }
    }
  }

  ctrl->hid_button_mapping_count =
      button_index - (ctrl->hid_has_hat_switch ? 4u : 0u);
  ctrl->hid_value_mapping_count = analog_index;

  maru_context_free(&ctx->base, value_caps);
  maru_context_free(&ctx->base, button_caps);
  (void)usage_page;
  (void)usage;
  _maru_windows_controller_sync_pub(ctrl);
  return ctrl;
}

static MARU_Controller_Windows *_maru_windows_find_wgi_controller(
    MARU_Context_Windows *ctx, IGamepad *gamepad) {
  MARU_Controller_Windows *it;
  for (it = ctx->controller_list_head; it; it = it->next) {
    if (it->wgi_gamepad == (void *)gamepad) {
      return it;
    }
  }
  return NULL;
}

static MARU_Controller_Windows *_maru_windows_find_xinput_controller(
    MARU_Context_Windows *ctx, uint32_t index) {
  MARU_Controller_Windows *it;
  for (it = ctx->controller_list_head; it; it = it->next) {
    if (it->xinput_index == index) {
      return it;
    }
  }
  return NULL;
}

static MARU_Controller_Windows *_maru_windows_find_raw_controller(
    MARU_Context_Windows *ctx, HANDLE raw_device, const wchar_t *raw_path) {
  MARU_Controller_Windows *it;
  for (it = ctx->controller_list_head; it; it = it->next) {
    if (it->raw_input_device == raw_device) {
      return it;
    }
    if (raw_path && it->raw_device_path && wcscmp(it->raw_device_path, raw_path) == 0) {
      return it;
    }
  }
  return NULL;
}

static void _maru_windows_mark_controller_present(MARU_Controller_Windows *ctrl) {
  ctrl->base.pub.flags &= ~MARU_CONTROLLER_FLAG_PENDING_REMOVAL;
  ctrl->base.pub.flags &= ~MARU_CONTROLLER_STATE_LOST;
  ctrl->base.is_active = true;
}

static void _maru_windows_append_controller(MARU_Context_Windows *ctx,
                                            MARU_Controller_Windows *ctrl) {
  ctrl->next = ctx->controller_list_head;
  ctx->controller_list_head = ctrl;
  ctx->controller_count++;
}

static void _maru_windows_resync_rawinput_devices(MARU_Context_Windows *ctx) {
  UINT count = 0u;
  RAWINPUTDEVICELIST *device_list = NULL;
  UINT i;

  if (GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST)) != 0u ||
      count == 0u) {
    return;
  }

  device_list = (RAWINPUTDEVICELIST *)maru_context_alloc(
      &ctx->base, sizeof(RAWINPUTDEVICELIST) * count);
  if (!device_list) {
    return;
  }

  if (GetRawInputDeviceList(device_list, &count, sizeof(RAWINPUTDEVICELIST)) ==
      (UINT)-1) {
    maru_context_free(&ctx->base, device_list);
    return;
  }

  for (i = 0u; i < count; ++i) {
    RID_DEVICE_INFO info;
    UINT info_size = sizeof(info);
    UINT path_size = 0u;
    wchar_t *path = NULL;
    MARU_Controller_Windows *existing;
    MARU_Controller_Windows *ctrl;

    if (device_list[i].dwType != RIM_TYPEHID) {
      continue;
    }

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (GetRawInputDeviceInfoW(device_list[i].hDevice, RIDI_DEVICEINFO, &info,
                               &info_size) == (UINT)-1) {
      continue;
    }
    if (!_maru_windows_is_game_controller_usage(info.hid.usUsagePage,
                                                info.hid.usUsage)) {
      continue;
    }

    if (GetRawInputDeviceInfoW(device_list[i].hDevice, RIDI_DEVICENAME, NULL,
                               &path_size) == (UINT)-1 ||
        path_size == 0u) {
      continue;
    }

    path = (wchar_t *)maru_context_alloc(&ctx->base,
                                         (size_t)(path_size + 1u) *
                                             sizeof(wchar_t));
    if (!path) {
      continue;
    }

    if (GetRawInputDeviceInfoW(device_list[i].hDevice, RIDI_DEVICENAME, path,
                               &path_size) == (UINT)-1) {
      maru_context_free(&ctx->base, path);
      continue;
    }
    path[path_size] = L'\0';

    if (_maru_windows_is_xinput_hid_path(path)) {
      maru_context_free(&ctx->base, path);
      continue;
    }

    existing = _maru_windows_find_raw_controller(ctx, device_list[i].hDevice, path);
    if (existing) {
      existing->raw_input_device = device_list[i].hDevice;
      _maru_windows_mark_controller_present(existing);
      maru_context_free(&ctx->base, path);
      continue;
    }

    ctrl = _maru_windows_controller_create_raw_hid(
        ctx, device_list[i].hDevice, path, info.hid.usUsagePage, info.hid.usUsage);
    maru_context_free(&ctx->base, path);
    if (!ctrl) {
      continue;
    }

    _maru_windows_append_controller(ctx, ctrl);
    _maru_windows_emit_controller_changed_event(ctx, ctrl, true);
  }

  maru_context_free(&ctx->base, device_list);
}

void _maru_windows_resync_controllers(MARU_Context_Windows *ctx) {
  MARU_Controller_Windows **curr;
  IGamepadStatics *statics;
  uint32_t xinput_present_count = 0u;

  _maru_windows_init_runtime(ctx);
  _maru_windows_init_xinput(ctx);

  for (curr = &ctx->controller_list_head; *curr; curr = &((*curr)->next)) {
    (*curr)->base.pub.flags |= MARU_CONTROLLER_FLAG_PENDING_REMOVAL;
  }

  if (ctx->XInputGetState) {
    DWORD i;
    for (i = 0u; i < XUSER_MAX_COUNT; ++i) {
      XINPUT_STATE state;
      if (ctx->XInputGetState(i, &state) == ERROR_SUCCESS) {
        xinput_present_count++;
        MARU_Controller_Windows *existing =
            _maru_windows_find_xinput_controller(ctx, (uint32_t)i);
        if (existing) {
          _maru_windows_mark_controller_present(existing);
        } else {
          MARU_Controller_Windows *ctrl =
              _maru_windows_controller_create_xinput(ctx, (uint32_t)i);
          if (ctrl) {
            _maru_windows_append_controller(ctx, ctrl);
            _maru_windows_emit_controller_changed_event(ctx, ctrl, true);
          }
        }
      }
    }
  }

  statics = _maru_windows_get_wgi_statics(ctx);
  if (statics) {
    IGamepadVectorView *gamepads = NULL;
    if (SUCCEEDED(statics->lpVtbl->get_Gamepads(statics, &gamepads))) {
      uint32_t count = 0u;
      uint32_t i;
      uint32_t remaining_xinput_duplicates = xinput_present_count;
      gamepads->lpVtbl->get_Size(gamepads, &count);
      for (i = 0u; i < count; ++i) {
        IGamepad *gamepad = NULL;
        if (SUCCEEDED(gamepads->lpVtbl->GetAt(gamepads, i, &gamepad))) {
          bool skip_duplicate = remaining_xinput_duplicates > 0u;
          if (skip_duplicate) {
            remaining_xinput_duplicates--;
          }

          if (!skip_duplicate) {
            MARU_Controller_Windows *existing =
                _maru_windows_find_wgi_controller(ctx, gamepad);
            if (existing) {
              _maru_windows_mark_controller_present(existing);
            } else {
              MARU_Controller_Windows *ctrl =
                  _maru_windows_controller_create_wgi(ctx, gamepad);
              if (ctrl) {
                _maru_windows_append_controller(ctx, ctrl);
                _maru_windows_emit_controller_changed_event(ctx, ctrl, true);
              }
            }
          }
          gamepad->lpVtbl->Release(gamepad);
        }
      }
      gamepads->lpVtbl->Release(gamepads);
    }
    statics->lpVtbl->Release(statics);
  }

  _maru_windows_resync_rawinput_devices(ctx);

  curr = &ctx->controller_list_head;
  while (*curr) {
    if (((*curr)->base.pub.flags & MARU_CONTROLLER_FLAG_PENDING_REMOVAL) != 0u) {
      MARU_Controller_Windows *to_remove = *curr;
      *curr = to_remove->next;
      ctx->controller_count--;
      _maru_windows_emit_controller_changed_event(ctx, to_remove, false);
      to_remove->base.is_active = false;
      to_remove->base.pub.flags |= MARU_CONTROLLER_STATE_LOST;
      to_remove->base.pub.flags &= ~MARU_CONTROLLER_FLAG_PENDING_REMOVAL;
      maru_releaseController((MARU_Controller *)to_remove);
    } else {
      curr = &((*curr)->next);
    }
  }
}

MARU_Status maru_getControllers_Windows(MARU_Context *context,
                                        MARU_ControllerList *out_list) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  uint32_t i = 0u;
  MARU_Controller_Windows *it;

  _maru_windows_resync_controllers(ctx);

  if (ctx->controller_count > ctx->controller_list_capacity) {
    uint32_t new_capacity = ctx->controller_count;
    MARU_Controller **new_list = (MARU_Controller **)maru_context_realloc(
        &ctx->base, ctx->controller_list_storage,
        ctx->controller_list_capacity * sizeof(MARU_Controller *),
        new_capacity * sizeof(MARU_Controller *));
    if (!new_list) {
      return MARU_FAILURE;
    }
    ctx->controller_list_storage = new_list;
    ctx->controller_list_capacity = new_capacity;
  }

  for (it = ctx->controller_list_head; it; it = it->next) {
    ctx->controller_list_storage[i++] = (MARU_Controller *)it;
  }

  out_list->controllers = ctx->controller_list_storage;
  out_list->count = i;
  return MARU_SUCCESS;
}

void maru_retainController_Windows(MARU_Controller *controller) {
  MARU_Controller_Windows *ctrl = (MARU_Controller_Windows *)controller;
  atomic_fetch_add_explicit(&ctrl->base.ref_count, 1u, memory_order_relaxed);
}

void maru_releaseController_Windows(MARU_Controller *controller) {
  MARU_Controller_Windows *ctrl = (MARU_Controller_Windows *)controller;
  if (atomic_fetch_sub_explicit(&ctrl->base.ref_count, 1u,
                                memory_order_acq_rel) == 1u) {
    _maru_controller_free(&ctrl->base);
  }
}

MARU_Status maru_getControllerInfo_Windows(const MARU_Controller *controller,
                                           MARU_ControllerInfo *out_info) {
  MARU_Controller_Windows *ctrl = (MARU_Controller_Windows *)controller;
  memset(out_info, 0, sizeof(*out_info));
  out_info->name = ctrl->base.pub.name;
  out_info->vendor_id = ctrl->base.pub.vendor_id;
  out_info->product_id = ctrl->base.pub.product_id;
  out_info->version = ctrl->base.pub.version;
  memcpy(out_info->guid, ctrl->base.pub.guid, sizeof(out_info->guid));
  out_info->is_standardized = ctrl->is_standardized;
  return MARU_SUCCESS;
}

MARU_Status maru_setControllerHapticLevels_Windows(MARU_Controller *controller,
                                                   uint32_t first_haptic,
                                                   uint32_t count,
                                                   const MARU_Scalar *intensities) {
  MARU_Controller_Windows *ctrl = (MARU_Controller_Windows *)controller;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)ctrl->base.pub.context;
  uint32_t i;

  if (!ctrl->base.is_active) {
    return MARU_FAILURE;
  }

  for (i = 0u; i < count; ++i) {
    uint32_t id = first_haptic + i;
    if (id < MARU_CONTROLLER_HAPTIC_STANDARD_COUNT &&
        ctrl->last_haptic_levels[id] != intensities[i]) {
      ctrl->last_haptic_levels[id] = intensities[i];
      ctrl->haptics_dirty = true;
    }
  }

  if (!ctrl->haptics_dirty) {
    return MARU_SUCCESS;
  }

  if (ctrl->wgi_gamepad) {
    IGamepad *gamepad = (IGamepad *)ctrl->wgi_gamepad;
    GamepadVibration vib = {(double)ctrl->last_haptic_levels
                                 [MARU_CONTROLLER_HAPTIC_LOW_FREQ],
                             (double)ctrl->last_haptic_levels
                                 [MARU_CONTROLLER_HAPTIC_HIGH_FREQ],
                             0.0,
                             0.0};
    gamepad->lpVtbl->put_Vibration(gamepad, vib);
  } else if (ctrl->xinput_index != 0xFFFFFFFFu && ctx->XInputSetState) {
    XINPUT_VIBRATION vib;
    vib.wLeftMotorSpeed = (WORD)(ctrl->last_haptic_levels
                                     [MARU_CONTROLLER_HAPTIC_LOW_FREQ] *
                                 65535.0f);
    vib.wRightMotorSpeed = (WORD)(ctrl->last_haptic_levels
                                      [MARU_CONTROLLER_HAPTIC_HIGH_FREQ] *
                                  65535.0f);
    ctx->XInputSetState(ctrl->xinput_index, &vib);
  }

  ctrl->haptics_dirty = false;
  return MARU_SUCCESS;
}

void _maru_windows_cleanup_controllers(MARU_Context_Windows *ctx) {
  while (ctx->controller_list_head) {
    MARU_Controller_Windows *ctrl = ctx->controller_list_head;
    ctx->controller_list_head = ctrl->next;
    maru_releaseController((MARU_Controller *)ctrl);
  }

  if (ctx->controller_list_storage) {
    maru_context_free(&ctx->base, ctx->controller_list_storage);
    ctx->controller_list_storage = NULL;
  }
  ctx->controller_count = 0u;
  ctx->controller_list_capacity = 0u;

  if (ctx->xinput_module) {
    FreeLibrary(ctx->xinput_module);
    ctx->xinput_module = NULL;
  }
  if (ctx->runtime_object_module) {
    if (ctx->RoUninitialize) {
      ctx->RoUninitialize();
    }
    FreeLibrary(ctx->runtime_object_module);
    ctx->runtime_object_module = NULL;
  }
}

static bool _maru_windows_usage_is_pressed(const USAGE_AND_PAGE *usages,
                                           ULONG usage_count, USAGE usage_page,
                                           USAGE usage) {
  ULONG i;
  for (i = 0u; i < usage_count; ++i) {
    if (usages[i].UsagePage == usage_page && usages[i].Usage == usage) {
      return true;
    }
  }
  return false;
}

static bool _maru_windows_process_dualshock4_report(
    MARU_Context_Windows *ctx, MARU_Controller_Windows *ctrl,
    const BYTE *report, UINT report_size) {
  size_t base = 0u;
  uint8_t buttons1;
  uint8_t buttons2;
  uint8_t buttons3;

  if (!report || report_size < 10u) {
    return false;
  }

  if (report[0] == 0x01u) {
    if (report_size < 10u) {
      return false;
    }
    base = 1u;
  } else if (report[0] == 0x11u) {
    if (report_size < 12u) {
      return false;
    }
    base = 3u;
  }

  buttons1 = report[base + 4u];
  buttons2 = report[base + 5u];
  buttons3 = report[base + 6u];

  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_X].value =
      _maru_windows_normalize_u8_centered(report[base + 0u], false);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_Y].value =
      _maru_windows_normalize_u8_centered(report[base + 1u], true);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_X].value =
      _maru_windows_normalize_u8_centered(report[base + 2u], false);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_Y].value =
      _maru_windows_normalize_u8_centered(report[base + 3u], true);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].value =
      _maru_windows_normalize_u8_trigger(report[base + 7u]);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].value =
      _maru_windows_normalize_u8_trigger(report[base + 8u]);

  _maru_windows_update_standard_dpad(ctx, ctrl, buttons1 & 0x0Fu);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_WEST,
                                 (buttons1 & 0x10u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_SOUTH,
                                 (buttons1 & 0x20u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_EAST,
                                 (buttons1 & 0x40u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_NORTH,
                                 (buttons1 & 0x80u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_LB,
                                 (buttons2 & 0x01u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_RB,
                                 (buttons2 & 0x02u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_BACK,
                                 (buttons2 & 0x10u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_START,
                                 (buttons2 & 0x20u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_L_THUMB,
                                 (buttons2 & 0x40u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_R_THUMB,
                                 (buttons2 & 0x80u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_GUIDE,
                                 (buttons3 & 0x01u) != 0u);
  return true;
}

static bool _maru_windows_process_dualsense_report(
    MARU_Context_Windows *ctx, MARU_Controller_Windows *ctrl,
    const BYTE *report, UINT report_size) {
  size_t base = 0u;
  uint8_t buttons1;
  uint8_t buttons2;
  uint8_t buttons3;

  if (!report || report_size < 11u) {
    return false;
  }

  if (report[0] == 0x01u) {
    if (report_size < 11u) {
      return false;
    }
    base = 0u;
  } else if (report[0] == 0x31u) {
    if (report_size < 13u) {
      return false;
    }
    base = 2u;
  } else {
    return false;
  }

  buttons1 = report[base + 8u];
  buttons2 = report[base + 9u];
  buttons3 = report[base + 10u];

  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_X].value =
      _maru_windows_normalize_u8_centered(report[base + 1u], false);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_Y].value =
      _maru_windows_normalize_u8_centered(report[base + 2u], true);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_X].value =
      _maru_windows_normalize_u8_centered(report[base + 3u], false);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_Y].value =
      _maru_windows_normalize_u8_centered(report[base + 4u], true);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].value =
      _maru_windows_normalize_u8_trigger(report[base + 5u]);
  ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].value =
      _maru_windows_normalize_u8_trigger(report[base + 6u]);

  _maru_windows_update_standard_dpad(ctx, ctrl, buttons1 & 0x0Fu);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_WEST,
                                 (buttons1 & 0x10u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_SOUTH,
                                 (buttons1 & 0x20u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_EAST,
                                 (buttons1 & 0x40u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_NORTH,
                                 (buttons1 & 0x80u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_LB,
                                 (buttons2 & 0x01u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_RB,
                                 (buttons2 & 0x02u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_BACK,
                                 (buttons2 & 0x10u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_START,
                                 (buttons2 & 0x20u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_L_THUMB,
                                 (buttons2 & 0x40u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_R_THUMB,
                                 (buttons2 & 0x80u) != 0u);
  _maru_windows_set_button_state(ctx, ctrl, MARU_CONTROLLER_BUTTON_GUIDE,
                                 (buttons3 & 0x01u) != 0u);
  return true;
}

static void _maru_windows_update_hat_buttons(MARU_Context_Windows *ctx,
                                             MARU_Controller_Windows *ctrl,
                                             LONG hat_value) {
  bool pressed[4] = {false, false, false, false};
  uint32_t i;

  if (hat_value >= ctrl->hid_hat_logical_min &&
      hat_value <= ctrl->hid_hat_logical_max) {
    LONG normalized = hat_value - ctrl->hid_hat_logical_min;
    if (normalized >= 0 && normalized <= 7) {
      pressed[0] = normalized == 0 || normalized == 1 || normalized == 7;
      pressed[1] = normalized == 1 || normalized == 2 || normalized == 3;
      pressed[2] = normalized == 3 || normalized == 4 || normalized == 5;
      pressed[3] = normalized == 5 || normalized == 6 || normalized == 7;
    }
  }

  for (i = 0u; i < 4u; ++i) {
    uint32_t button_id = ctrl->hid_hat_button_base + i;
    MARU_ButtonState8 new_state =
        pressed[i] ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
    if (button_id < ctrl->button_count &&
        ctrl->button_states[button_id] != new_state) {
      ctrl->button_states[button_id] = new_state;
      _maru_windows_emit_controller_button_event(ctx, ctrl, button_id,
                                                 new_state);
    }
  }
}

static void _maru_windows_process_hid_report(MARU_Context_Windows *ctx,
                                             MARU_Controller_Windows *ctrl,
                                             const BYTE *report,
                                             UINT report_size) {
  ULONG pressed_count = ctrl->hid_pressed_usage_capacity;
  ULONG i;

  if (ctrl->hid_kind == MARU_WINDOWS_HID_KIND_DUALSHOCK4) {
    (void)_maru_windows_process_dualshock4_report(ctx, ctrl, report, report_size);
    return;
  }
  if (ctrl->hid_kind == MARU_WINDOWS_HID_KIND_DUALSENSE) {
    (void)_maru_windows_process_dualsense_report(ctx, ctrl, report, report_size);
    return;
  }

  if (ctrl->hid_button_mapping_count > 0u && ctrl->hid_pressed_usages) {
    NTSTATUS status = HidP_GetUsagesEx(
        HidP_Input, 0, ctrl->hid_pressed_usages, &pressed_count,
        ctrl->hid_preparsed_data, (PCHAR)report, report_size);
    if (status != HIDP_STATUS_SUCCESS &&
        status != HIDP_STATUS_BUFFER_TOO_SMALL) {
      pressed_count = 0u;
    }

    for (i = 0u; i < ctrl->hid_button_mapping_count; ++i) {
      MARU_ButtonState8 new_state = _maru_windows_usage_is_pressed(
                                        ctrl->hid_pressed_usages, pressed_count,
                                        ctrl->hid_button_mappings[i].usage_page,
                                        ctrl->hid_button_mappings[i].usage)
                                        ? MARU_BUTTON_STATE_PRESSED
                                        : MARU_BUTTON_STATE_RELEASED;
      if (ctrl->button_states[i] != new_state) {
        ctrl->button_states[i] = new_state;
        _maru_windows_emit_controller_button_event(ctx, ctrl, i, new_state);
      }
    }
  }

  if (ctrl->hid_has_hat_switch) {
    ULONG hat_raw = (ULONG)(ctrl->hid_hat_logical_max + 1);
    if (HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0,
                           ctrl->hid_hat_usage, &hat_raw,
                           ctrl->hid_preparsed_data, (PCHAR)report,
                           report_size) == HIDP_STATUS_SUCCESS) {
      _maru_windows_update_hat_buttons(ctx, ctrl, (LONG)hat_raw);
    } else {
      _maru_windows_update_hat_buttons(ctx, ctrl,
                                       ctrl->hid_hat_logical_max + 1);
    }
  }

  for (i = 0u; i < ctrl->hid_value_mapping_count; ++i) {
    LONG scaled_value = 0;
    ULONG raw_value = 0;
    MARU_Scalar normalized = 0.0f;
    LONG logical_min = ctrl->hid_value_mappings[i].logical_min;
    LONG logical_max = ctrl->hid_value_mappings[i].logical_max;
    bool is_centered = ctrl->hid_value_mappings[i].is_centered;
    NTSTATUS status = HidP_GetScaledUsageValue(
        HidP_Input, ctrl->hid_value_mappings[i].usage_page, 0,
        ctrl->hid_value_mappings[i].usage, &scaled_value,
        ctrl->hid_preparsed_data, (PCHAR)report, report_size);

    if (status != HIDP_STATUS_SUCCESS) {
      if (HidP_GetUsageValue(HidP_Input, ctrl->hid_value_mappings[i].usage_page,
                             0, ctrl->hid_value_mappings[i].usage, &raw_value,
                             ctrl->hid_preparsed_data, (PCHAR)report,
                             report_size) != HIDP_STATUS_SUCCESS) {
        continue;
      }
      scaled_value = (LONG)raw_value;
    }

    if (logical_max > logical_min) {
      if (logical_min < 0 || is_centered) {
        normalized = (MARU_Scalar)((double)(scaled_value - logical_min) /
                                       (double)(logical_max - logical_min) *
                                       2.0 -
                                   1.0);
      } else {
        normalized = (MARU_Scalar)((double)(scaled_value - logical_min) /
                                   (double)(logical_max - logical_min));
      }
    } else {
      normalized = (MARU_Scalar)scaled_value;
    }

    if (is_centered) {
      if (normalized < -1.0f) {
        normalized = -1.0f;
      } else if (normalized > 1.0f) {
        normalized = 1.0f;
      }
    } else {
      if (normalized < 0.0f) {
        normalized = 0.0f;
      } else if (normalized > 1.0f) {
        normalized = 1.0f;
      }
    }

    ctrl->analog_states[i].value = normalized;
  }
}

void _maru_windows_process_raw_input(MARU_Context_Windows *ctx,
                                     const RAWINPUT *raw) {
  MARU_Controller_Windows *ctrl;
  UINT report_index;

  if (!raw || raw->header.dwType != RIM_TYPEHID) {
    return;
  }

  ctrl = _maru_windows_find_raw_controller(ctx, raw->header.hDevice, NULL);
  if (!ctrl || !ctrl->hid_preparsed_data) {
    return;
  }

  for (report_index = 0u; report_index < raw->data.hid.dwCount; ++report_index) {
    const BYTE *report =
        raw->data.hid.bRawData + (report_index * raw->data.hid.dwSizeHid);
    _maru_windows_process_hid_report(ctx, ctrl, report,
                                     raw->data.hid.dwSizeHid);
  }
}

void _maru_windows_update_controllers(MARU_Context_Windows *ctx) {
  static uint64_t last_resync_ms = 0u;
  uint64_t now = GetTickCount64();
  MARU_Controller_Windows *ctrl;

  if (now - last_resync_ms > 2000u) {
    _maru_windows_resync_controllers(ctx);
    last_resync_ms = now;
  }

  for (ctrl = ctx->controller_list_head; ctrl; ctrl = ctrl->next) {
    if (ctrl->wgi_gamepad) {
      IGamepad *gamepad = (IGamepad *)ctrl->wgi_gamepad;
      GamepadReading reading;
      if (SUCCEEDED(gamepad->lpVtbl->GetCurrentReading(gamepad, &reading))) {
        const struct {
          GamepadButtons bit;
          MARU_ControllerButton id;
        } button_map[] = {{GamepadButtons_A, MARU_CONTROLLER_BUTTON_SOUTH},
                          {GamepadButtons_B, MARU_CONTROLLER_BUTTON_EAST},
                          {GamepadButtons_X, MARU_CONTROLLER_BUTTON_WEST},
                          {GamepadButtons_Y, MARU_CONTROLLER_BUTTON_NORTH},
                          {GamepadButtons_LeftShoulder, MARU_CONTROLLER_BUTTON_LB},
                          {GamepadButtons_RightShoulder, MARU_CONTROLLER_BUTTON_RB},
                          {GamepadButtons_Menu, MARU_CONTROLLER_BUTTON_START},
                          {GamepadButtons_View, MARU_CONTROLLER_BUTTON_BACK},
                          {GamepadButtons_LeftThumbstick, MARU_CONTROLLER_BUTTON_L_THUMB},
                          {GamepadButtons_RightThumbstick, MARU_CONTROLLER_BUTTON_R_THUMB},
                          {GamepadButtons_DPadUp, MARU_CONTROLLER_BUTTON_DPAD_UP},
                          {GamepadButtons_DPadRight, MARU_CONTROLLER_BUTTON_DPAD_RIGHT},
                          {GamepadButtons_DPadDown, MARU_CONTROLLER_BUTTON_DPAD_DOWN},
                          {GamepadButtons_DPadLeft, MARU_CONTROLLER_BUTTON_DPAD_LEFT}};
        uint32_t i;
        for (i = 0u; i < sizeof(button_map) / sizeof(button_map[0]); ++i) {
          MARU_ButtonState8 new_state =
              (reading.Buttons & button_map[i].bit) ? MARU_BUTTON_STATE_PRESSED
                                                    : MARU_BUTTON_STATE_RELEASED;
          if (ctrl->button_states[button_map[i].id] != new_state) {
            ctrl->button_states[button_map[i].id] = new_state;
            _maru_windows_emit_controller_button_event(ctx, ctrl,
                                                       button_map[i].id,
                                                       new_state);
          }
        }

        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_X].value =
            (MARU_Scalar)reading.LeftThumbstickX;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_Y].value =
            (MARU_Scalar)reading.LeftThumbstickY;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_X].value =
            (MARU_Scalar)reading.RightThumbstickX;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_Y].value =
            (MARU_Scalar)reading.RightThumbstickY;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].value =
            (MARU_Scalar)reading.LeftTrigger;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].value =
            (MARU_Scalar)reading.RightTrigger;
      }
    } else if (ctrl->xinput_index != 0xFFFFFFFFu && ctx->XInputGetState) {
      XINPUT_STATE state;
      if (ctx->XInputGetState(ctrl->xinput_index, &state) == ERROR_SUCCESS) {
        const struct {
          WORD bit;
          MARU_ControllerButton id;
        } button_map[] = {{XINPUT_GAMEPAD_A, MARU_CONTROLLER_BUTTON_SOUTH},
                          {XINPUT_GAMEPAD_B, MARU_CONTROLLER_BUTTON_EAST},
                          {XINPUT_GAMEPAD_X, MARU_CONTROLLER_BUTTON_WEST},
                          {XINPUT_GAMEPAD_Y, MARU_CONTROLLER_BUTTON_NORTH},
                          {XINPUT_GAMEPAD_LEFT_SHOULDER, MARU_CONTROLLER_BUTTON_LB},
                          {XINPUT_GAMEPAD_RIGHT_SHOULDER, MARU_CONTROLLER_BUTTON_RB},
                          {XINPUT_GAMEPAD_BACK, MARU_CONTROLLER_BUTTON_BACK},
                          {XINPUT_GAMEPAD_START, MARU_CONTROLLER_BUTTON_START},
                          {XINPUT_GAMEPAD_LEFT_THUMB, MARU_CONTROLLER_BUTTON_L_THUMB},
                          {XINPUT_GAMEPAD_RIGHT_THUMB, MARU_CONTROLLER_BUTTON_R_THUMB},
                          {XINPUT_GAMEPAD_DPAD_UP, MARU_CONTROLLER_BUTTON_DPAD_UP},
                          {XINPUT_GAMEPAD_DPAD_RIGHT, MARU_CONTROLLER_BUTTON_DPAD_RIGHT},
                          {XINPUT_GAMEPAD_DPAD_DOWN, MARU_CONTROLLER_BUTTON_DPAD_DOWN},
                          {XINPUT_GAMEPAD_DPAD_LEFT, MARU_CONTROLLER_BUTTON_DPAD_LEFT}};
        uint32_t i;
        WORD buttons = state.Gamepad.wButtons;

        for (i = 0u; i < sizeof(button_map) / sizeof(button_map[0]); ++i) {
          MARU_ButtonState8 new_state =
              (buttons & button_map[i].bit) ? MARU_BUTTON_STATE_PRESSED
                                            : MARU_BUTTON_STATE_RELEASED;
          if (ctrl->button_states[button_map[i].id] != new_state) {
            ctrl->button_states[button_map[i].id] = new_state;
            _maru_windows_emit_controller_button_event(ctx, ctrl,
                                                       button_map[i].id,
                                                       new_state);
          }
        }

#define NORMALIZE_AXIS(val)                                                     \
  ((MARU_Scalar)(val) / ((val) < 0 ? 32768.0f : 32767.0f))
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_X].value =
            NORMALIZE_AXIS(state.Gamepad.sThumbLX);
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_Y].value =
            NORMALIZE_AXIS(state.Gamepad.sThumbLY);
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_X].value =
            NORMALIZE_AXIS(state.Gamepad.sThumbRX);
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_Y].value =
            NORMALIZE_AXIS(state.Gamepad.sThumbRY);
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_LEFT_TRIGGER].value =
            (MARU_Scalar)state.Gamepad.bLeftTrigger / 255.0f;
        ctrl->analog_states[MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER].value =
            (MARU_Scalar)state.Gamepad.bRightTrigger / 255.0f;
#undef NORMALIZE_AXIS
      }
    }
  }
}
