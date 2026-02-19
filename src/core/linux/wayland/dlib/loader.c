// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "loader.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "maru_internal.h"
#include "wayland-client.h"
#include "wayland-cursor.h"
#include "libdecor.h"

#ifdef MARU_VALIDATE_API_CALLS
static void _set_diagnostic(struct MARU_Context_Base* ctx, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char msg[256];
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE, msg);
}
#else
#define _set_diagnostic(...) ((void)0)
#endif

// dlsym causes unavoidable cast warnings...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static bool _wl_load_lib_base(struct MARU_Context_Base* ctx, const char* name, MARU_External_Lib_Base* tgt) {
  tgt->handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
  if (!tgt->handle) {
    _set_diagnostic(ctx, "dlopen(%s) failed: %s", name, dlerror());
    tgt->available = false;
    return false;
  }
  tgt->available = true;

  return tgt->available;
}

static void _wl_unload_lib_base(MARU_External_Lib_Base* tgt) {
  if (tgt->handle) {
    dlclose(tgt->handle);
  }
  tgt->handle = NULL;
  tgt->available = false;
}

static bool wl_load(struct MARU_Context_Base* ctx, MARU_Lib_WaylandClient *lib) {
  if (lib->base.available) return true;

  if (!_wl_load_lib_base(ctx, "libwayland-client.so.0", &lib->base)) {
    return false;
  }
  bool functions_ok = true;

#define MARU_LIB_FN(name)                                  \
  lib->name = dlsym(lib->base.handle, "wl_" #name);        \
  if (!lib->name) {                                        \
    _set_diagnostic(ctx, "dlsym(wl_" #name ") failed");    \
    functions_ok = false;                                  \
  }
  MARU_WL_FUNCTIONS_TABLE
#undef MARU_LIB_FN

  if (!functions_ok) {
    _wl_unload_lib_base(&lib->base);
  }
  return lib->base.available;
}

static bool wlc_load(struct MARU_Context_Base* ctx, MARU_Lib_WaylandCursor *lib) {
  if (lib->base.available) return true;

  if (!_wl_load_lib_base(ctx, "libwayland-cursor.so.0", &lib->base)) {
    return false;
  }
  bool functions_ok = true;

#define MARU_LIB_FN(name)                                           \
  lib->name = dlsym(lib->base.handle, "wl_cursor_" #name);          \
  if (!lib->name) {                                                 \
    _set_diagnostic(ctx, "dlsym(wl_cursor_" #name ") failed");      \
    functions_ok = false;                                           \
  }
  MARU_WL_CURSOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN

  if (!functions_ok) {
    _wl_unload_lib_base(&lib->base);
  }
  return lib->base.available;
}

static bool decor_load(struct MARU_Context_Base* ctx, MARU_Lib_Decor *lib) {
  if (lib->base.available) return true;

  if (!_wl_load_lib_base(ctx, "libdecor-0.so.0", &lib->base)) {
    return false;
  }
  bool functions_ok = true;

#define MARU_LIB_FN(name)                                              \
  lib->name = dlsym(lib->base.handle, "libdecor_" #name);              \
  if (!lib->name) {                                                    \
    _set_diagnostic(ctx, "dlsym(libdecor_" #name ") failed");          \
    functions_ok = false;                                              \
  }
#define MARU_LIB_OPT_FN(name) \
  lib->opt.name = dlsym(lib->base.handle, "libdecor_" #name);
  MARU_LIBDECOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
#undef MARU_LIB_OPT_FN

  if (!functions_ok) {
    _wl_unload_lib_base(&lib->base);
  }
  return lib->base.available;
}

#pragma GCC diagnostic pop

bool maru_load_wayland_symbols(struct MARU_Context_Base *ctx, 
                               MARU_Lib_WaylandClient *wl, 
                               MARU_Lib_WaylandCursor *wlc, 
                               MARU_Lib_Decor *decor) {
  if (!wl_load(ctx, wl)) goto failure_wl;
  if (!wlc_load(ctx, wlc)) goto failure_wlc;

  decor_load(ctx, decor);  // Optional

  return true;

failure_wlc:
  _wl_unload_lib_base(&wlc->base);
failure_wl:
  return false;
}

void maru_unload_wayland_symbols(MARU_Lib_WaylandClient *wl, 
                                 MARU_Lib_WaylandCursor *wlc, 
                                 MARU_Lib_Decor *decor) {
  _wl_unload_lib_base(&decor->base);
  _wl_unload_lib_base(&wlc->base);
  _wl_unload_lib_base(&wl->base);
}
