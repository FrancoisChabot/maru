// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EXAMPLES_IME_UTILS_H_INCLUDED
#define MARU_EXAMPLES_IME_UTILS_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Example/support helper for applying a byte-based UTF-8 IME commit to a
 * caller-owned buffer in place.
 *
 * This is not part of Maru's installed core API. Copy it into your project if
 * you want a ready-made implementation for MARU_TextEditCommittedEvent.
 *
 * `buffer` must contain valid UTF-8 for the first `*inout_length` bytes and be
 * NUL-terminated at `buffer[*inout_length]`. `*inout_cursor_byte` and the
 * commit's delete spans must land on UTF-8 code point boundaries.
 * `capacity_bytes` must include room for the trailing NUL after the updated
 * text.
 */
bool maru_applyTextEditCommitUtf8(char* buffer,
                                  uint32_t capacity_bytes,
                                  uint32_t* inout_length,
                                  uint32_t* inout_cursor_byte,
                                  const MARU_TextEditCommittedEvent* commit);

#ifdef __cplusplus
}
#endif

#endif
