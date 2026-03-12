// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

/**
 * ATTENTION: This file is in the maru details/ directory. This means that
 * it's NOT part of the Maru API, and is just machinery that is required to
 * implement the API.
 *
 * Nothing in here is meant to be stable, or even read by a user of the library.
 */

#ifndef MARU_DETAILS_DATA_EXCHANGE_H_INCLUDED
#define MARU_DETAILS_DATA_EXCHANGE_H_INCLUDED

#include "maru/c/data_exchange.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_DropSessionExposed {
  MARU_DropAction *action;
  void **session_userdata;
} MARU_DropSessionExposed;

static inline void maru_setDropSessionAction(MARU_DropSession *session,
                                             MARU_DropAction action) {
  *(((MARU_DropSessionExposed *)session)->action) = action;
}

static inline MARU_DropAction
maru_getDropSessionAction(const MARU_DropSession *session) {
  return *(((const MARU_DropSessionExposed *)session)->action);
}

static inline void maru_setDropSessionUserdata(MARU_DropSession *session,
                                               void *userdata) {
  *(((MARU_DropSessionExposed *)session)->session_userdata) = userdata;
}

static inline void *
maru_getDropSessionUserdata(const MARU_DropSession *session) {
  return *(((const MARU_DropSessionExposed *)session)->session_userdata);
}

#ifdef __cplusplus
}
#endif

#endif // MARU_DETAILS_DATA_EXCHANGE_H_INCLUDED
