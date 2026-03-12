// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Status maru_announceData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList mime_types, MARU_DropActionMask allowed_actions) {
  (void)window;
  (void)target;
  (void)mime_types;
  (void)allowed_actions;
  return MARU_FAILURE;
}

MARU_Status maru_provideData_Cocoa(MARU_DataRequest *request, const void *data, size_t size, MARU_DataProvideFlags flags) {
  return MARU_FAILURE;
}

MARU_Status maru_requestData_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, const char *mime_type, void *userdata) {
  return MARU_FAILURE;
}

MARU_Status maru_getAvailableMIMETypes_Cocoa(MARU_Window *window, MARU_DataExchangeTarget target, MARU_MIMETypeList *out_list) {
  return MARU_FAILURE;
}
