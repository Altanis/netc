#include "udp/server.h"

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

int udp_server_main_loop(struct netc_udp_server* server)
{
    /** The server socket should be nonblocking when listening for events. */
    socket_set_non_blocking(server->sockfd);

    while (netc_udp_server_listening)
    {
#ifdef __linux__
        int pfd = server->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(EVCREATE);
#elif _WIN32
        int pfd = server->pfd;
        int nev = WSAWaitForMultipleEvents(1, &pfd, FALSE, -1, FALSE);
        if (nev == WSA_WAIT_FAILED) return netc_error(EVCREATE);
#elif __APPLE__
        int pfd = server->pfd;
        struct kevent evlist[1];
        int nev = kevent(pfd, NULL, 0, evlist, 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_udp_server_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            int sockfd = ev.data.fd;
            if (ev.events & EPOLLIN && server->on_data != NULL) server->on_data();
#elif _WIN32
            SOCKET sockfd = server->sockfd;
            if (WSAEnumNetworkEvents(sockfd, pfd, &server->events) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
            int sockfd = server->sockfd;
            if (evlist[i].flags & EVFILT_READ && server->on_data != NULL) server->on_data();
#endif
        };

    };
};

int udp_server_init(struct netc_udp_server* server, struct netc_udp_server_config config)
{
    if (server == NULL) return -1;
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    server->port = config.port;
    server->backlog = config.backlog;

    server->sockfd = socket(protocol, SOCK_DGRAM, 0); // IPv4, UDP, 0
    if (server->sockfd == -1) return netc_error(SOCKET);

    if (protocol == AF_INET6)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&server->socket_addr;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(config.port);
        addr->sin6_addr = in6addr_any;
    }
    else
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)&server->socket_addr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(config.port);
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
    };

    /** The size of the server's address. */
    server->addrlen = sizeof(server->socket_addr);

    if (config.reuse_addr && setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &config.reuse_addr, sizeof(int)) == -1) 
        return netc_error(SOCKOPT);

    if (config.non_blocking == 0) return 0;
    if (socket_set_non_blocking(server->sockfd) == -1) return netc_error(FCNTL);

    /** Register event for the server socket. */
#ifdef __linux__
    server->pfd = epoll_create1(0);
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server->sockfd;

    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, server->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    server->pfd = WSACreateEvent();
    if (server->pfd == WSA_INVALID_EVENT) return netc_error(EVCREATE);

    if (WSAEventSelect(server->sockfd, server->pfd, FD_READ) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
    server->pfd = kqueue();
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, server->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif
};

int udp_server_bind(struct netc_udp_server* server)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&server->socket_addr;
    socklen_t addrlen = server->addrlen;

    int result = bind(sockfd, addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int udp_server_receive(struct netc_udp_server* server, char* message, size_t msglen, struct sockaddr* client_addr, socklen_t* client_addrlen)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&server->socket_addr;
    socklen_t addrlen = server->addrlen;

    int result = recvfrom(sockfd, message, msglen, 0, client_addr, client_addrlen);
    if (result == -1) return netc_error(SERVRECV);

    return 0;
};

int udp_server_send(struct netc_udp_server* server, const char* message, size_t msglen, struct sockaddr* client_addr, socklen_t client_addrlen)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&server->socket_addr;
    socklen_t addrlen = server->addrlen;

    int result = sendto(sockfd, message, msglen, 0, client_addr, client_addrlen);
    if (result == -1) return netc_error(SERVSEND);

    return 0;
};

int udp_server_close(struct netc_udp_server* server)
{
    socket_t sockfd = server->sockfd;

    int result = close(sockfd);
    if (result == -1) return netc_error(CLOSE);

    return 0;
};