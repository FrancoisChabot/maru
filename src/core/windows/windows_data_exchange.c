// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_announceData_Windows(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions) {
  (void)window;
  (void)target;
  (void)mime_types;
  (void)count;
  (void)allowed_actions;
  return MARU_FAILURE;
}

MARU_Status maru_provideData_Windows(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags) {
  (void)request_event;
  (void)data;
  (void)size;
  (void)flags;
  return MARU_FAILURE;
}

MARU_Status maru_requestData_Windows(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag) {
  (void)window;
  (void)target;
  (void)mime_type;
  (void)user_tag;
  return MARU_FAILURE;
}

MARU_Status maru_getAvailableMIMETypes_Windows(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list) {
  (void)window;
  (void)target;
  if (out_list) {
    out_list->mime_types = NULL;
    out_list->count = 0;
  }
  return MARU_FAILURE;
}
