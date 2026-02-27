#ifndef MARU_INTEGRATION_TRACKING_ALLOCATOR_H_INCLUDED
#define MARU_INTEGRATION_TRACKING_ALLOCATOR_H_INCLUDED

#include <cstddef>

#include "maru/maru.h"

class MARU_IntegrationTrackingAllocator {
 public:
  MARU_IntegrationTrackingAllocator();
  ~MARU_IntegrationTrackingAllocator();

  MARU_IntegrationTrackingAllocator(const MARU_IntegrationTrackingAllocator &) =
      delete;
  MARU_IntegrationTrackingAllocator &operator=(
      const MARU_IntegrationTrackingAllocator &) = delete;

  void reset();
  void apply(MARU_ContextCreateInfo *create_info);
  void set_oom(bool enabled, size_t fail_at_event);

  size_t alloc_event_count() const;
  bool oom_triggered() const;
  bool is_clean() const;

 private:
  struct TrackedAllocation {
    void *ptr;
    size_t size;
    TrackedAllocation *next;
  };

  static void *alloc_cb(size_t size, void *userdata);
  static void *realloc_cb(void *ptr, size_t new_size, void *userdata);
  static void free_cb(void *ptr, void *userdata);

  TrackedAllocation *find_allocation(void *ptr, TrackedAllocation ***out_prev_next);
  void shutdown();

  MARU_Allocator allocator_;
  TrackedAllocation *head_;
  size_t live_alloc_count_;
  size_t live_alloc_bytes_;
  size_t total_alloc_count_;
  size_t total_free_count_;
  size_t alloc_event_count_;
  bool oom_enabled_;
  size_t oom_fail_at_event_;
  bool oom_triggered_;
  bool tracking_error_;
};

#endif  // MARU_INTEGRATION_TRACKING_ALLOCATOR_H_INCLUDED
