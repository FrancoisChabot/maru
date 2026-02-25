// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_DATAEXCHANGE_H_INCLUDED
#define MARU_LINUX_DATAEXCHANGE_H_INCLUDED

#include <poll.h>
#include <stddef.h>
#include <stdint.h>

#include "maru/c/data_exchange.h"
#include "maru_internal.h"

typedef struct MARU_LinuxDataTransfer {
  int fd;
  uint8_t *buffer;
  size_t size;
  size_t capacity;
  MARU_Window *window;
  MARU_DataExchangeTarget target;
  const char *mime_type;
  void *user_tag;
  struct MARU_LinuxDataTransfer *next;
} MARU_LinuxDataTransfer;

MARU_Status maru_linux_dataexchange_queueTransfer(MARU_Context_Base *ctx_base,
                                                  MARU_LinuxDataTransfer **head,
                                                  int fd,
                                                  MARU_Window *window,
                                                  MARU_DataExchangeTarget target,
                                                  const char *mime_type,
                                                  void *user_tag);
int maru_linux_dataexchange_fillPollFds(const MARU_LinuxDataTransfer *head,
                                        struct pollfd *pfds, int max_count);
void maru_linux_dataexchange_processTransfers(MARU_Context_Base *ctx_base,
                                              MARU_LinuxDataTransfer **head,
                                              const struct pollfd *pfds,
                                              int pfds_offset, int pfds_count);
void maru_linux_dataexchange_destroyTransfers(MARU_Context_Base *ctx_base,
                                              MARU_LinuxDataTransfer **head);

#endif  // MARU_LINUX_DATAEXCHANGE_H_INCLUDED
