#include "udp/client.h"

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_udp_client_listening = 0;

int udp_client_init(struct netc_udp_client* client, struct netc_udp_client_config config)
{
    if (client == NULL) return -1;

    int protocol_from = config.ipv6_connect_from ? AF_INET6 : AF_INET;
    int protocol_to = config.ipv6_connect_to ? AF_INET6 : AF_INET;

    client->sockfd = socket(protocol_from, SOCK_DGRAM, 0);
    if (client->sockfd == -1) return netc_error(SOCKET);

    if (protocol_from == AF_INET6)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&client->sockaddr;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(config.port);

        if (inet_pton(protocol_to, config.ip, &addr->sin6_addr) <= 0) return netc_error(INETPTON);
    }
    else
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)&client->sockaddr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(config.port);

        if (inet_pton(protocol_to, config.ip, &addr->sin_addr) <= 0) return netc_error(INETPTON);
    };

    client->addrlen = sizeof(client->sockaddr);

    if (config.non_blocking == 0) return 0;
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FCNTL);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    client->pfd = WSACreateEvent();
    if (client->pfd == WSA_INVALID_EVENT) return netc_error(EVCREATE);

    if (WSAEventSelect(client->sockfd, client->pfd, FD_READ) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
    client->pfd = kqueue();
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, client->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(client->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif
};