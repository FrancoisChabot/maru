// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_MEM_INTERNAL_H_INCLUDED
#define MARU_MEM_INTERNAL_H_INCLUDED

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "maru/maru.h"
#include "maru_internal.h"

/* Memory allocation helpers */

static inline void *maru_alloc(const MARU_Allocator *alloc, size_t size) {
  return alloc->alloc_cb(size, alloc->userdata);
}

static inline void maru_free(const MARU_Allocator *alloc, void *ptr) {
  if (ptr) alloc->free_cb(ptr, alloc->userdata);
}

static inline void *maru_realloc(const MARU_Allocator *alloc, void *ptr,
                                 size_t old_size, size_t new_size) {
  if (alloc->realloc_cb) {
    return alloc->realloc_cb(ptr, new_size, alloc->userdata);
  } else {
    if (new_size == 0) {
      maru_free(alloc, ptr);
      return NULL;
    }

    void *new_ptr = maru_alloc(alloc, new_size);
    if (new_ptr && ptr) {
      if (old_size > 0) {
        size_t copy_size = (old_size < new_size) ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
      }
      maru_free(alloc, ptr);
    }
    return new_ptr;
  }
}

static inline void *maru_context_alloc_bootstrap(const MARU_ContextCreateInfo *create_info,
                                                 size_t size) {
  MARU_Allocator alloc;
  if (create_info->allocator.alloc_cb) {
    alloc = create_info->allocator;
  } else {
    alloc.alloc_cb = _maru_default_alloc;
    alloc.realloc_cb = NULL;
    alloc.free_cb = _maru_default_free;
    alloc.userdata = NULL;
  }
  void *ptr = alloc.alloc_cb(size, alloc.userdata);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void *maru_context_alloc(MARU_Context_Base *ctx_base, size_t size) {
  void *ptr = ctx_base->allocator.alloc_cb(size, ctx_base->allocator.userdata);
  if (!ptr) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
  }
  return ptr;
}

static inline void maru_context_free(MARU_Context_Base *ctx_base, void *ptr) {
  MARU_Allocator alloc = ctx_base->allocator;
  maru_free(&alloc, ptr);
}

static inline void *maru_context_alloc_aligned64(MARU_Context_Base *ctx_base, size_t size) {
  const size_t alignment = 64;
  /* Allocate extra space for alignment and to store the original pointer */
  void *ptr = maru_context_alloc(ctx_base, size + alignment + sizeof(void *));
  if (!ptr) return NULL;

  /* Calculate aligned address */
  size_t addr = (size_t)ptr + sizeof(void *);
  void *aligned_ptr = (void *)((addr + alignment - 1) & ~(alignment - 1));

  /* Store original pointer immediately before the aligned pointer */
  ((void **)aligned_ptr)[-1] = ptr;

  return aligned_ptr;
}

static inline void maru_context_free_aligned64(MARU_Context_Base *ctx_base, void *ptr) {
  if (ptr) {
    /* Retrieve original pointer stored before the aligned address */
    void *original_ptr = ((void **)ptr)[-1];
    maru_context_free(ctx_base, original_ptr);
  }
}

static inline void *maru_context_realloc(MARU_Context_Base *ctx_base, void *ptr, size_t old_size,
                                         size_t new_size) {
  void *new_ptr = maru_realloc(&ctx_base->allocator, ptr,
                               old_size, new_size);
  if (!new_ptr && new_size > 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
  }

  return new_ptr;
}

#endif  // MARU_MEM_INTERNAL_H_INCLUDED
