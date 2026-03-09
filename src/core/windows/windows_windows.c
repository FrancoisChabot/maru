// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

static void _maru_windows_dispatch_presentation_event(MARU_Window_Windows *win, uint32_t changed_fields) {
    MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;
    MARU_Event evt = {0};
    evt.presentation.changed_fields = changed_fields;
    evt.presentation.visible = (win->base.pub.flags & MARU_WINDOW_STATE_VISIBLE) != 0;
    evt.presentation.minimized = (win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
    evt.presentation.maximized = (win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
    evt.presentation.focused = (win->base.pub.flags & MARU_WINDOW_STATE_FOCUSED) != 0;
    evt.presentation.icon_effective = (win->base.pub.icon != NULL);
    _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED, (MARU_Window *)win, &evt);
}

static void _maru_windows_set_window_icon(MARU_Window_Windows *win, const MARU_Image *icon) {
  if (win->icon_big) {
    DestroyIcon(win->icon_big);
    win->icon_big = NULL;
  }
  if (win->icon_small) {
    DestroyIcon(win->icon_small);
    win->icon_small = NULL;
  }

  if (icon) {
    // Windows expects the icon to be resized to SM_CXICON/SM_CYICON for the taskbar (big)
    // and SM_CXSMICON/SM_CYSMICON for the title bar (small).
    // For now we just create them from the provided image and let Windows handle scaling if needed,
    // though ideally we would resize them ourselves if we had a resizer.
    
    // Create big icon
    win->icon_big = _maru_windows_create_hicon_from_image(icon, true, 0, 0);
    // Create small icon
    win->icon_small = _maru_windows_create_hicon_from_image(icon, true, 0, 0);

    SendMessageW(win->hwnd, WM_SETICON, ICON_BIG, (LPARAM)win->icon_big);
    SendMessageW(win->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)win->icon_small);
  } else {
    SendMessageW(win->hwnd, WM_SETICON, ICON_BIG, (LPARAM)NULL);
    SendMessageW(win->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)NULL);
  }
}

static void _maru_update_cursor_mode_windows(MARU_Window_Windows *win) {
  if (win->cursor_mode == MARU_CURSOR_LOCKED) {
    RECT rect;
    GetClientRect(win->hwnd, &rect);
    ClientToScreen(win->hwnd, (POINT *)&rect.left);
    ClientToScreen(win->hwnd, (POINT *)&rect.right);
    ClipCursor(&rect);
  } else {
    ClipCursor(NULL);
  }

  if (win->cursor_mode != MARU_CURSOR_NORMAL && win->is_cursor_in_client_area) {
    while (ShowCursor(FALSE) >= 0);
  } else {
    while (ShowCursor(TRUE) < 0);
  }
}

static void _maru_register_rawinput_window(HWND hwnd) {
  RAWINPUTDEVICE devices[3];

  memset(devices, 0, sizeof(devices));
  devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
  devices[0].usUsage = HID_USAGE_GENERIC_JOYSTICK;
  devices[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
  devices[0].hwndTarget = hwnd;

  devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
  devices[1].usUsage = HID_USAGE_GENERIC_GAMEPAD;
  devices[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
  devices[1].hwndTarget = hwnd;

  devices[2].usUsagePage = HID_USAGE_PAGE_GENERIC;
  devices[2].usUsage = HID_USAGE_GENERIC_MULTI_AXIS_CONTROLLER;
  devices[2].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
  devices[2].hwndTarget = hwnd;

  RegisterRawInputDevices(devices, 3u, sizeof(RAWINPUTDEVICE));
}

LRESULT CALLBACK _maru_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (!win) {
    if (uMsg == WM_CREATE) {
        LPCREATESTRUCTW cs = (LPCREATESTRUCTW)lParam;
        win = (MARU_Window_Windows *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
        win->hwnd = hwnd;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
  }

  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  switch (uMsg) {
    case WM_INPUT_DEVICE_CHANGE: {
      _maru_windows_resync_controllers(ctx);
      break;
    }
    case WM_DEVICECHANGE: {
      _maru_windows_resync_controllers(ctx);
      break;
    }
    case WM_INPUT: {
      UINT raw_size = 0u;
      RAWINPUT *raw = NULL;
      if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &raw_size,
                          sizeof(RAWINPUTHEADER)) == 0u &&
          raw_size >= sizeof(RAWINPUT)) {
        raw = (RAWINPUT *)maru_context_alloc(&ctx->base, raw_size);
        if (raw &&
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw, &raw_size,
                            sizeof(RAWINPUTHEADER)) == raw_size) {
          _maru_windows_process_raw_input(ctx, raw);
        }
        maru_context_free(&ctx->base, raw);
      }
      return 0;
    }
    case WM_SETCURSOR: {
      if (LOWORD(lParam) == HTCLIENT) {
        if (win->cursor_mode != MARU_CURSOR_NORMAL) {
          SetCursor(NULL);
          return TRUE;
        } else if (win->base.pub.current_cursor) {
          SetCursor(((MARU_Cursor_Windows *)win->base.pub.current_cursor)->hcursor);
          return TRUE;
        }
      }
      break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      MARU_Key key = _maru_translate_key_windows(wParam, lParam);
      if (key != MARU_KEY_UNKNOWN) {
        MARU_ButtonState state = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) 
                                 ? MARU_BUTTON_STATE_PRESSED 
                                 : MARU_BUTTON_STATE_RELEASED;
        
        // Update internal state
        win->base.keyboard_state[key] = (MARU_ButtonState8)state;

        // Skip repeats for state changed events
        if (state == MARU_BUTTON_STATE_PRESSED && (lParam & (1 << 30))) {
          return 0;
        }

        MARU_Event evt = {0};
        evt.key.raw_key = key;
        evt.key.state = state;
        evt.key.modifiers = _maru_get_modifiers_windows();
        _maru_dispatch_event(&ctx->base, MARU_EVENT_KEY_STATE_CHANGED, (MARU_Window *)win, &evt);
      }
      return 0;
    }

    case WM_CHAR:
    case WM_SYSCHAR: {
      wchar_t codepoint = (wchar_t)wParam;
      if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
        win->high_surrogate = codepoint;
        return 0;
      }
      
      wchar_t utf16[3] = {0};
      int utf16_len = 0;
      if (win->high_surrogate && codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
        utf16[0] = win->high_surrogate;
        utf16[1] = codepoint;
        utf16_len = 2;
      } else {
        utf16[0] = codepoint;
        utf16_len = 1;
      }
      win->high_surrogate = 0;

      char utf8[8];
      int n = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, utf8, sizeof(utf8), NULL, NULL);
      if (n > 0) {
        MARU_Event evt = {0};
        evt.text_edit_commit.committed_utf8 = utf8;
        evt.text_edit_commit.committed_length = (uint32_t)n;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)win, &evt);
      }
      return 0;
    }

    case WM_UNICHAR: {
      if (wParam == UNICODE_NOCHAR) {
        return TRUE;
      }

      uint32_t codepoint = (uint32_t)wParam;
      wchar_t utf16[3] = {0};
      int utf16_len = 0;

      if (codepoint > 0xFFFF) {
        utf16[0] = (wchar_t)(0xD800 + ((codepoint - 0x10000) >> 10));
        utf16[1] = (wchar_t)(0xDC00 + ((codepoint - 0x10000) & 0x3FF));
        utf16_len = 2;
      } else {
        utf16[0] = (wchar_t)codepoint;
        utf16_len = 1;
      }

      char utf8[8];
      int n = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, utf8, sizeof(utf8), NULL, NULL);
      if (n > 0) {
        MARU_Event evt = {0};
        evt.text_edit_commit.committed_utf8 = utf8;
        evt.text_edit_commit.committed_length = (uint32_t)n;
        _maru_dispatch_event(&ctx->base, MARU_EVENT_TEXT_EDIT_COMMIT, (MARU_Window *)win, &evt);
      }
      return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP: {
      uint32_t button_id = 0;
      MARU_ButtonState state = MARU_BUTTON_STATE_RELEASED;

      switch (uMsg) {
        case WM_LBUTTONDOWN: state = MARU_BUTTON_STATE_PRESSED; // fallthrough
        case WM_LBUTTONUP:   button_id = 0; break;
        case WM_RBUTTONDOWN: state = MARU_BUTTON_STATE_PRESSED; // fallthrough
        case WM_RBUTTONUP:   button_id = 1; break;
        case WM_MBUTTONDOWN: state = MARU_BUTTON_STATE_PRESSED; // fallthrough
        case WM_MBUTTONUP:   button_id = 2; break;
        case WM_XBUTTONDOWN: state = MARU_BUTTON_STATE_PRESSED; // fallthrough
        case WM_XBUTTONUP:   button_id = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; break;
      }

      if (ctx->base.mouse_button_states) {
        ctx->base.mouse_button_states[button_id] = (MARU_ButtonState8)state;
      }

      MARU_Event evt = {0};
      evt.mouse_button.button_id = button_id;
      evt.mouse_button.state = state;
      evt.mouse_button.modifiers = _maru_get_modifiers_windows();
      _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)win, &evt);

      if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN || uMsg == WM_XBUTTONDOWN) {
          SetCapture(hwnd);
      } else {
          ReleaseCapture();
      }

      return 0;
    }

    case WM_MOUSEMOVE: {
      MARU_Vec2Dip pos;
      pos.x = (MARU_Scalar)(short)LOWORD(lParam);
      pos.y = (MARU_Scalar)(short)HIWORD(lParam);

      if (!win->is_cursor_in_client_area) {
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        win->is_cursor_in_client_area = true;
        _maru_update_cursor_mode_windows(win);
      }

      MARU_Event evt = {0};

      if (win->cursor_mode == MARU_CURSOR_LOCKED) {
        RECT rect;
        GetClientRect(win->hwnd, &rect);
        MARU_Scalar centerX = (MARU_Scalar)(rect.right - rect.left) / 2.0f;
        MARU_Scalar centerY = (MARU_Scalar)(rect.bottom - rect.top) / 2.0f;

        evt.mouse_motion.delta.x = pos.x - centerX;
        evt.mouse_motion.delta.y = pos.y - centerY;

        if (evt.mouse_motion.delta.x != 0 || evt.mouse_motion.delta.y != 0) {
          win->virtual_cursor_pos.x += evt.mouse_motion.delta.x;
          win->virtual_cursor_pos.y += evt.mouse_motion.delta.y;

          POINT pt = {(int)centerX, (int)centerY};
          ClientToScreen(win->hwnd, &pt);
          SetCursorPos(pt.x, pt.y);
        }
        evt.mouse_motion.position = win->virtual_cursor_pos;
      } else {
        evt.mouse_motion.position = pos;
        if (win->last_mouse_pos_valid) {
          evt.mouse_motion.delta.x = pos.x - win->last_mouse_pos.x;
          evt.mouse_motion.delta.y = pos.y - win->last_mouse_pos.y;
        } else {
          evt.mouse_motion.delta.x = 0;
          evt.mouse_motion.delta.y = 0;
        }
        win->last_mouse_pos = pos;
        win->last_mouse_pos_valid = true;
      }

      evt.mouse_motion.raw_delta = evt.mouse_motion.delta; // TODO: handle raw input
      evt.mouse_motion.modifiers = _maru_get_modifiers_windows();

      _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_MOVED, (MARU_Window *)win, &evt);
      return 0;
    }

    case WM_MOUSELEAVE: {
        win->last_mouse_pos_valid = false;
        win->is_cursor_in_client_area = false;
        _maru_update_cursor_mode_windows(win);
        return 0;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
      MARU_Event evt = {0};
      short delta = GET_WHEEL_DELTA_WPARAM(wParam);
      MARU_Scalar value = (MARU_Scalar)delta / (MARU_Scalar)WHEEL_DELTA;

      if (uMsg == WM_MOUSEWHEEL) {
        evt.mouse_scroll.steps.y = (int32_t)value;
        evt.mouse_scroll.delta.y = value * 10.0f; // TODO: use system settings
      } else {
        evt.mouse_scroll.steps.x = (int32_t)value;
        evt.mouse_scroll.delta.x = value * 10.0f; // TODO: use system settings
      }
      evt.mouse_scroll.modifiers = _maru_get_modifiers_windows();

      _maru_dispatch_event(&ctx->base, MARU_EVENT_MOUSE_SCROLLED, (MARU_Window *)win, &evt);
      return 0;
    }

    case WM_SIZE: {
      if (win->cursor_mode == MARU_CURSOR_LOCKED && GetActiveWindow() == win->hwnd) {
        _maru_update_cursor_mode_windows(win);
      }

      uint32_t changed_fields = 0;
      if (wParam == SIZE_MAXIMIZED) {
          if (!(win->base.pub.flags & MARU_WINDOW_STATE_MAXIMIZED)) {
              win->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
              win->base.pub.flags &= ~MARU_WINDOW_STATE_MINIMIZED;
              changed_fields = MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED | MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED;
          }
      } else if (wParam == SIZE_MINIMIZED) {
          if (!(win->base.pub.flags & MARU_WINDOW_STATE_MINIMIZED)) {
              win->base.pub.flags |= MARU_WINDOW_STATE_MINIMIZED;
              win->base.pub.flags &= ~MARU_WINDOW_STATE_MAXIMIZED;
              changed_fields = MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED | MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED;
          }
      } else if (wParam == SIZE_RESTORED) {
          if ((win->base.pub.flags & (MARU_WINDOW_STATE_MAXIMIZED | MARU_WINDOW_STATE_MINIMIZED))) {
              win->base.pub.flags &= ~(MARU_WINDOW_STATE_MAXIMIZED | MARU_WINDOW_STATE_MINIMIZED);
              changed_fields = MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED | MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED;
          }
      }

      if (changed_fields) {
          _maru_windows_dispatch_presentation_event(win, changed_fields);
      }

      MARU_Event evt = {0};
      maru_getWindowGeometry_Windows((MARU_Window *)win, &evt.resized.geometry);
      _maru_dispatch_event(&ctx->base, MARU_EVENT_WINDOW_RESIZED, (MARU_Window *)win, &evt);
      return 0;
    }

    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        bool focused = (uMsg == WM_SETFOCUS);
        if (focused) {
            win->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
        } else {
            win->base.pub.flags &= ~MARU_WINDOW_STATE_FOCUSED;
            win->high_surrogate = 0;
        }
        _maru_update_cursor_mode_windows(win);

        _maru_windows_dispatch_presentation_event(win, MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED);
        return 0;
    }

    case WM_CLOSE: {
      MARU_Event evt = {0};
      _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)win, &evt);
      return 0;
    }

    case WM_DESTROY: {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      win->hwnd = NULL;
      return 0;
    }
    
    case WM_PAINT: {
        ValidateRect(hwnd, NULL);
        return 0;
    }

    case WM_ERASEBKGND:
      return 1;
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  MARU_Window_Windows *win = (MARU_Window_Windows *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_Windows));
  if (!win) {
    return MARU_FAILURE;
  }
  memset(win, 0, sizeof(MARU_Window_Windows));

  win->base.ctx_base = &ctx->base;
  win->base.attrs_requested = create_info->attributes;
  win->base.attrs_effective = create_info->attributes;
  win->base.pending_ready_event = true;

  // Initialize public-facing (exposed) part of the window
  win->base.pub.context = context;
  win->base.pub.userdata = create_info->userdata;
  win->base.pub.metrics = &win->base.metrics;
  win->base.pub.keyboard_state = win->base.keyboard_state;
  win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  win->base.pub.mouse_button_state = ctx->base.pub.mouse_button_state;
  win->base.pub.mouse_button_channels = ctx->base.pub.mouse_button_channels;
  win->base.pub.mouse_button_count = ctx->base.pub.mouse_button_count;
  memcpy(win->base.pub.mouse_default_button_channels,
         ctx->base.pub.mouse_default_button_channels,
         sizeof(win->base.pub.mouse_default_button_channels));
  win->base.pub.event_mask = create_info->attributes.event_mask;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = (MARU_Cursor *)create_info->attributes.cursor;
  win->base.pub.title = NULL; // Updated when title is changed or requested

  if (create_info->decorated) {
      win->base.pub.flags |= MARU_WINDOW_STATE_DECORATED;
  }
  if (create_info->attributes.resizable) {
      win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
  }

  wchar_t title_w[256];
  if (create_info->attributes.title && create_info->attributes.title[0] != '\0') {
    MultiByteToWideChar(CP_UTF8, 0, create_info->attributes.title, -1, title_w, 256);
  } else {
    wcscpy(title_w, L"Maru Window");
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  if (!create_info->decorated) {
      style = WS_POPUP;
  } else if (!create_info->attributes.resizable) {
      style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }

  DWORD ex_style = WS_EX_APPWINDOW;

  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int w = (int)create_info->attributes.logical_size.x;
  int h = (int)create_info->attributes.logical_size.y;

  if (w == 0 || h == 0) {
      w = 800;
      h = 600;
  }

  if (create_info->attributes.position.x != 0 || create_info->attributes.position.y != 0) {
      x = (int)create_info->attributes.position.x;
      y = (int)create_info->attributes.position.y;
      
      RECT rect = {x, y, x + w, y + h};
      AdjustWindowRectEx(&rect, style, FALSE, ex_style);
      x = rect.left;
      y = rect.top;
      w = rect.right - rect.left;
      h = rect.bottom - rect.top;
  } else {
      RECT rect = {0, 0, w, h};
      AdjustWindowRectEx(&rect, style, FALSE, ex_style);
      w = rect.right - rect.left;
      h = rect.bottom - rect.top;
  }

  win->hwnd = CreateWindowExW(
      ex_style, L"MARU_WindowClass", title_w, style,
      x, y, w, h,
      NULL, NULL, ctx->instance, win);

  if (!win->hwnd) {
    maru_context_free(&ctx->base, win);
    return MARU_FAILURE;
  }

  win->hdc = GetDC(win->hwnd);
  _maru_register_rawinput_window(win->hwnd);
  _maru_windows_resync_controllers(ctx);
  
  if (create_info->attributes.icon) {
      _maru_windows_set_window_icon(win, create_info->attributes.icon);
      win->base.pub.icon = create_info->attributes.icon;
  }

  // Initial attribute application
  uint64_t initial_mask = MARU_WINDOW_ATTR_CURSOR_MODE | 
                          MARU_WINDOW_ATTR_CURSOR | 
                          MARU_WINDOW_ATTR_RESIZABLE | 
                          MARU_WINDOW_ATTR_MAXIMIZED | 
                          MARU_WINDOW_ATTR_MINIMIZED | 
                          MARU_WINDOW_ATTR_FULLSCREEN;
  maru_updateWindow_Windows((MARU_Window *)win, initial_mask, &create_info->attributes);

  _maru_register_window(&ctx->base, (MARU_Window *)win);

  // If visible was requested, we'll show it, but we delay it
  // until the first event pump to avoid the white flash during initialization.
  if (create_info->attributes.visible) {
    win->show_on_first_pump = true;
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
  }

  *out_window = (MARU_Window *)win;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  if (win->hwnd) {
    _maru_windows_set_window_icon(win, NULL); // This will destroy the icons
    ReleaseDC(win->hwnd, win->hdc);
    DestroyWindow(win->hwnd);
  }

  _maru_unregister_window(&ctx->base, window);
  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

void maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window_handle;
  if (!win->hwnd || !out_geometry) return;

  RECT rect;
  GetClientRect(win->hwnd, &rect);
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;

  out_geometry->pixel_size.x = (int32_t)w;
  out_geometry->pixel_size.y = (int32_t)h;
  
  // For now, scale is 1.0. 
  // TODO: Add DPI support
  out_geometry->scale = (MARU_Scalar)1.0;
  
  out_geometry->logical_size.x = (MARU_Scalar)w;
  out_geometry->logical_size.y = (MARU_Scalar)h;

  POINT pt = {0, 0};
  ClientToScreen(win->hwnd, &pt);
  out_geometry->origin.x = (MARU_Scalar)pt.x;
  out_geometry->origin.y = (MARU_Scalar)pt.y;
  
  out_geometry->buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
}

MARU_Status maru_updateWindow_Windows(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  
  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    if (attributes->title) {
      wchar_t title_w[256];
      MultiByteToWideChar(CP_UTF8, 0, attributes->title, -1, title_w, 256);
      SetWindowTextW(win->hwnd, title_w);
      win->base.pub.title = attributes->title; // Note: this is a shallow copy, user must keep it alive or we need storage
      win->base.attrs_effective.title = attributes->title;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    if (attributes->visible) {
      ShowWindow(win->hwnd, SW_SHOW);
      win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    } else {
      ShowWindow(win->hwnd, SW_HIDE);
      win->base.pub.flags &= ~MARU_WINDOW_STATE_VISIBLE;
    }
    win->base.attrs_effective.visible = attributes->visible;
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    if (win->cursor_mode != attributes->cursor_mode) {
      win->cursor_mode = attributes->cursor_mode;
      win->base.pub.cursor_mode = attributes->cursor_mode;
      _maru_update_cursor_mode_windows(win);
      win->base.attrs_effective.cursor_mode = attributes->cursor_mode;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    win->base.pub.current_cursor = (MARU_Cursor *)attributes->cursor;
    win->base.attrs_effective.cursor = attributes->cursor;
    
    // Trigger cursor update if we are in normal mode and the cursor is over the client area.
    if (win->cursor_mode == MARU_CURSOR_NORMAL && win->is_cursor_in_client_area) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(win->hwnd, &pt);
        RECT client_rect;
        GetClientRect(win->hwnd, &client_rect);
        if (PtInRect(&client_rect, pt)) {
            // Send a dummy WM_SETCURSOR to trigger the change
            PostMessageW(win->hwnd, WM_SETCURSOR, (WPARAM)win->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        }
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_ICON) {
      _maru_windows_set_window_icon(win, attributes->icon);
      win->base.attrs_effective.icon = attributes->icon;
      win->base.pub.icon = attributes->icon;
  }

  if (field_mask & (MARU_WINDOW_ATTR_LOGICAL_SIZE | MARU_WINDOW_ATTR_POSITION)) {
      MARU_Scalar new_w = (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) ? attributes->logical_size.x : win->base.attrs_effective.logical_size.x;
      MARU_Scalar new_h = (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) ? attributes->logical_size.y : win->base.attrs_effective.logical_size.y;
      
      MARU_Scalar new_x, new_y;
      if (field_mask & MARU_WINDOW_ATTR_POSITION) {
          new_x = attributes->position.x;
          new_y = attributes->position.y;
      } else {
          MARU_WindowGeometry geo;
          maru_getWindowGeometry_Windows(window, &geo);
          new_x = geo.origin.x;
          new_y = geo.origin.y;
      }

      DWORD style = GetWindowLongW(win->hwnd, GWL_STYLE);
      DWORD ex_style = GetWindowLongW(win->hwnd, GWL_EXSTYLE);
      RECT rect = {(int)new_x, (int)new_y, (int)new_x + (int)new_w, (int)new_y + (int)new_h};
      AdjustWindowRectEx(&rect, style, FALSE, ex_style);
      
      if (!(field_mask & MARU_WINDOW_ATTR_POSITION) && (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE)) {
          SetWindowPos(win->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
      } else if ((field_mask & MARU_WINDOW_ATTR_POSITION) && !(field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE)) {
          SetWindowPos(win->hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
      } else {
          SetWindowPos(win->hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
      }
      
      if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
          win->base.attrs_effective.logical_size = attributes->logical_size;
      }
      if (field_mask & MARU_WINDOW_ATTR_POSITION) {
          win->base.attrs_effective.position = attributes->position;
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_RESIZABLE) {
      DWORD style = GetWindowLongW(win->hwnd, GWL_STYLE);
      if (attributes->resizable) {
          style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
      } else {
          style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
      }
      SetWindowLongW(win->hwnd, GWL_STYLE, style);
      SetWindowPos(win->hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
      win->base.attrs_effective.resizable = attributes->resizable;
      if (attributes->resizable) {
          win->base.pub.flags |= MARU_WINDOW_STATE_RESIZABLE;
      } else {
          win->base.pub.flags &= ~MARU_WINDOW_STATE_RESIZABLE;
      }
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
      if (attributes->fullscreen && !win->is_fullscreen) {
          // Enter fullscreen
          win->saved_style = GetWindowLongW(win->hwnd, GWL_STYLE);
          win->saved_ex_style = GetWindowLongW(win->hwnd, GWL_EXSTYLE);
          GetWindowRect(win->hwnd, &win->saved_rect);

          MONITORINFO mi = { sizeof(mi) };
          if (GetMonitorInfoW(MonitorFromWindow(win->hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
              SetWindowLongW(win->hwnd, GWL_STYLE, win->saved_style & ~WS_OVERLAPPEDWINDOW);
              SetWindowPos(win->hwnd, HWND_TOP,
                           mi.rcMonitor.left, mi.rcMonitor.top,
                           mi.rcMonitor.right - mi.rcMonitor.left,
                           mi.rcMonitor.bottom - mi.rcMonitor.top,
                           SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
              win->is_fullscreen = true;
          }
      } else if (!attributes->fullscreen && win->is_fullscreen) {
          // Exit fullscreen
          SetWindowLongW(win->hwnd, GWL_STYLE, win->saved_style);
          SetWindowLongW(win->hwnd, GWL_EXSTYLE, win->saved_ex_style);
          SetWindowPos(win->hwnd, NULL,
                       win->saved_rect.left, win->saved_rect.top,
                       win->saved_rect.right - win->saved_rect.left,
                       win->saved_rect.bottom - win->saved_rect.top,
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
          win->is_fullscreen = false;
      }
      win->base.attrs_effective.fullscreen = attributes->fullscreen;
      if (win->is_fullscreen) {
          win->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
      } else {
          win->base.pub.flags &= ~MARU_WINDOW_STATE_FULLSCREEN;
      }
      _maru_windows_dispatch_presentation_event(win, MARU_WINDOW_PRESENTATION_CHANGED_VISIBLE);
  }

  if (!win->is_fullscreen) {
      if (field_mask & MARU_WINDOW_ATTR_MAXIMIZED) {
          if (attributes->maximized) {
              ShowWindow(win->hwnd, SW_MAXIMIZE);
          } else {
              ShowWindow(win->hwnd, SW_RESTORE);
          }
          win->base.attrs_effective.maximized = attributes->maximized;
      }

      if (field_mask & MARU_WINDOW_ATTR_MINIMIZED) {
          if (attributes->minimized) {
              ShowWindow(win->hwnd, SW_MINIMIZE);
          } else {
              ShowWindow(win->hwnd, SW_RESTORE);
          }
          win->base.attrs_effective.minimized = attributes->minimized;
      }
  }

  // TODO: Implement other attributes (resize, etc)

  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  if (win->hwnd) {
    SetForegroundWindow(win->hwnd);
    SetFocus(win->hwnd);
  }
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_Windows(MARU_Window *window) {
  (void)window;
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  if (win->hwnd) {
    FlashWindow(win->hwnd, TRUE);
  }
  return MARU_SUCCESS;
}

void *_maru_getWindowNativeHandle_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  return (void *)win->hwnd;
}
