#include <errno.h>
#include <netinet/in.h> /* Internet address family, see ip(7) and socket(7) */
#include <poll.h>
#include <signal.h> /* signal, sigaction */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    struct pollfd fds[OBSERVED_FDS_POLL] = {
        {.fd = sockfd, .events = POLLIN}, /* Set server's socket as the first element */
    };

    for (int i = 1; i < OBSERVED_FDS_POLL; ++i) {
        fds[i].events = POLLIN;
        fds[i].fd = UNUSED_FD;
    }

    printf("Server is listenting on port %d\n", port);

    while (PRINT_CALL_RESULT(poll(fds, OBSERVED_FDS_POLL, -1))) {
        for (int i = 0; i < OBSERVED_FDS_POLL; ++i) {
            struct pollfd* polled_fd = fds + i;
            if (!(polled_fd->revents & POLLIN)) {
                if (polled_fd->revents != 0) {
                    /*
                     * In this app we're not interested in other events, however if such occurres,
                     * let's log it to the console. What could be expected here? If for instance,
                     * we did not handle the client's socket file descriptor properly
                     * when connection is terminated (i.e. file descriptor has been closed),
                     * we might the POLLNVAL error. See poll(2) for more information.
                     */
                    printf("Event (%d): %d on fd: %d\n", i, polled_fd->revents, polled_fd->fd);
                }

                /* If reading from a file descriptor is not possible, then do nothing. */
                continue;
            }

            if (i == 0) { /* the server's socker */
                printf("Accepting new incoming connection\n");
                /* polled_fd.fd in this case is quivalet to sockfd */
                int client_sockfd = PRINT_CALL_RESULT(accept(polled_fd->fd, NULL, NULL));

                /*
                 * Now we'd like to add client's file descriptor to the fds array.
                 * To do so we have to find the first element of fds array which has
                 * the fd property not set to an actual file descriptor (i.e. is NULL)
                 */
                int free_fd_idx;
                for (free_fd_idx = 0; fds[free_fd_idx].fd > UNUSED_FD; ++free_fd_idx)
                    ;
                fds[free_fd_idx].fd = client_sockfd;
            } else {
                printf("Receiving data from the client with socket fd: %d\n", polled_fd->fd);

                char buffer[RECV_BUFFER_SIZE];
                ssize_t recv_buff_len = read(polled_fd->fd, buffer, sizeof(buffer));
                if (recv_buff_len > 0) {
                    const char* msg = "Hello stranger! Thanks for the message!\n";
                    write(polled_fd->fd, msg, strlen(msg) * sizeof(char));
                }

                if (recv_buff_len < 1) {
                    printf("Closing connection with client with socket fd: %d\n", polled_fd->fd);
                    close(polled_fd->fd);
                    polled_fd->fd = UNUSED_FD;
                }
            }
        }
    }
    return 0;
}