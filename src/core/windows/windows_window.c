// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

static const char* _maru_win32_window_class = "MaruWindowClass";

static MARU_Key _maru_win32_map_key(WPARAM wparam) {
  switch (wparam) {
    case VK_SPACE: return MARU_KEY_SPACE;
    case VK_OEM_7: return MARU_KEY_APOSTROPHE;
    case VK_OEM_COMMA: return MARU_KEY_COMMA;
    case VK_OEM_MINUS: return MARU_KEY_MINUS;
    case VK_OEM_PERIOD: return MARU_KEY_PERIOD;
    case VK_OEM_2: return MARU_KEY_SLASH;
    case '0': return MARU_KEY_0;
    case '1': return MARU_KEY_1;
    case '2': return MARU_KEY_2;
    case '3': return MARU_KEY_3;
    case '4': return MARU_KEY_4;
    case '5': return MARU_KEY_5;
    case '6': return MARU_KEY_6;
    case '7': return MARU_KEY_7;
    case '8': return MARU_KEY_8;
    case '9': return MARU_KEY_9;
    case VK_OEM_1: return MARU_KEY_SEMICOLON;
    case VK_OEM_PLUS: return MARU_KEY_EQUAL;
    case 'A': return MARU_KEY_A;
    case 'B': return MARU_KEY_B;
    case 'C': return MARU_KEY_C;
    case 'D': return MARU_KEY_D;
    case 'E': return MARU_KEY_E;
    case 'F': return MARU_KEY_F;
    case 'G': return MARU_KEY_G;
    case 'H': return MARU_KEY_H;
    case 'I': return MARU_KEY_I;
    case 'J': return MARU_KEY_J;
    case 'K': return MARU_KEY_K;
    case 'L': return MARU_KEY_L;
    case 'M': return MARU_KEY_M;
    case 'N': return MARU_KEY_N;
    case 'O': return MARU_KEY_O;
    case 'P': return MARU_KEY_P;
    case 'Q': return MARU_KEY_Q;
    case 'R': return MARU_KEY_R;
    case 'S': return MARU_KEY_S;
    case 'T': return MARU_KEY_T;
    case 'U': return MARU_KEY_U;
    case 'V': return MARU_KEY_V;
    case 'W': return MARU_KEY_W;
    case 'X': return MARU_KEY_X;
    case 'Y': return MARU_KEY_Y;
    case 'Z': return MARU_KEY_Z;
    case VK_OEM_4: return MARU_KEY_LEFT_BRACKET;
    case VK_OEM_5: return MARU_KEY_BACKSLASH;
    case VK_OEM_6: return MARU_KEY_RIGHT_BRACKET;
    case VK_OEM_3: return MARU_KEY_GRAVE_ACCENT;
    case VK_ESCAPE: return MARU_KEY_ESCAPE;
    case VK_RETURN: return MARU_KEY_ENTER;
    case VK_TAB: return MARU_KEY_TAB;
    case VK_BACK: return MARU_KEY_BACKSPACE;
    case VK_INSERT: return MARU_KEY_INSERT;
    case VK_DELETE: return MARU_KEY_DELETE;
    case VK_RIGHT: return MARU_KEY_RIGHT;
    case VK_LEFT: return MARU_KEY_LEFT;
    case VK_DOWN: return MARU_KEY_DOWN;
    case VK_UP: return MARU_KEY_UP;
    case VK_PRIOR: return MARU_KEY_PAGE_UP;
    case VK_NEXT: return MARU_KEY_PAGE_DOWN;
    case VK_HOME: return MARU_KEY_HOME;
    case VK_END: return MARU_KEY_END;
    case VK_CAPITAL: return MARU_KEY_CAPS_LOCK;
    case VK_SCROLL: return MARU_KEY_SCROLL_LOCK;
    case VK_NUMLOCK: return MARU_KEY_NUM_LOCK;
    case VK_SNAPSHOT: return MARU_KEY_PRINT_SCREEN;
    case VK_PAUSE: return MARU_KEY_PAUSE;
    case VK_F1: return MARU_KEY_F1;
    case VK_F2: return MARU_KEY_F2;
    case VK_F3: return MARU_KEY_F3;
    case VK_F4: return MARU_KEY_F4;
    case VK_F5: return MARU_KEY_F5;
    case VK_F6: return MARU_KEY_F6;
    case VK_F7: return MARU_KEY_F7;
    case VK_F8: return MARU_KEY_F8;
    case VK_F9: return MARU_KEY_F9;
    case VK_F10: return MARU_KEY_F10;
    case VK_F11: return MARU_KEY_F11;
    case VK_F12: return MARU_KEY_F12;
    case VK_NUMPAD0: return MARU_KEY_KP_0;
    case VK_NUMPAD1: return MARU_KEY_KP_1;
    case VK_NUMPAD2: return MARU_KEY_KP_2;
    case VK_NUMPAD3: return MARU_KEY_KP_3;
    case VK_NUMPAD4: return MARU_KEY_KP_4;
    case VK_NUMPAD5: return MARU_KEY_KP_5;
    case VK_NUMPAD6: return MARU_KEY_KP_6;
    case VK_NUMPAD7: return MARU_KEY_KP_7;
    case VK_NUMPAD8: return MARU_KEY_KP_8;
    case VK_NUMPAD9: return MARU_KEY_KP_9;
    case VK_DECIMAL: return MARU_KEY_KP_DECIMAL;
    case VK_DIVIDE: return MARU_KEY_KP_DIVIDE;
    case VK_MULTIPLY: return MARU_KEY_KP_MULTIPLY;
    case VK_SUBTRACT: return MARU_KEY_KP_SUBTRACT;
    case VK_ADD: return MARU_KEY_KP_ADD;
    case VK_LSHIFT: return MARU_KEY_LEFT_SHIFT;
    case VK_LCONTROL: return MARU_KEY_LEFT_CONTROL;
    case VK_LMENU: return MARU_KEY_LEFT_ALT;
    case VK_LWIN: return MARU_KEY_LEFT_META;
    case VK_RSHIFT: return MARU_KEY_RIGHT_SHIFT;
    case VK_RCONTROL: return MARU_KEY_RIGHT_CONTROL;
    case VK_RMENU: return MARU_KEY_RIGHT_ALT;
    case VK_RWIN: return MARU_KEY_RIGHT_META;
    case VK_APPS: return MARU_KEY_MENU;
    default: return MARU_KEY_UNKNOWN;
  }
}

static MARU_ModifierFlags _maru_win32_get_modifiers(void) {
  MARU_ModifierFlags mods = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MARU_MODIFIER_SHIFT;
  if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MARU_MODIFIER_CONTROL;
  if (GetKeyState(VK_MENU) & 0x8000) mods |= MARU_MODIFIER_ALT;
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MARU_MODIFIER_META;
  if (GetKeyState(VK_CAPITAL) & 1) mods |= MARU_MODIFIER_CAPS_LOCK;
  if (GetKeyState(VK_NUMLOCK) & 1) mods |= MARU_MODIFIER_NUM_LOCK;
  return mods;
}

static LRESULT CALLBACK _maru_win32_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

  if (window) {
    MARU_Context_Base *ctx = window->base.ctx_base;

    switch (msg) {
    case WM_CLOSE: {
      if (ctx->event_cb && (ctx->event_mask & MARU_CLOSE_REQUESTED)) {
        MARU_Event evt = { {0} };
        ctx->event_cb(MARU_CLOSE_REQUESTED, (MARU_Window *)window, &evt);
      }
      return 0;
    }

    case WM_SIZE: {
      int width = LOWORD(lparam);
      int height = HIWORD(lparam);
      window->size.x = (MARU_Scalar)width;
      window->size.y = (MARU_Scalar)height;

      if (wparam == SIZE_MAXIMIZED) {
        window->base.pub.flags |= MARU_WINDOW_STATE_MAXIMIZED;
      } else if (wparam == SIZE_RESTORED) {
        window->base.pub.flags &= ~MARU_WINDOW_STATE_MAXIMIZED;
      }

      MARU_Event evt = {
        .resized = {
          .geometry = {
            .pixel_size = { width, height },
            .logical_size = { (MARU_Scalar)width, (MARU_Scalar)height }
          }
        }
      };
      _maru_dispatch_event(ctx, MARU_WINDOW_RESIZED, (MARU_Window *)window, &evt);

      if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
        MARU_Event max_evt = {
          .maximized = { .maximized = (wparam == SIZE_MAXIMIZED) }
        };
        _maru_dispatch_event(ctx, MARU_WINDOW_MAXIMIZED, (MARU_Window *)window, &max_evt);
      }
      return 0;
    }

    case WM_MOUSEMOVE: {
      MARU_Event evt = {
        .mouse_motion = {
          .position = { (MARU_Scalar)GET_X_LPARAM(lparam), (MARU_Scalar)GET_Y_LPARAM(lparam) }
        }
      };
      _maru_dispatch_event(ctx, MARU_MOUSE_MOVED, (MARU_Window *)window, &evt);
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
      MARU_MouseButton button;
      if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) button = MARU_MOUSE_BUTTON_LEFT;
      else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) button = MARU_MOUSE_BUTTON_RIGHT;
      else if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP) button = MARU_MOUSE_BUTTON_MIDDLE;
      else if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) button = MARU_MOUSE_BUTTON_4;
      else button = MARU_MOUSE_BUTTON_5;

      MARU_ButtonState state = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN) 
        ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;

      MARU_Event evt = {
        .mouse_button = {
          .button = button,
          .state = state,
          .modifiers = _maru_win32_get_modifiers()
        }
      };
      _maru_dispatch_event(ctx, MARU_MOUSE_BUTTON_STATE_CHANGED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_MOUSEWHEEL: {
      MARU_Event evt = {
        .mouse_scroll = {
          .delta = { 0, (MARU_Scalar)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA },
          .steps = { 0, GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA }
        }
      };
      _maru_dispatch_event(ctx, MARU_MOUSE_SCROLLED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_MOUSEHWHEEL: {
      MARU_Event evt = {
        .mouse_scroll = {
          .delta = { (MARU_Scalar)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA, 0 },
          .steps = { GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA, 0 }
        }
      };
      _maru_dispatch_event(ctx, MARU_MOUSE_SCROLLED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      MARU_Key key = _maru_win32_map_key(wparam);
      MARU_ButtonState state = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? MARU_BUTTON_STATE_PRESSED : MARU_BUTTON_STATE_RELEASED;
      
      if (key != MARU_KEY_UNKNOWN) {
        window->base.keyboard_state[key] = (MARU_ButtonState8)state;
      }

      MARU_Event evt = {
        .key = {
          .raw_key = key,
          .state = state,
          .modifiers = _maru_win32_get_modifiers()
        }
      };
      _maru_dispatch_event(ctx, MARU_KEY_STATE_CHANGED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_SETFOCUS: {
      window->base.pub.flags |= MARU_WINDOW_STATE_FOCUSED;
      MARU_Event evt = {
        .focus = { .focused = true }
      };
      _maru_dispatch_event(ctx, MARU_FOCUS_CHANGED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_KILLFOCUS: {
      window->base.pub.flags &= ~MARU_WINDOW_STATE_FOCUSED;
      MARU_Event evt = {
        .focus = { .focused = false }
      };
      _maru_dispatch_event(ctx, MARU_FOCUS_CHANGED, (MARU_Window *)window, &evt);
      return 0;
    }

    case WM_SETCURSOR: {
      if (LOWORD(lparam) == HTCLIENT) {
        if (window->cursor_mode == MARU_CURSOR_HIDDEN || window->cursor_mode == MARU_CURSOR_LOCKED) {
          SetCursor(NULL);
        } else if (window->current_cursor) {
          SetCursor(window->current_cursor->hcursor);
        } else {
          SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
        return TRUE;
      }
      break;
    }

    case WM_CHAR: {
      char utf8[4];
      int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&wparam, 1, utf8, 4, NULL, NULL);
      if (len > 0) {
        MARU_Event evt = {
          .text_input = {
            .text = utf8,
            .length = (uint32_t)len
          }
        };
        _maru_dispatch_event(ctx, MARU_TEXT_INPUT_RECEIVED, (MARU_Window *)window, &evt);
      }
      return 0;
    }
    }
  }

  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static ATOM _maru_win32_register_class(HINSTANCE hinstance) {
  static bool registered = false;
  if (registered) return 1;

  WNDCLASSA wc = { 0 };
  wc.lpfnWndProc = _maru_win32_wndproc;
  wc.hInstance = hinstance;
  wc.lpszClassName = _maru_win32_window_class;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  
  ATOM atom = RegisterClassA(&wc);
  if (atom) registered = true;
  return atom;
}

static HWND _maru_win32_create_window(HINSTANCE hinstance,
                                       const MARU_WindowCreateInfo *create_info,
                                       void* userdata) {
  ATOM atom = _maru_win32_register_class(hinstance);
  if (!atom) {
    return NULL;
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = 0;

  if (create_info->attributes.mouse_passthrough) {
    ex_style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
  }

  int width = 800;
  int height = 600;
  if (create_info->attributes.logical_size.x > 0 && create_info->attributes.logical_size.y > 0) {
    width = (int)create_info->attributes.logical_size.x;
    height = (int)create_info->attributes.logical_size.y;
  }

  RECT rect = { 0, 0, width, height };
  AdjustWindowRectEx(&rect, style, FALSE, ex_style);

  const char* title = create_info->attributes.title ? create_info->attributes.title : "Maru Window";

  HWND hwnd = CreateWindowExA(
      ex_style,
      _maru_win32_window_class,
      title,
      style,
      CW_USEDEFAULT, CW_USEDEFAULT,
      rect.right - rect.left, rect.bottom - rect.top,
      NULL, NULL,
      hinstance,
      NULL);

  if (hwnd) {
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)userdata);
  }

  return hwnd;
}

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  MARU_Window_Windows *window = (MARU_Window_Windows *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_Windows));
  if (!window) {
    return MARU_FAILURE;
  }
  
  _maru_init_window_base(&window->base, &ctx->base, create_info);
  memset(((uint8_t*)window) + sizeof(MARU_Window_Base), 0, sizeof(MARU_Window_Windows) - sizeof(MARU_Window_Base));

#ifdef MARU_INDIRECT_BACKEND
  window->base.backend = ctx->base.backend;
#endif

  window->size = create_info->attributes.logical_size;
  if (window->size.x <= 0) window->size.x = 800;
  if (window->size.y <= 0) window->size.y = 600;

  HMODULE hinstance = GetModuleHandleA(NULL);
  window->hwnd = _maru_win32_create_window(hinstance, create_info, window);
  if (!window->hwnd) {
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  window->hdc = GetDC(window->hwnd);
  if (!window->hdc) {
    DestroyWindow(window->hwnd);
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  ShowWindow(window->hwnd, SW_SHOW);
  UpdateWindow(window->hwnd);

  if (create_info->attributes.mouse_passthrough) {
    SetLayeredWindowAttributes(window->hwnd, 0, 255, LWA_ALPHA);
  }

  window->base.pub.flags = MARU_WINDOW_STATE_READY;
  _maru_register_window(&ctx->base, (MARU_Window *)window);

  if (ctx->base.event_cb && (ctx->base.event_mask & MARU_WINDOW_READY)) {
      MARU_Event evt = { {0} };
      ctx->base.event_cb(MARU_WINDOW_READY, (MARU_Window *)window, &evt);
  }

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Windows(MARU_Window *window_handle) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)window_handle;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)window->base.ctx_base;

  _maru_unregister_window(&ctx->base, window_handle);

  if (window->hdc) {
    ReleaseDC(window->hwnd, window->hdc);
  }

  if (window->hwnd) {
    DestroyWindow(window->hwnd);
  }

  _maru_cleanup_window_base(&window->base);
  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

MARU_Status maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_Windows *window = (const MARU_Window_Windows *)window_handle;

  RECT window_rect = { 0, 0, 0, 0 };
  RECT client_rect = { 0, 0, 0, 0 };

  if (window->hwnd) {
    GetWindowRect(window->hwnd, &window_rect);
    GetClientRect(window->hwnd, &client_rect);
  }

  *out_geometry = (MARU_WindowGeometry){
      .origin = { (MARU_Scalar)window_rect.left, (MARU_Scalar)window_rect.top },
      .logical_size = { (MARU_Scalar)client_rect.right, (MARU_Scalar)client_rect.bottom },
      .pixel_size = { client_rect.right, client_rect.bottom },
  };

  return MARU_SUCCESS;
}

MARU_Status maru_updateWindow_Windows(MARU_Window *window_handle, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)window_handle;

  _maru_update_window_base(&window->base, field_mask, attributes);

  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    SetWindowTextA(window->hwnd, attributes->title);
  }

  if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
    RECT rect = { 0, 0, (int)attributes->logical_size.x, (int)attributes->logical_size.y };
    DWORD style = GetWindowLongA(window->hwnd, GWL_STYLE);
    DWORD ex_style = GetWindowLongA(window->hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
    SetWindowPos(window->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  if (field_mask & MARU_WINDOW_ATTR_FULLSCREEN) {
    if (attributes->fullscreen && !window->is_fullscreen) {
      window->saved_style = GetWindowLongA(window->hwnd, GWL_STYLE);
      window->saved_ex_style = GetWindowLongA(window->hwnd, GWL_EXSTYLE);
      GetWindowRect(window->hwnd, &window->saved_rect);

      MONITORINFO mi = { sizeof(mi) };
      if (GetMonitorInfoA(MonitorFromWindow(window->hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        SetWindowLongA(window->hwnd, GWL_STYLE, window->saved_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window->hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        
        window->is_fullscreen = true;
        window->base.pub.flags |= MARU_WINDOW_STATE_FULLSCREEN;
      }
    } else if (!attributes->fullscreen && window->is_fullscreen) {
      SetWindowLongA(window->hwnd, GWL_STYLE, window->saved_style);
      SetWindowLongA(window->hwnd, GWL_EXSTYLE, window->saved_ex_style);
      SetWindowPos(window->hwnd, NULL,
                   window->saved_rect.left, window->saved_rect.top,
                   window->saved_rect.right - window->saved_rect.left,
                   window->saved_rect.bottom - window->saved_rect.top,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
      
      window->is_fullscreen = false;
      window->base.pub.flags &= ~MARU_WINDOW_STATE_FULLSCREEN;
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_EVENT_MASK) {
    window->base.pub.event_mask = attributes->event_mask;
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
    window->current_cursor = (MARU_Cursor_Windows *)attributes->cursor;
    window->base.pub.current_cursor = attributes->cursor;
    PostMessageA(window->hwnd, WM_SETCURSOR, (WPARAM)window->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
  }

  if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
    window->cursor_mode = attributes->cursor_mode;
    window->base.pub.cursor_mode = attributes->cursor_mode;
    // For HIDDEN/LOCKED, we might need to capture or clip the cursor
    // For now just trigger WM_SETCURSOR
    PostMessageA(window->hwnd, WM_SETCURSOR, (WPARAM)window->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
  }

  if (field_mask & MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH) {
    DWORD ex_style = GetWindowLongA(window->hwnd, GWL_EXSTYLE);
    if (attributes->mouse_passthrough) {
      ex_style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
      SetWindowLongA(window->hwnd, GWL_EXSTYLE, ex_style);
      SetLayeredWindowAttributes(window->hwnd, 0, 255, LWA_ALPHA);
    } else {
      ex_style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
      SetWindowLongA(window->hwnd, GWL_EXSTYLE, ex_style);
    }
    SetWindowPos(window->hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  }

  // TODO: implement other attributes

  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window_handle) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)window_handle;
  SetForegroundWindow(window->hwnd);
  SetFocus(window->hwnd);
  return MARU_SUCCESS;
}

MARU_Status maru_getWindowBackendHandle_Windows(MARU_Window *window_handle,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)window_handle;
  *out_type = MARU_BACKEND_WINDOWS;
  out_handle->win32_hwnd = window->hwnd;
  return MARU_SUCCESS;
}
