#include "udp/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/errno.h>
#endif

__thread int netc_udp_server_listening = 0;

int udp_server_main_loop(struct udp_server *server)
{
    /** The server socket should be nonblocking when listening for events. */
    socket_set_non_blocking(server->sockfd);
    netc_udp_server_listening = 1;

    while (netc_udp_server_listening)
    {
#ifdef __linux__
        int pfd = server->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(EVCREATE);
#elif _WIN32
        SOCKET sockfd = server->sockfd;
        WSAEVENT event = server->event;

        DWORD dw_result = WSAWaitForMultipleEvents(1, &event, FALSE, WSA_INFINITE, FALSE);
        if (dw_result == WSA_WAIT_FAILED)
        {
            WSACloseEvent(event);
            WSACleanup();
            return netc_error(WSA_WAIT);
        };

        WSANETWORKEVENTS events;
        if (WSAEnumNetworkEvents(sockfd, event, &events) == SOCKET_ERROR)
        {
            WSACloseEvent(event);
            WSACleanup();
            return netc_error(NETWORK_EVENT);
        };
#elif __APPLE__
        int pfd = server->pfd;
        struct kevent events[1];
        int nev = kevent(pfd, NULL, 0, events, 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_udp_server_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            socket_t sockfd = ev.data.fd;
            if (ev.events & EPOLLIN && server->on_data != NULL) server->on_data(server, server->data);
#elif _WIN32
            if (events.lNetworkEvents & FD_READ && server->on_data != NULL) server->on_data(server, server->data);
#elif __APPLE__
            socket_t sockfd = server->sockfd;
            if (events[i].flags & EVFILT_READ && server->on_data != NULL) server->on_data(server, server->data);
#endif
        };

    };

    return 0;
};

int udp_server_init(struct udp_server *server, struct sockaddr addr, int non_blocking)
{
    if (server == NULL) return -1;

    server->sockaddr = addr;
    int protocol = addr.sa_family;

    server->sockfd = socket(protocol, SOCK_DGRAM, 0); // IPv4, UDP, 0
    if (server->sockfd == -1) return netc_error(SOCKET_C);

    if (non_blocking == 0) return 0;
    if (socket_set_non_blocking(server->sockfd) == -1) return netc_error(FD_CTL);

    /** Register event for the server socket. */
#ifdef __linux__
    server->pfd = epoll_create1(0);
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server->sockfd;

    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, server->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    server->event = WSACreateEvent();
    if (server->event == WSA_INVALID_EVENT)
    {
        WSACloseEvent(server->event);
        WSACleanup();
        return netc_error(EVCREATE);
    };

    if (WSAEventSelect(server->sockfd, server->pfd, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
    {
        WSACloseEvent(server->event);
        WSACleanup();
        return netc_error(EVENT_SELECT);
    };
#elif __APPLE__
    server->pfd = kqueue();
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, server->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif
    return 0;
};

int udp_server_bind(struct udp_server *server)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr addr = server->sockaddr;
    socklen_t addrlen = sizeof(addr);

    int result = bind(sockfd, &addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int udp_server_send(struct udp_server *server, char *message, size_t msglen, int flags, struct sockaddr *client_addr, socklen_t client_addrlen)
{
    socket_t sockfd = server->sockfd;

    int result = sendto(sockfd, message, msglen, flags, client_addr, client_addrlen);
    if (result == -1) netc_error(BADSEND);

    return result;
};

int udp_server_receive(struct udp_server *server, char *message, size_t msglen, int flags, struct sockaddr *client_addr, socklen_t *client_addrlen)
{
    socket_t sockfd = server->sockfd;

    int result = recvfrom(sockfd, message, msglen, flags, client_addr, client_addrlen);
    if (result == -1) netc_error(BADRECV);

    return result;
};

int udp_server_close(struct udp_server *server)
{
    socket_t sockfd = server->sockfd;

#ifdef _WIN32
    int result = closesocket(sockfd);
#else
    int result = close(sockfd);
#endif

    if (result == -1) return netc_error(CLOSE);

    netc_udp_server_listening = 0;

    return 0;
};