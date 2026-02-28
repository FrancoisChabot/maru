// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char **mime_types, uint32_t count, MARU_DropActionMask allowed_actions) {
  return MARU_FAILURE;
}

MARU_Status maru_provideData_Cocoa(const MARU_DataRequestEvent *request_event, const void *data, size_t size, MARU_DataProvideFlags flags) {
  return MARU_FAILURE;
}

MARU_Status maru_requestData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *user_tag) {
  return MARU_FAILURE;
}

MARU_Status maru_getAvailableMIMETypes_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list) {
  return MARU_FAILURE;
}
