#include "tcp/server.h"
#include "utils/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_tcp_server_listening = 0;

int tcp_server_main_loop(struct netc_tcp_server* server)
{
    /** The server socket should be nonblocking when listening for events. */
    socket_set_non_blocking(server->sockfd);
    netc_tcp_server_listening = 1;

    while (netc_tcp_server_listening)
    {
#ifdef __linux__
        int pfd = server->pfd;
        struct epoll_event events[server->clients->size + 1];
        int nev = epoll_wait(pfd, events, server->clients->size + 1, -1);
        if (nev == -1)
            return netc_error(EVCREATE);
#elif _WIN32
        int pfd = server->pfd;
        int nev = WSAWaitForMultipleEvents(server->clients->size + 1, &pfd, FALSE, -1, FALSE);
        if (nev == WSA_WAIT_FAILED) return netc_error(EVCREATE);
#elif __APPLE__
        int pfd = server->pfd;
        struct kevent evlist[server->clients->size + 1];
        int nev = kevent(pfd, NULL, 0, evlist, server->clients->size + 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_tcp_server_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            int sockfd = ev.data.fd;

            if (sockfd == server->sockfd)
            {
                if (ev.events & EPOLLERR || ev.events & EPOLLHUP) // server socket closed
                    return netc_error(HANGUP);

                 if (server->on_connect != NULL) server->on_connect(server);
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
                     server->on_data(server, sockfd);
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
                    if (server->on_connect != NULL) server->on_connect(server);
                }
            }
            else
            {
                if (WSAEnumNetworkEvents(sockfd, server->pfd, &client->events) == SOCKET_ERROR) return netc_error(HANGUP);

                if (client->events.lNetworkEvents & FD_READ)
                {
                    if (client->events.iErrorCode[FD_READ_BIT] != 0) return netc_error(HANGUP);
                   if (server->on_data != NULL) server->on_data(server, sockfd);
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
            struct kevent ev = evlist[i];
            int sockfd = ev.ident;

            if (sockfd == server->sockfd)
            {
                if (ev.flags & EV_EOF || ev.flags & EV_ERROR) // server socket closed
                    return netc_error(HANGUP); 

                if (server->on_connect != NULL) server->on_connect(server);
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
                     server->on_data(server, sockfd);
                }
            }
#endif
        }
    }

    return 0;
};

int tcp_server_init(struct netc_tcp_server* server, struct netc_tcp_server_config config)
{
    if (server == NULL) return -1;
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    server->port = config.port;
    server->backlog = config.backlog;
    server->non_blocking = config.non_blocking;

    server->sockfd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (server->sockfd == -1) return netc_error(SOCKET);

    if (protocol == AF_INET6)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&server->address;
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = in6addr_any;
        addr->sin6_port = htons(server->port);
    }
    else
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)&server->address;
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = INADDR_ANY;
        addr->sin_port = htons(server->port);
    }

    /** The size of the server's address. */
    server->addrlen = sizeof(server->address);

    if (config.reuse_addr && setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &config.reuse_addr, sizeof(int)) != 0)
        return netc_error(SOCKOPT);

    server->clients = malloc(sizeof(struct vector));
    vector_init(server->clients, 10, sizeof(int));

    if (config.non_blocking == 0) return 0;
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

int tcp_server_bind(struct netc_tcp_server* server)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&server->address;
    socklen_t addrlen = server->addrlen;

    int result = bind(sockfd, addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int tcp_server_listen(struct netc_tcp_server* server)
{
    socket_t sockfd = server->sockfd;

    int result = listen(sockfd, server->backlog);
    if (result == -1) return netc_error(LISTEN);
    
    return 0;
};

int tcp_server_accept(struct netc_tcp_server* server, struct netc_tcp_client* client)
{
    socket_t sockfd = server->sockfd;
    struct sockaddr* addr = (struct sockaddr*)&client->sockaddr;
    socklen_t* addrlen = &client->addrlen;

    int result = accept(sockfd, addr, addrlen);
    if (result == -1) return netc_error(ACCEPT);

    client->sockfd = result;
    vector_push(server->clients, &result);

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

int tcp_server_send(int sockfd, char* message, size_t msglen)
{
    int flags = 0;
    int result = send(sockfd, message, msglen, flags);
    
    if (result == -1) return netc_error(SERVSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int tcp_server_receive(int sockfd, char* message, size_t msglen)
{
    int flags = 0;
    int result = recv(sockfd, message, msglen, flags);

    if (result == -1) return netc_error(SERVRECV);
    else if (result == 0) return netc_error(BADRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int tcp_server_close_self(struct netc_tcp_server* server)
{
    int result = close(server->sockfd);
    if (result == -1) return netc_error(CLOSE);

    netc_tcp_server_listening = 0;

    return 0;
};

int tcp_server_close_client(struct netc_tcp_server* server, int sockfd, int is_error)
{
    if (server->on_disconnect != NULL) server->on_disconnect(server, sockfd, is_error);

    int result = close(sockfd);
    if (result == -1) return netc_error(CLOSE);

    vector_delete(server->clients, sockfd);

    return 0;
};