// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "ime_utils.h"

#include <string.h>

static bool _maru_is_utf8_continuation_byte(unsigned char byte) {
  return (byte & 0xC0u) == 0x80u;
}

static bool _maru_is_valid_utf8_sequence(const char* text,
                                         uint32_t length_bytes) {
  uint32_t i = 0;
  while (i < length_bytes) {
    const unsigned char lead = (unsigned char)text[i];

    if (lead <= 0x7Fu) {
      ++i;
      continue;
    }

    if (lead >= 0xC2u && lead <= 0xDFu) {
      if (i + 1u >= length_bytes ||
          !_maru_is_utf8_continuation_byte((unsigned char)text[i + 1u])) {
        return false;
      }
      i += 2u;
      continue;
    }

    if (lead >= 0xE0u && lead <= 0xEFu) {
      const unsigned char b1 =
          (i + 1u < length_bytes) ? (unsigned char)text[i + 1u] : 0u;
      const unsigned char b2 =
          (i + 2u < length_bytes) ? (unsigned char)text[i + 2u] : 0u;
      if (i + 2u >= length_bytes ||
          !_maru_is_utf8_continuation_byte(b1) ||
          !_maru_is_utf8_continuation_byte(b2)) {
        return false;
      }
      if ((lead == 0xE0u && b1 < 0xA0u) ||
          (lead == 0xEDu && b1 >= 0xA0u)) {
        return false;
      }
      i += 3u;
      continue;
    }

    if (lead >= 0xF0u && lead <= 0xF4u) {
      const unsigned char b1 =
          (i + 1u < length_bytes) ? (unsigned char)text[i + 1u] : 0u;
      const unsigned char b2 =
          (i + 2u < length_bytes) ? (unsigned char)text[i + 2u] : 0u;
      const unsigned char b3 =
          (i + 3u < length_bytes) ? (unsigned char)text[i + 3u] : 0u;
      if (i + 3u >= length_bytes ||
          !_maru_is_utf8_continuation_byte(b1) ||
          !_maru_is_utf8_continuation_byte(b2) ||
          !_maru_is_utf8_continuation_byte(b3)) {
        return false;
      }
      if ((lead == 0xF0u && b1 < 0x90u) ||
          (lead == 0xF4u && b1 >= 0x90u)) {
        return false;
      }
      i += 4u;
      continue;
    }

    return false;
  }

  return true;
}

static bool _maru_is_utf8_boundary(const char* text, uint32_t byte_offset) {
  uint32_t i = 0;

  while (true) {
    if (i == byte_offset) {
      return true;
    }
    if (text[i] == '\0' || i > byte_offset) {
      return false;
    }

    const unsigned char lead = (unsigned char)text[i];
    if (lead <= 0x7Fu) {
      ++i;
    } else if (lead >= 0xC2u && lead <= 0xDFu) {
      i += 2u;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
      i += 3u;
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
      i += 4u;
    } else {
      return false;
    }
  }
}

bool maru_applyTextEditCommitUtf8(char* buffer, uint32_t capacity_bytes,
                                  uint32_t* inout_length,
                                  uint32_t* inout_cursor_byte,
                                  const MARU_TextEditCommittedEvent* commit) {
  if (!buffer || !inout_length || !inout_cursor_byte || !commit) {
    return false;
  }

  const uint32_t length = *inout_length;
  const uint32_t cursor = *inout_cursor_byte;
  if (cursor > length) {
    return false;
  }
  if (length >= capacity_bytes || buffer[length] != '\0') {
    return false;
  }
  if (!_maru_is_valid_utf8_sequence(buffer, length)) {
    return false;
  }
  if (!_maru_is_utf8_boundary(buffer, cursor)) {
    return false;
  }
  if (commit->delete_before_bytes > cursor) {
    return false;
  }

  const uint32_t delete_start = cursor - commit->delete_before_bytes;
  const uint32_t delete_end = cursor + commit->delete_after_bytes;
  if (delete_end > length || delete_end < delete_start) {
    return false;
  }
  if (!_maru_is_utf8_boundary(buffer, delete_start) ||
      !_maru_is_utf8_boundary(buffer, delete_end)) {
    return false;
  }
  if (commit->committed_length_bytes > 0 && !commit->committed_utf8) {
    return false;
  }
  if (commit->committed_length_bytes > 0 &&
      !_maru_is_valid_utf8_sequence(commit->committed_utf8,
                                    commit->committed_length_bytes)) {
    return false;
  }

  const uint32_t delete_len = delete_end - delete_start;
  const uint32_t tail_len = length - delete_end;
  const uint32_t new_length =
      length - delete_len + commit->committed_length_bytes;
  if (new_length >= capacity_bytes) {
    return false;
  }

  if (commit->committed_length_bytes != delete_len) {
    memmove(buffer + delete_start + commit->committed_length_bytes,
            buffer + delete_end, tail_len);
  }
  if (commit->committed_length_bytes > 0) {
    memcpy(buffer + delete_start, commit->committed_utf8,
           commit->committed_length_bytes);
  }

  *inout_length = new_length;
  *inout_cursor_byte = delete_start + commit->committed_length_bytes;
  buffer[new_length] = '\0';
  return true;
}
