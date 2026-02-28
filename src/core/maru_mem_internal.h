// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_MEM_INTERNAL_H_INCLUDED
#define MARU_MEM_INTERNAL_H_INCLUDED

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "maru/maru.h"
#include "maru_internal.h"
#include "maru/c/details/maru_config.h"
#include <stddef.h>

#ifdef MARU_GATHER_METRICS
#include <stddef.h>
typedef union MARU_MemoryHeader {
  size_t size;
  max_align_t _align;
} MARU_MemoryHeader;

static inline void _maru_update_mem_metrics_alloc(MARU_Context_Base *ctx_base, size_t size) {
  ctx_base->metrics.memory_allocated_current += size;
  if (ctx_base->metrics.memory_allocated_current > ctx_base->metrics.memory_allocated_peak) {
    ctx_base->metrics.memory_allocated_peak = ctx_base->metrics.memory_allocated_current;
  }
}

static inline void _maru_update_mem_metrics_free(MARU_Context_Base *ctx_base, size_t size) {
  ctx_base->metrics.memory_allocated_current -= size;
}
#endif

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
    alloc.realloc_cb = _maru_default_realloc;
    alloc.free_cb = _maru_default_free;
    alloc.userdata = NULL;
  }

#ifdef MARU_GATHER_METRICS
  MARU_MemoryHeader *header = (MARU_MemoryHeader *)alloc.alloc_cb(size + sizeof(MARU_MemoryHeader), alloc.userdata);
  if (!header) return NULL;
  header->size = size;
  void *ptr = header + 1;
#else
  void *ptr = alloc.alloc_cb(size, alloc.userdata);
#endif

  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void *maru_context_alloc(MARU_Context_Base *ctx_base, size_t size) {
  fprintf(stderr, "maru_context_alloc(%zu)\n", size);
#ifdef MARU_GATHER_METRICS
  MARU_MemoryHeader *header = (MARU_MemoryHeader *)ctx_base->allocator.alloc_cb(size + sizeof(MARU_MemoryHeader), ctx_base->allocator.userdata);
  if (!header) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
    return NULL;
  }
  header->size = size;
  _maru_update_mem_metrics_alloc(ctx_base, size);
  return header + 1;
#else
  void *ptr = ctx_base->allocator.alloc_cb(size, ctx_base->allocator.userdata);
  if (!ptr) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
  }
  return ptr;
#endif
}

static inline void maru_context_free(MARU_Context_Base *ctx_base, void *ptr) {
  if (!ptr) return;
  MARU_Allocator alloc = ctx_base->allocator;
#ifdef MARU_GATHER_METRICS
  MARU_MemoryHeader *header = (MARU_MemoryHeader *)ptr - 1;
  if (ctx_base) {
    _maru_update_mem_metrics_free(ctx_base, header->size);
  }
  ptr = header;
#endif
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
#ifdef MARU_GATHER_METRICS
  (void)old_size;
  MARU_MemoryHeader *old_header = ptr ? (MARU_MemoryHeader *)ptr - 1 : NULL;
  size_t actual_old_size = old_header ? old_header->size : 0;

  MARU_MemoryHeader *new_header;
  if (ctx_base->allocator.realloc_cb) {
    new_header = (MARU_MemoryHeader *)ctx_base->allocator.realloc_cb(old_header, new_size + sizeof(MARU_MemoryHeader), ctx_base->allocator.userdata);
  } else {
    if (new_size == 0) {
      maru_context_free(ctx_base, ptr);
      return NULL;
    }
    new_header = (MARU_MemoryHeader *)ctx_base->allocator.alloc_cb(new_size + sizeof(MARU_MemoryHeader), ctx_base->allocator.userdata);
    if (new_header && old_header) {
      size_t copy_size = (actual_old_size < new_size) ? actual_old_size : new_size;
      memcpy(new_header + 1, old_header + 1, copy_size);
      maru_context_free(ctx_base, ptr);
    }
  }

  if (!new_header && new_size > 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
    return NULL;
  }

  if (new_header) {
    new_header->size = new_size;
    if (ctx_base) {
      _maru_update_mem_metrics_free(ctx_base, actual_old_size);
      _maru_update_mem_metrics_alloc(ctx_base, new_size);
    }
    return new_header + 1;
  }
  return NULL;
#else
  void *new_ptr = maru_realloc(&ctx_base->allocator, ptr,
                               old_size, new_size);
  if (!new_ptr && new_size > 0) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context *)ctx_base, MARU_DIAGNOSTIC_OUT_OF_MEMORY, "Out of memory");
  }

  return new_ptr;
#endif
}

#endif  // MARU_MEM_INTERNAL_H_INCLUDED
