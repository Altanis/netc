#include "../../include/udp/client.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

int udp_client_main_loop(struct udp_client *client)
{
    /** The client socket should be nonblocking when listening for events. */
    socket_set_non_blocking(client->sockfd);
    client->listening = 1;

    while (client->listening)
    {
#ifdef __linux__
        int pfd = client->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(POLL_FD);
#elif _WIN32
        WSAPOLLFD events[1];
        events[0].fd = client->sockfd;
        events[0].events = POLLIN;
        int nev = WSAPoll(events, sizeof(events), -1);
        if (nev == -1) return netc_error(POLL_FD);
#elif __APPLE__
        int pfd = client->pfd;
        struct kevent events[1];
        int nev = kevent(pfd, NULL, 0, events, 1, NULL);
        if (nev == -1) return netc_error(POLL_FD);
#endif

        if (client->listening == 0) break;

#ifdef __linux__
        struct epoll_event ev = events[0];
        if (ev.events & EPOLLIN && client->on_data != NULL) client->on_data(client);
#elif _WIN32
            WSAPOLLFD event = events[0];
            if (event.revents & POLLIN && client->on_data != NULL) client->on_data(client);
#elif __APPLE__
        if (events[0].flags & EVFILT_READ && client->on_data != NULL) client->on_data(client);
#endif
    };

    return 0;
};

int udp_client_init(struct udp_client *client, struct sockaddr *addr, int non_blocking)
{
    if (client == NULL) return -1;

    client->sockaddr = addr;
    int protocol = addr->sa_family;

    client->sockfd = socket(protocol, SOCK_DGRAM, 0);
    if (client->sockfd == -1) return netc_error(SOCKET_C);

    client->listening = 0;

    if (non_blocking == 0) return 0;
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FD_CTL);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) return netc_error(SIGPIPE);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
#elif __APPLE__
    client->pfd = kqueue();
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, client->sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(client->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int udp_client_connect(struct udp_client *client)
{
    socket_t sockfd = client->sockfd;
    struct sockaddr *addr = client->sockaddr;
    socklen_t addrlen = sizeof(struct sockaddr);

    int result = connect(sockfd, addr, addrlen);
    if (result == -1 && errno != EINPROGRESS) return netc_error(CONNECT);

    return 0;
};

int udp_client_send(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t server_addrlen)
{
    socket_t sockfd = client->sockfd;

    int result = sendto(sockfd, message, msglen, flags, server_addr, server_addrlen);
    if (result == -1) netc_error(BADSEND);

    return result;
};

int udp_client_receive(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t *server_addrlen)
{
    socket_t sockfd = client->sockfd;

    int result = recvfrom(sockfd, message, msglen, flags, server_addr, server_addrlen);
    if (result == -1) netc_error(BADRECV);

    return result;
};

int udp_client_close(struct udp_client *client)
{
    socket_t sockfd = client->sockfd;
    client->listening = 0;

#ifdef _WIN32
    int result = closesocket(sockfd);
#else
    int result = close(sockfd);
#endif

    if (result == -1) return netc_error(CLOSE);

    return 0;
};