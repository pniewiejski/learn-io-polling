#ifndef __COMMON_IO_POLLING__
#define __COMMON_IO_POLLING__

#include <errno.h>
#include <netinet/in.h> /* Internet address family, see ip(7) and socket(7)    */
#include <signal.h>     /* signal, sigaction                                   */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h> /* socket, ssize_t, etc.                                */
#include <unistd.h>    /* POSIX close, write, read, pipe                       */

#define RECV_BUFFER_SIZE 1024
#define OBSERVED_FDS_POLL 64

#define UNUSED_FD -1

#define MAX_EVENTS 16

int print_call_result(int code, const char* call);
#define PRINT_CALL_RESULT(CALL) print_call_result(CALL, #CALL)

#endif
