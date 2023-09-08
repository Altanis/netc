#include "tcp/server.h"
#include "utils/error.h"

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

__thread int netc_tcp_server_listening = 0;

int tcp_server_main_loop(struct tcp_server* server)
{
    /** The server socket should be nonblocking when listening for events. */
    socket_set_non_blocking(server->sockfd);
    netc_tcp_server_listening = 1;

    while (netc_tcp_server_listening)
    {
#ifdef __linux__
        int pfd = server->pfd;
        struct epoll_event events[server->client_count + 1];
        int nev = epoll_wait(pfd, events, server->client_count + 1, -1);
        if (nev == -1)
            return netc_error(EVCREATE);
#elif _WIN32
        int pfd = server->pfd;
        int nev = WSAWaitForMultipleEvents(server->client_count + 1, &pfd, FALSE, -1, FALSE);
        if (nev == WSA_WAIT_FAILED) return netc_error(EVCREATE);
#elif __APPLE__
        int pfd = server->pfd;
        struct kevent events[server->client_count + 1];
        int nev = kevent(pfd, NULL, 0, events, server->client_count + 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_tcp_server_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            socket_t sockfd = ev.data.fd;

            if (sockfd == server->sockfd)
            {
                if (ev.events & EPOLLERR || ev.events & EPOLLHUP) // server socket closed
                    return netc_error(HANGUP);

                 if (server->on_connect != NULL) server->on_connect(server, server->data);
            }
            else
            {
                if (ev.events & EPOLLERR || ev.events & EPOLLHUP) // client socket closed
                {
                    if (tcp_server_close_client(server, sockfd, ev.events & EPOLLERR) != 0)
                        return netc_error(CLOSE);
                }
                else if (ev.events & EPOLLIN && server->on_data != NULL) 
                {
                     server->on_data(server, sockfd, server->data);
                }
            }
#elif _WIN32
            SOCKET sockfd = server->sockfd;

            if (sockfd == server->sockfd)
            {
                if (WSAEnumNetworkEvents(sockfd, server->pfd, &server->events) == SOCKET_ERROR) return netc_error(HANGUP);

                if (server->events.lNetworkEvents & FD_ACCEPT)
                {
                    if (server->events.iErrorCode[FD_ACCEPT_BIT] != 0) return netc_error(HANGUP);
                    if (server->on_connect != NULL) server->on_connect(server, server->data);
                }
            }
            else
            {
                if (WSAEnumNetworkEvents(sockfd, server->pfd, &client->events) == SOCKET_ERROR) return netc_error(HANGUP);

                if (client->events.lNetworkEvents & FD_READ)
                {
                    if (client->events.iErrorCode[FD_READ_BIT] != 0) return netc_error(HANGUP);
                   if (server->on_data != NULL) server->on_data(server, sockfd, server->data);
                }
                else if (client->events.lNetworkEvents & FD_WRITE)
                {
                    if (client->events.iErrorCode[FD_WRITE_BIT] != 0) return netc_error(HANGUP);
                }
                else if (client->events.lNetworkEvents & FD_CLOSE)
                {
                    if (client->events.iErrorCode[FD_CLOSE_BIT] != 0) return netc_error(HANGUP);

                    if (tcp_server_close_client(server, sockfd, 0) != 0)
                        return netc_error(CLOSE);
                }
            }
#elif __APPLE__
            struct kevent ev = events[i];
            socket_t sockfd = ev.ident;

            if (sockfd == server->sockfd)
            {
                if (ev.flags & EV_EOF || ev.flags & EV_ERROR) // server socket closed
                    return netc_error(HANGUP); 

                if (server->on_connect != NULL) server->on_connect(server, server->data);
            }
            else
            {
                if (ev.flags & EV_EOF || ev.flags & EV_ERROR) // client socket closed
                {
                    if (tcp_server_close_client(server, sockfd, ev.flags & EV_ERROR) != 0)
                        return netc_error(CLOSE);
                }
                else if (ev.flags & EVFILT_READ && server->on_data != NULL)
                {
                     server->on_data(server, sockfd, server->data);
                }
            }
#endif
        }
    }

    return 0;
};

int tcp_server_init(struct tcp_server* server, struct sockaddr address, int non_blocking)
{
    if (server == NULL) return -1;

    server->non_blocking = non_blocking;
    server->address = address;
    int protocol = address.sa_family;

    server->sockfd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (server->sockfd == -1) return netc_error(SOCKET);

    server->client_count = 0;

    if (server->non_blocking == 0) return 0;
    if (socket_set_non_blocking(server->sockfd) != 0) return netc_error(FCNTL);

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

    if (WSAEventSelect(server->sockfd, server->pfd, FD_ACCEPT) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
    server->pfd = kqueue();
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, server->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif 

    return 0;
};

int tcp_server_bind(struct tcp_server* server)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr addr = server->address;
    socklen_t addrlen = sizeof(addr);

    int result = bind(sockfd, &addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int tcp_server_listen(struct tcp_server* server, int backlog)
{
    socket_t sockfd = server->sockfd;

    int result = listen(sockfd, backlog);
    if (result == -1) return netc_error(LISTEN);
    
    return 0;
};

int tcp_server_accept(struct tcp_server* server, struct tcp_client* client)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&client->sockaddr;
    socklen_t addrlen = sizeof(client->sockaddr);

    int result = accept(sockfd, addr, &addrlen);
    if (result == -1) return netc_error(ACCEPT);

    client->sockfd = result;
    ++server->client_count;

    if (server->non_blocking == 0) return 0;
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FCNTL);

#ifdef __linux__
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    if (WSAEventSelect(client->sockfd, server->pfd, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
    struct kevent ev;
    EV_SET(&ev, client->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int tcp_server_send(socket_t sockfd, char* message, size_t msglen, int flags)
{
    int result = send(sockfd, message, msglen, flags);
    if (result == -1) netc_error(BADSEND);

    return result;
};

int tcp_server_receive(socket_t sockfd, char* message, size_t msglen, int flags)
{
    int result = recv(sockfd, message, msglen, flags);
    if (result == -1) netc_error(BADRECV);

    return result;
};

int tcp_server_close_self(struct tcp_server* server)
{
    int result = close(server->sockfd);
    if (result == -1) return netc_error(CLOSE);

    netc_tcp_server_listening = 0;

    if (server->on_disconnect != NULL) 
        server->on_disconnect(server, server->sockfd, 0, server->data);

    return 0;
};

int tcp_server_close_client(struct tcp_server* server, socket_t sockfd, int is_error)
{
    if (server->on_disconnect != NULL) server->on_disconnect(server, sockfd, is_error, server->data);

#ifdef _WIN32
    int result = closesocket(sockfd);
#else
    int result = close(sockfd);
#endif

    if (result == -1) return netc_error(CLOSE);

    --server->client_count;

    return 0;
};