#include <errno.h>
#include <netinet/in.h> /* Internet address family, see ip(7) and socket(7) */
#include <signal.h>     /* signal, sigaction */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h> /* socket, ssize_t, etc. */
#include <unistd.h>    /* POSIX close, write, read, pipe */

int print_call_result(int code, const char* call) {
    if (code < 0) { /* error */
        fputs(call, stderr);
        fprintf(stderr, "\tError: %s\n", strerror(errno));
        abort(); /* send SIGABRT to the process */
    }

    printf("[CALL] %s ==> %d\n", call, code);

    return code;
}

#define PRINT_CALL_RESULT(CALL) print_call_result(CALL, #CALL)
#define OBSERVED_FDS_POLL 64
#define RECV_BUFFER_SIZE 1024
#define UNUSED_FD -1

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        return 1;
    }
    struct sigaction action = {.sa_handler = SIG_IGN};
    sigaction(SIGPIPE, &action, NULL);

    const int port = atoi(argv[1]);
    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = PRINT_CALL_RESULT(
        socket(AF_INET, SOCK_STREAM, 0)); /* Create a socket for internet communication */
    PRINT_CALL_RESULT(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)));

    PRINT_CALL_RESULT(bind(sockfd, (struct sockaddr*)&socket_address, sizeof(socket_address)));
    PRINT_CALL_RESULT(
        listen(sockfd, 128)); /* TODO: What is the best value of backlog in this case? */

    fd_set readfds, readfds_copy;
    int nfds = 0;
    FD_ZERO(&readfds);
    FD_ZERO(&readfds_copy);

    FD_SET(sockfd, &readfds);
    FD_COPY(&readfds, &readfds_copy);
    if (sockfd >= nfds) nfds = sockfd + 1;

    printf("Server is listenting on port %d\n", port);

    while (PRINT_CALL_RESULT(select(nfds, &readfds_copy, NULL, NULL, NULL))) {
        printf("Select called with nfds: %d", nfds);
        for (int fd = 3; fd < nfds; ++fd) {
            /* No point to check stdin, stdout, stderr (i.e. 0, 1, 2) */
            if (FD_ISSET(fd, &readfds_copy)) {
                printf("File descriptor %d is present in readfds\n", fd);

                if (fd == sockfd) { /* the server's socket */
                    printf("Accepting new incoming connection\n");
                    int client_sockfd = PRINT_CALL_RESULT(accept(fd, NULL, NULL));

                    /*
                     * Once we accept(2) and clinet's socket is returned,
                     * we want to be able to select(2) for reading from the client.
                     */
                    FD_SET(client_sockfd, &readfds);
                    if (client_sockfd >= nfds) nfds = client_sockfd + 1;
                    printf("Update nfds: %d\n", nfds);
                } else {
                    printf("Receiving data from the client with socket fd: %d\n", fd);
                    char buffer[RECV_BUFFER_SIZE];
                    ssize_t recv_buff_len = read(fd, buffer, sizeof(buffer));
                    if (recv_buff_len > 0) {
                        const char* msg = "Hello stranger! Thanks for the message!\n";
                        write(fd, msg, strlen(msg) * sizeof(char));
                    }

                    if (recv_buff_len < 1) {
                        printf("Closing connection with client with socket fd: %d\n", fd);
                        close(fd);
                        FD_CLR(fd, &readfds);
                        /*
                         * TODO:
                         *   Can nfds be recomputed when a file descriptor is closed?
                         *
                         *   In this implementation I do not recompute the value of nfds.
                         *   So once a file descriptor with a high value is closed nfds will stay
                         *   "high".
                         */
                    }
                }
            }
        }

        /*
         * Pointers to fd_set instances passed to select(2) (e.g. readfds, writefds, errorfds)
         * are all _value-result_. They are overwritten when select(2) returns. Because of that,
         * we need to be sure to reinitialize them before another call to select(2) is done.
         *
         * In my implementation I'm using two instances of fd_set. One which is _local_ and serves
         * as a reference. The second one is passed as argument to select(2),
         * which passes (copies) it to kernel, and used as a _value-result_.
         */
        FD_COPY(&readfds, &readfds_copy);
    }
    return 0;
}