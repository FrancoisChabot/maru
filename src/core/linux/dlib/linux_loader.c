// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "linux_loader.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "xkbcommon.h"
#include "maru_internal.h"

#ifdef MARU_VALIDATE_API_CALLS
static void _set_diagnostic(struct MARU_Context_Base* ctx, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char msg[256];
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE, msg);
}
#else
#define _set_diagnostic(...) ((void)0)
#endif

// dlsym causes unavoidable cast warnings...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static bool _lib_load_base(struct MARU_Context_Base* ctx, const char* name, MARU_External_Lib_Base* tgt) {
  tgt->handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
  if (!tgt->handle) {
    _set_diagnostic(ctx, "dlopen(%s) failed: %s", name, dlerror());
    tgt->available = false;
    return false;
  }
  tgt->available = true;
  return true;
}

static void _lib_unload_base(MARU_External_Lib_Base* tgt) {
  if (tgt->handle) {
    dlclose(tgt->handle);
  }
  tgt->handle = NULL;
  tgt->available = false;
}

bool maru_linux_xkb_load(struct MARU_Context_Base *ctx, MARU_Lib_Xkb *out_lib) {
  if (out_lib->base.available) return true;

  if (!_lib_load_base(ctx, "libxkbcommon.so.0", &out_lib->base)) {
    return false;
  }

  bool functions_ok = true;
#define MARU_LIB_FN(name)                                            \
  out_lib->name = dlsym(out_lib->base.handle, "xkb_" #name);         \
  if (!out_lib->name) {                                              \
    _set_diagnostic(ctx, "dlsym(xkb_" #name ") failed");             \
    functions_ok = false;                                            \
  }
  MARU_XKB_FUNCTIONS_TABLE
#undef MARU_LIB_FN

  if (!functions_ok) {
    _lib_unload_base(&out_lib->base);
    return false;
  }

  return out_lib->base.available;
}

bool maru_linux_udev_load(struct MARU_Context_Base *ctx, MARU_Lib_Udev *out_lib) {
  if (out_lib->base.available) return true;

  if (!_lib_load_base(ctx, "libudev.so.1", &out_lib->base) &&
      !_lib_load_base(ctx, "libudev.so.0", &out_lib->base)) {
    return false;
  }

  bool functions_ok = true;
#define MARU_LIB_FN(ret, name, args)                                  \
  out_lib->name = dlsym(out_lib->base.handle, #name);                 \
  if (!out_lib->name) {                                               \
    _set_diagnostic(ctx, "dlsym(" #name ") failed");                  \
    functions_ok = false;                                             \
  }
  MARU_UDEV_FUNCTIONS_TABLE
#undef MARU_LIB_FN

  if (!functions_ok) {
    _lib_unload_base(&out_lib->base);
    return false;
  }

  return out_lib->base.available;
}

void maru_linux_xkb_unload(MARU_Lib_Xkb *lib) {
  _lib_unload_base(&lib->base);
}

void maru_linux_udev_unload(MARU_Lib_Udev *lib) {
  _lib_unload_base(&lib->base);
}

#pragma GCC diagnostic pop
