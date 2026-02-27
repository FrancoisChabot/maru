#include "tracking_allocator.h"

#include <cstdlib>
#include <cstring>

MARU_IntegrationTrackingAllocator::MARU_IntegrationTrackingAllocator()
    : allocator_{},
      head_(nullptr),
      live_alloc_count_(0u),
      live_alloc_bytes_(0u),
      total_alloc_count_(0u),
      total_free_count_(0u),
      alloc_event_count_(0u),
      oom_enabled_(false),
      oom_fail_at_event_(0u),
      oom_triggered_(false),
      tracking_error_(false) {
  allocator_.alloc_cb = &MARU_IntegrationTrackingAllocator::alloc_cb;
  allocator_.realloc_cb = &MARU_IntegrationTrackingAllocator::realloc_cb;
  allocator_.free_cb = &MARU_IntegrationTrackingAllocator::free_cb;
  allocator_.userdata = this;
  reset();
}

MARU_IntegrationTrackingAllocator::~MARU_IntegrationTrackingAllocator() {
  shutdown();
}

void MARU_IntegrationTrackingAllocator::reset() {
  shutdown();
  head_ = nullptr;
  live_alloc_count_ = 0u;
  live_alloc_bytes_ = 0u;
  total_alloc_count_ = 0u;
  total_free_count_ = 0u;
  alloc_event_count_ = 0u;
  oom_enabled_ = false;
  oom_fail_at_event_ = 0u;
  oom_triggered_ = false;
  tracking_error_ = false;
}

void MARU_IntegrationTrackingAllocator::apply(MARU_ContextCreateInfo *create_info) {
  if (!create_info) {
    return;
  }
  create_info->allocator = allocator_;
}

void MARU_IntegrationTrackingAllocator::set_oom(bool enabled,
                                                 size_t fail_at_event) {
  oom_enabled_ = enabled;
  oom_fail_at_event_ = fail_at_event;
  oom_triggered_ = false;
}

size_t MARU_IntegrationTrackingAllocator::alloc_event_count() const {
  return alloc_event_count_;
}

bool MARU_IntegrationTrackingAllocator::oom_triggered() const {
  return oom_triggered_;
}

bool MARU_IntegrationTrackingAllocator::is_clean() const {
  return !tracking_error_ && live_alloc_count_ == 0u && live_alloc_bytes_ == 0u;
}

MARU_IntegrationTrackingAllocator::TrackedAllocation *
MARU_IntegrationTrackingAllocator::find_allocation(
    void *ptr, TrackedAllocation ***out_prev_next) {
  if (out_prev_next) {
    *out_prev_next = nullptr;
  }
  if (!ptr) {
    return nullptr;
  }

  TrackedAllocation **link = &head_;
  while (*link) {
    if ((*link)->ptr == ptr) {
      if (out_prev_next) {
        *out_prev_next = link;
      }
      return *link;
    }
    link = &(*link)->next;
  }
  return nullptr;
}

void *MARU_IntegrationTrackingAllocator::alloc_cb(size_t size, void *userdata) {
  auto *tracking = static_cast<MARU_IntegrationTrackingAllocator *>(userdata);
  if (!tracking) {
    return nullptr;
  }

  tracking->alloc_event_count_++;
  if (tracking->oom_enabled_ &&
      tracking->alloc_event_count_ == tracking->oom_fail_at_event_) {
    tracking->oom_triggered_ = true;
    return nullptr;
  }

  void *ptr = std::malloc(size);
  if (!ptr) {
    return nullptr;
  }

  auto *node =
      static_cast<TrackedAllocation *>(std::malloc(sizeof(TrackedAllocation)));
  if (!node) {
    std::free(ptr);
    tracking->tracking_error_ = true;
    return nullptr;
  }

  node->ptr = ptr;
  node->size = size;
  node->next = tracking->head_;
  tracking->head_ = node;
  tracking->live_alloc_count_++;
  tracking->live_alloc_bytes_ += size;
  tracking->total_alloc_count_++;
  return ptr;
}

void *MARU_IntegrationTrackingAllocator::realloc_cb(void *ptr, size_t new_size,
                                                     void *userdata) {
  auto *tracking = static_cast<MARU_IntegrationTrackingAllocator *>(userdata);
  if (!tracking) {
    return nullptr;
  }

  if (!ptr) {
    return alloc_cb(new_size, userdata);
  }

  if (new_size == 0u) {
    TrackedAllocation **prev_next = nullptr;
    TrackedAllocation *node = tracking->find_allocation(ptr, &prev_next);
    if (node && prev_next) {
      *prev_next = node->next;
      if (tracking->live_alloc_count_ > 0u) {
        tracking->live_alloc_count_--;
      }
      if (tracking->live_alloc_bytes_ >= node->size) {
        tracking->live_alloc_bytes_ -= node->size;
      } else {
        tracking->live_alloc_bytes_ = 0u;
        tracking->tracking_error_ = true;
      }
      std::free(node);
    } else {
      tracking->tracking_error_ = true;
    }
    std::free(ptr);
    tracking->total_free_count_++;
    return nullptr;
  }

  tracking->alloc_event_count_++;
  if (tracking->oom_enabled_ &&
      tracking->alloc_event_count_ == tracking->oom_fail_at_event_) {
    tracking->oom_triggered_ = true;
    return nullptr;
  }

  TrackedAllocation **prev_next = nullptr;
  TrackedAllocation *node = tracking->find_allocation(ptr, &prev_next);

  void *new_ptr = std::realloc(ptr, new_size);
  if (!new_ptr) {
    return nullptr;
  }

  if (!node) {
    node = static_cast<TrackedAllocation *>(
        std::malloc(sizeof(TrackedAllocation)));
    if (!node) {
      tracking->tracking_error_ = true;
      return new_ptr;
    }
    node->ptr = new_ptr;
    node->size = new_size;
    node->next = tracking->head_;
    tracking->head_ = node;
    tracking->live_alloc_count_++;
    tracking->live_alloc_bytes_ += new_size;
    tracking->total_alloc_count_++;
    tracking->tracking_error_ = true;
    return new_ptr;
  }

  if (tracking->live_alloc_bytes_ >= node->size) {
    tracking->live_alloc_bytes_ -= node->size;
  } else {
    tracking->live_alloc_bytes_ = 0u;
    tracking->tracking_error_ = true;
  }
  tracking->live_alloc_bytes_ += new_size;
  node->ptr = new_ptr;
  node->size = new_size;
  return new_ptr;
}

void MARU_IntegrationTrackingAllocator::free_cb(void *ptr, void *userdata) {
  if (!ptr) {
    return;
  }

  auto *tracking = static_cast<MARU_IntegrationTrackingAllocator *>(userdata);
  if (!tracking) {
    std::free(ptr);
    return;
  }

  TrackedAllocation **prev_next = nullptr;
  TrackedAllocation *node = tracking->find_allocation(ptr, &prev_next);
  if (!node || !prev_next) {
    tracking->tracking_error_ = true;
    std::free(ptr);
    tracking->total_free_count_++;
    return;
  }

  *prev_next = node->next;
  if (tracking->live_alloc_count_ > 0u) {
    tracking->live_alloc_count_--;
  }
  if (tracking->live_alloc_bytes_ >= node->size) {
    tracking->live_alloc_bytes_ -= node->size;
  } else {
    tracking->live_alloc_bytes_ = 0u;
    tracking->tracking_error_ = true;
  }

  std::free(node);
  std::free(ptr);
  tracking->total_free_count_++;
}

void MARU_IntegrationTrackingAllocator::shutdown() {
  TrackedAllocation *it = head_;
  head_ = nullptr;
  while (it) {
    TrackedAllocation *next = it->next;
    std::free(it->ptr);
    std::free(it);
    it = next;
  }
  live_alloc_count_ = 0u;
  live_alloc_bytes_ = 0u;
}
