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
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_tcp_server_listening = 1;

int tcp_server_main_loop(struct netc_tcp_server* server)
{
    /** The server socket should be nonblocking when listening for events. */
    tcp_socket_set_non_blocking(server->socket_fd);

    while (netc_tcp_server_listening)
    {
#ifdef __linux__
        int pfd = server->pfd;
        struct epoll_event events[server->clients->size + 1];
        int nev = epoll_wait(pfd, events, server->clients->size + 1, -1);
        if (nev == -1)
            return netc_error(EVCREATE);
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

            if (sockfd == server->socket_fd)
            {
                if (ev.events & EPOLLERR || ev.events & EPOLLHUP) // server socket closed
                    return netc_error(HANGUP);

                 if (server->on_connect) server->on_connect(server);
            }
            else
            {
                struct netc_tcp_client* client = server->clients->data[sockfd];
                if (client == NULL) continue;

                if (ev.events & EPOLLERR || ev.events & EPOLLHUP) // client socket closed
                {
                    if (tcp_server_close_client(server, client, ev.events & EPOLLERR) != 0)
                        return netc_error(CLOSE);
                }
                else if (ev.events & EPOLLIN)
                {
                     server->on_data(server, client);
                }
            }
#elif __APPLE__
            struct kevent ev = evlist[i];
            int sockfd = ev.ident;

            if (sockfd == server->socket_fd)
            {
                if (ev.flags & EV_EOF || ev.flags & EV_ERROR) // server socket closed
                    return netc_error(HANGUP); 

                if (server->on_connect) server->on_connect(server);
            }
            else
            {
                struct netc_tcp_client* client = server->clients->data[sockfd];
                if (client == NULL) continue;

                if (ev.flags & EV_EOF || ev.flags & EV_ERROR) // client socket closed
                {
                    if (tcp_server_close_client(server, client, ev.flags & EV_ERROR) != 0)
                        return netc_error(CLOSE);
                }
                else if (ev.flags & EVFILT_READ)
                {
                     server->on_data(server, client);
                }
            }
#endif
        }
    }

    return 0;
};

int tcp_server_init(struct netc_tcp_server* server, struct netc_tcp_server_config config)
{
    if (server == NULL) return -1; // server is NULL
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    server->port = config.port;
    server->backlog = config.backlog;
    server->non_blocking = config.non_blocking;

    server->socket_fd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (server->socket_fd == -1) return netc_error(SOCKET);

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

    if (config.reuse_addr && setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &config.reuse_addr, sizeof(int)) != 0)
        return netc_error(SOCKOPT);

    server->clients = malloc(sizeof(struct vector));
    vector_init(server->clients, 10, sizeof(struct netc_tcp_client*));

    if (config.non_blocking == 0) return 0;
    if (tcp_socket_set_non_blocking(server->socket_fd) != 0) return netc_error(FCNTL);

    /** Register event for the server socket. */
#ifdef __linux__
    server->pfd = epoll_create1(0);
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server->socket_fd;
    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, server->socket_fd, &ev) == -1) return netc_error(POLL_FD);
#elif __APPLE__
    server->pfd = kqueue();
    if (server->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, server->socket_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif 

    return 0;
};

int tcp_server_bind(struct netc_tcp_server* server)
{
    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&server->address;
    socklen_t addrlen = server->addrlen;

    int result = bind(sockfd, addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int tcp_server_listen(struct netc_tcp_server* server)
{
    int sockfd = server->socket_fd;

    int result = listen(sockfd, server->backlog);
    if (result == -1) return netc_error(LISTEN);
    
    return 0;
};

int tcp_server_accept(struct netc_tcp_server* server, struct netc_tcp_client* client)
{
    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&client->address;
    socklen_t* addrlen = (socklen_t*)&client->addrlen;

    int result = accept(sockfd, addr, addrlen);
    if (result == -1) return netc_error(ACCEPT);

    client->socket_fd = result;
    vector_set_index(server->clients, client->socket_fd, client);

    if (server->non_blocking == 0) return 0;
    if (tcp_socket_set_non_blocking(client->socket_fd) != 0) return netc_error(FCNTL);

#ifdef __linux__
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client->socket_fd;
    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, client->socket_fd, &ev) == -1) return netc_error(POLL_FD);
#elif __APPLE__
    struct kevent ev;
    EV_SET(&ev, client->socket_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int tcp_server_send(struct netc_tcp_client* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    int result = send(sockfd, message, msglen, flags);
    if (result == -1) return netc_error(SERVSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int tcp_server_receive(struct netc_tcp_server* server, struct netc_tcp_client* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    int result = recv(sockfd, message, msglen, flags);

    if (result == -1) return netc_error(SERVRECV);
    else if (result == 0) return netc_error(BADRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int tcp_server_close_self(struct netc_tcp_server* server)
{
    int result = close(server->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    netc_tcp_server_listening = 0;

    return 0;
};

int tcp_server_close_client(struct netc_tcp_server* server, struct netc_tcp_client* client, int is_error)
{
    if (server->on_disconnect) server->on_disconnect(server, client, is_error);

    int result = close(client->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    vector_delete(server->clients, client->socket_fd);

    return 0;
};

int tcp_socket_get_flags(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);

    return flags;
};

int tcp_socket_set_non_blocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);
    else if (flags & O_NONBLOCK) return 0; // socket is already non-blocking

    int result = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) return netc_error(FCNTL);

    return 0;
};