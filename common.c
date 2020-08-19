#include "common.h"

int print_call_result(int code, const char* call) {
    if (code < 0) { /* error */
        fputs(call, stderr);
        fprintf(stderr, "\tError: %s\n", strerror(errno));
        abort(); /* send SIGABRT to the process */
    }

    printf("[CALL] %s ==> %d\n", call, code);

    return code;
}