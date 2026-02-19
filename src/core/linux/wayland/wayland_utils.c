#include "wayland_internal.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef MARU_ENABLE_INTERNAL_CHECKS
void _maru_wayland_symbol_poison_trap(void) {
  fprintf(stderr, "MARU: Fatal Error: Poisoned Wayland symbol called. "
                  "This usually means the library was linked against libwayland-client "
                  "directly instead of using the dynamic loader.\n");
  abort();
}
#endif
