#include <sys/epoll.h>

#include <netinet/in.h> /* Internet address family, see ip(7) and socket(7) */
#include <sys/socket.h>
#include <sys/types.h> /* socket, ssize_t, etc. */

#include <unistd.h> /* POSIX close, write, read, pipe */

#include <signal.h> /* signal, sigaction */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

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

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        return 1;
    }

    /* 
     * Override the default handling of the SIGPIPE.
     * This can be done either with signal(2) or sigaction(2) 
     * Note that Linux manual discourages from using signal(2) and encourages sigaction(2) instead.
     *
     * Alternatively signal(SIGPIPE, SIG_IGN) could hae been used.
     */
    struct sigaction action = { .sa_handler = SIG_IGN };
    sigaction(SIGPIPE, &action, NULL);

    const int port = atoi(argv[1]);
    /*
     * Configure the IP4 address family
     * https://github.com/torvalds/linux/blob/master/include/linux/socket.h#L178
     *
     * sin_port has to be in network byte order (big-endian).
     * Further reading:
     *  https://www.ibm.com/support/knowledgecenter/SSB27U_6.4.0/com.ibm.zvm.v640.kiml0/asonetw.htm
     *  https://www.gnu.org/software/libc/manual/html_node/Byte-Order.html
     *
     *  Remember that on Linux ports below 1024 are privileged and only process with
     *  CAP_NET_BIND_SERVICE cab bind to such port.
     */
    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = PRINT_CALL_RESULT(socket(AF_INET, SOCK_STREAM, 0)); /* Create a socket for internet communication */

    /* 
     * SO_REUSEADDR - for address family AF_INET this indicates that may bind to an address, 
     *                unless there is an active listening socket bound to "that" address.
     * SO_REUSEPORT - permit multiple AF_INET sockets to bind to the same socket address. 
     */
    PRINT_CALL_RESULT(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)));

    /* 
     * Why do we cast an instance of sockaddr_in to sockaddr?
     *
     * sockaddr is a generic socket address structure, whereas sockaddr_in is a protocol specific type.
     * In C the structure address and the address of its first member are the same.
     * Notice that both structs have the same "first" member.
     *
     * The "in" in sockaddr_in stands for internet (or IP network - I'm not sure).
     * For instance there is also sockaddr_un used with unix sockets.
     * Each socket domain, has a different struct type to store address.
     * Further reading: "Linux Programming Interface" by Michael Kerrisk.
     */
    PRINT_CALL_RESULT(bind(sockfd, (struct sockaddr*) &socket_address, sizeof(socket_address)));
    PRINT_CALL_RESULT(listen(sockfd, 128)); /* TODO: What is the best value of backlog in this case? */

    #define MAX_EVENTS 16
    struct epoll_event event;
    struct epoll_event polled_events[MAX_EVENTS];

    int epfd = epoll_create1(EPOLL_CLOEXEC);

    event.events = EPOLLIN;
    event.data.fd = sockfd;

    PRINT_CALL_RESULT(epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event));
    /* 
     * Why did we effectively passed the monitored file descriptor twice to epoll_ctl?
     *
     * Notice that the data property of the epoll_event structure is of type epoll_data,
     * which is an union (see epoll_ctl(2)). For that reason epoll_ctl cannot rely on a hope
     * that a programmer will fill the fd field of epoll_data and thus provide 
     * the monitored file descriptor.
     */

    printf("Server is listening on port %d\n", port);

    while(1) {
        int num_fds = PRINT_CALL_RESULT(epoll_wait(epfd, polled_events, MAX_EVENTS, -1));
        printf("epoll_wait returned %d new events\n", num_fds);
        for (int i = 0; i < num_fds; ++i) {
            if (polled_events[i].data.fd == sockfd) {
                printf("Accepting new incoming connection\n");
                int client_sockfd = PRINT_CALL_RESULT(accept(sockfd, NULL, NULL));
                event.events = EPOLLIN;
                event.data.fd = client_sockfd;
                PRINT_CALL_RESULT(epoll_ctl(epfd, EPOLL_CTL_ADD, client_sockfd, &event));
            } else {
                printf("Receiving data from a client with socket fd: %d\n", polled_events[i].data.fd);
#define RECV_BUFFER_SIZE 1024
                char buffer[RECV_BUFFER_SIZE];
                ssize_t recv_buff_len = read(polled_events[i].data.fd, buffer, RECV_BUFFER_SIZE);
                if (recv_buff_len > 0) {
                    const char* msg = "Hello stranger! Thanks for the message!\n";
                    write(polled_events[i].data.fd, msg, strlen(msg)*sizeof(char));
                }
                
                if (recv_buff_len < 1) {
                    printf("Closing connection with client with socket fd: %d\n", polled_events[i].data.fd);
                    close(polled_events[i].data.fd); 
                }
            }
        }
    }

    return 0;
}