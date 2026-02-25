// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "linux_dataexchange.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "maru_mem_internal.h"

static const char *_maru_linux_dataexchange_copy_string(MARU_Context_Base *ctx_base,
                                                        const char *text) {
  if (!text) return NULL;
  const size_t len = strlen(text);
  char *copy = (char *)maru_context_alloc(ctx_base, len + 1u);
  if (!copy) return NULL;
  memcpy(copy, text, len + 1u);
  return copy;
}

MARU_Status maru_linux_dataexchange_queueTransfer(MARU_Context_Base *ctx_base,
                                                  MARU_LinuxDataTransfer **head,
                                                  int fd,
                                                  MARU_Window *window,
                                                  MARU_DataExchangeTarget target,
                                                  const char *mime_type,
                                                  void *user_tag) {
  if (!ctx_base || !head || fd < 0 || !mime_type) {
    if (fd >= 0) close(fd);
    return MARU_FAILURE;
  }

  MARU_LinuxDataTransfer *transfer =
      (MARU_LinuxDataTransfer *)maru_context_alloc(ctx_base, sizeof(*transfer));
  if (!transfer) {
    close(fd);
    return MARU_FAILURE;
  }
  memset(transfer, 0, sizeof(*transfer));

  transfer->mime_type = _maru_linux_dataexchange_copy_string(ctx_base, mime_type);
  if (!transfer->mime_type) {
    maru_context_free(ctx_base, transfer);
    close(fd);
    return MARU_FAILURE;
  }

  transfer->fd = fd;
  transfer->window = window;
  transfer->target = target;
  transfer->user_tag = user_tag;
  transfer->next = *head;
  *head = transfer;
  return MARU_SUCCESS;
}

int maru_linux_dataexchange_fillPollFds(const MARU_LinuxDataTransfer *head,
                                        struct pollfd *pfds, int max_count) {
  int count = 0;
  const MARU_LinuxDataTransfer *curr = head;
  while (curr && count < max_count) {
    pfds[count].fd = curr->fd;
    pfds[count].events = POLLIN;
    pfds[count].revents = 0;
    curr = curr->next;
    ++count;
  }
  return count;
}

static bool _maru_linux_dataexchange_grow_buffer(MARU_Context_Base *ctx_base,
                                                 MARU_LinuxDataTransfer *transfer,
                                                 size_t required_capacity) {
  if (required_capacity <= transfer->capacity) return true;
  size_t next_cap = (transfer->capacity == 0) ? 4096u : transfer->capacity;
  while (next_cap < required_capacity) next_cap *= 2u;
  uint8_t *new_buffer = (uint8_t *)maru_context_realloc(
      ctx_base, transfer->buffer, transfer->capacity, next_cap);
  if (!new_buffer) return false;
  transfer->buffer = new_buffer;
  transfer->capacity = next_cap;
  return true;
}

static void _maru_linux_dataexchange_dispatch_complete(MARU_Context_Base *ctx_base,
                                                       MARU_LinuxDataTransfer *transfer,
                                                       MARU_Status status) {
  MARU_Event evt = {0};
  evt.data_received.user_tag = transfer->user_tag;
  evt.data_received.status = status;
  evt.data_received.target = transfer->target;
  evt.data_received.mime_type = transfer->mime_type;
  evt.data_received.data = transfer->buffer;
  evt.data_received.size = transfer->size;
  _maru_dispatch_event(ctx_base, MARU_EVENT_DATA_RECEIVED, transfer->window, &evt);
}

void maru_linux_dataexchange_processTransfers(MARU_Context_Base *ctx_base,
                                              MARU_LinuxDataTransfer **head,
                                              const struct pollfd *pfds,
                                              int pfds_offset, int pfds_count) {
  if (!ctx_base || !head || !pfds || pfds_count <= 0) return;

  MARU_LinuxDataTransfer **link = head;
  MARU_LinuxDataTransfer *curr = *head;
  int idx = 0;

  while (curr && idx < pfds_count) {
    MARU_LinuxDataTransfer *next = curr->next;
    const short revents = pfds[pfds_offset + idx].revents;

    bool done = false;
    bool error = (revents & (POLLERR | POLLNVAL)) != 0;

    if (!error && (revents & (POLLIN | POLLHUP)) != 0) {
      for (;;) {
        uint8_t tmp[4096];
        const ssize_t n = read(curr->fd, tmp, sizeof(tmp));
        if (n > 0) {
          const size_t needed = curr->size + (size_t)n;
          if (!_maru_linux_dataexchange_grow_buffer(ctx_base, curr, needed)) {
            error = true;
            break;
          }
          memcpy(curr->buffer + curr->size, tmp, (size_t)n);
          curr->size += (size_t)n;
          continue;
        }
        if (n == 0) {
          done = true;
          break;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        error = true;
        break;
      }
    }

    if (done || error) {
      _maru_linux_dataexchange_dispatch_complete(ctx_base, curr,
                                                 error ? MARU_FAILURE : MARU_SUCCESS);
      close(curr->fd);
      *link = next;
      maru_context_free(ctx_base, curr->buffer);
      maru_context_free(ctx_base, (void *)curr->mime_type);
      maru_context_free(ctx_base, curr);
    } else {
      link = &curr->next;
    }

    curr = next;
    ++idx;
  }
}

void maru_linux_dataexchange_destroyTransfers(MARU_Context_Base *ctx_base,
                                              MARU_LinuxDataTransfer **head) {
  if (!ctx_base || !head) return;
  MARU_LinuxDataTransfer *curr = *head;
  *head = NULL;
  while (curr) {
    MARU_LinuxDataTransfer *next = curr->next;
    if (curr->fd >= 0) close(curr->fd);
    maru_context_free(ctx_base, curr->buffer);
    maru_context_free(ctx_base, (void *)curr->mime_type);
    maru_context_free(ctx_base, curr);
    curr = next;
  }
}
