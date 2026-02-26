// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "loader.h"
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include "maru_internal.h"

#ifdef MARU_ENABLE_DIAGNOSTICS
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

static bool _x11_load_lib_base(struct MARU_Context_Base* ctx, const char* name, MARU_External_Lib_Base* tgt) {
  tgt->handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
  if (!tgt->handle) {
    _set_diagnostic(ctx, "dlopen(%s) failed: %s", name, dlerror());
    tgt->available = false;
    return false;
  }
  tgt->available = true;
  return true;
}

static void _x11_unload_lib_base(MARU_External_Lib_Base* tgt) {
  if (tgt->handle) {
    dlclose(tgt->handle);
  }
  tgt->handle = NULL;
  tgt->available = false;
}

bool maru_load_x11_symbols(struct MARU_Context_Base *ctx, MARU_Lib_X11 *lib) {
  if (lib->base.available) return true;

  if (!_x11_load_lib_base(ctx, "libX11.so.6", &lib->base)) {
    return false;
  }
  bool functions_ok = true;

#define MARU_LIB_FN(name)                                  \
  lib->name = dlsym(lib->base.handle, #name);              \
  if (!lib->name) {                                        \
    _set_diagnostic(ctx, "dlsym(" #name ") failed");       \
    functions_ok = false;                                  \
  }
  MARU_X11_FUNCTIONS_TABLE
#undef MARU_LIB_FN

  if (!functions_ok) {
    _x11_unload_lib_base(&lib->base);
  }
  return lib->base.available;
}

void maru_unload_x11_symbols(MARU_Lib_X11 *lib) {
  _x11_unload_lib_base(&lib->base);
}

#pragma GCC diagnostic pop
