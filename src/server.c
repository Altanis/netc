#include "server.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_running = 1;

int server_main_loop(struct netc_server_t* server)
{
    server->on_ready(server);

    while (is_running)
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
        if (nev == -1)
            return netc_error(EVCREATE);
#endif

        if (is_running == 0)
            break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            int sockfd = ev.data.fd;

            if (sockfd == server->socket_fd)
            {
                if (ev.events & EPOLLERR || ev.events & EPOLLHUP)
                    return netc_error(EVCREATE); // EOF on server socket

                server->on_connect(server);
            }
            else
            {
                struct netc_client_t* client = server->clients->data[sockfd];
                if (client == NULL)
                {
                    printf("netc warn: polling returned an event for a socket that is not in the server's client list. sockfd: %d\n", sockfd);
                    printf("netc warn: this is likely a bug in netc. report this at https://github.com/Altanis/netc \n\n");
                }

                if (ev.events & EPOLLERR || ev.events & EPOLLHUP)
                {
                    if (server_close_client(server, client, ev.events & EPOLLERR) != 0)
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
                if (ev.flags & EV_EOF || ev.flags & EV_ERROR)
                    return netc_error(EVCREATE); // EOF on server socket

                server->on_connect(server);
            }
            else
            {
                struct netc_client_t* client = server->clients->data[sockfd];
                if (client == NULL)
                {
                    printf("netc warn: polling returned an event for a socket that is not in the server's client list. sockfd: %d\n", sockfd);
                    printf("netc warn: this is likely a bug in netc. report this at https://github.com/Altanis/netc \n\n");
                    continue;
                }

                if (ev.flags & EV_EOF || ev.flags & EV_ERROR)
                {
                    if (server_close_client(server, client, ev.flags & EV_ERROR) != 0)
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

int server_init(struct netc_server_t* server, struct netc_server_config config)
{
    if (server == NULL) return -1; // server is NULL
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    server->port = config.port;
    server->backlog = config.backlog;

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
    vector_init(server->clients, 10, sizeof(struct netc_client_t*));

    /** Register event for the server socket. */
#ifdef __linux__
    server->pfd = epoll_create1(0);
    if (server->pfd == -1) return netc_error(EPOLL);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server->socket_fd;
    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, server->socket_fd, &ev) == -1) return netc_error(EVCREATE);
#elif __APPLE__
    server->pfd = kqueue();
    if (server->pfd == -1) return netc_error(POLL);

    struct kevent ev;
    EV_SET(&ev, server->socket_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL);
#endif 

    return 0;
};

int server_bind(struct netc_server_t* server)
{
    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&server->address;
    socklen_t addrlen = server->addrlen;

    int result = bind(sockfd, addr, addrlen);
    if (result == -1) return netc_error(BIND);

    return 0;
};

int server_listen(struct netc_server_t* server)
{
    int sockfd = server->socket_fd;

    int result = listen(sockfd, server->backlog);
    if (result == -1) return netc_error(LISTEN);

    server_main_loop(server);

    return 0;
};

int server_accept(struct netc_server_t* server, struct netc_client_t* client)
{
    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&client->address;
    socklen_t* addrlen = (socklen_t*)&client->addrlen;

    int result = accept(sockfd, addr, addrlen);
    if (result == -1) return netc_error(ACCEPT);

    client->socket_fd = result;
    vector_set_index(server->clients, client->socket_fd, client);

    socket_set_non_blocking(client->socket_fd);

#ifdef __linux__
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client->socket_fd;
    if (epoll_ctl(server->pfd, EPOLL_CTL_ADD, client->socket_fd, &ev) == -1) return netc_error(POLL);
#elif __APPLE__
    struct kevent ev;
    EV_SET(&ev, client->socket_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(server->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(EVCREATE);
#endif

    return 0;
};

int server_send(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    int result = send(sockfd, message, msglen, flags);
    if (result == -1) return netc_error(SERVSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int server_receive(struct netc_server_t* server, struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    int result = recv(sockfd, message, msglen, flags);

    if (result == -1) return netc_error(SERVRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int server_close_self(struct netc_server_t* server)
{
    int result = close(server->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    is_running = 0;

    return 0;
};

int server_close_client(struct netc_server_t* server, struct netc_client_t* client, int is_error)
{
    int result = close(client->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    vector_delete(server->clients, client->socket_fd);

    server->on_disconnect(server, client, is_error);
    return 0;
};

int socket_get_flags(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);

    return flags;
};

int socket_set_non_blocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);
    else if (flags & O_NONBLOCK) return 0; // socket is already non-blocking

    int result = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) return netc_error(FCNTL);

    return 0;
};