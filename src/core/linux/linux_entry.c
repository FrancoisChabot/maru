#include "maru_api_constraints.h"

#ifdef MARU_INDIRECT_BACKEND

extern MARU_Status maru_createContext_Linux(const MARU_ContextCreateInfo *create_info,
                                           MARU_Context **out_context);

MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                               MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Linux(create_info, out_context);
}

#endif
