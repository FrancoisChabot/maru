#include <stdio.h>
#include "maru/maru.h"

int main() {
    MARU_Version version = maru_getVersion();
    printf("Maru version: %u.%u.%u\n", version.major, version.minor, version.patch);
    printf("MARU_Scalar size: %zu bytes\n", sizeof(MARU_Scalar));

    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    create_info.userdata = (void*)0xDEADBEEF;

    MARU_Context *context = NULL;
    if (maru_createContext(&create_info, &context) == MARU_SUCCESS) {
        printf("Context created successfully. Userdata: %p\n", maru_getContextUserdata(context));
        if (maru_isContextReady(context)) {
            printf("Context is ready.\n");
        }
        maru_destroyContext(context);
        printf("Context destroyed.\n");
    } else {
        printf("Failed to create context.\n");
    }

    return 0;
}
