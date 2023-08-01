#include "tcp/client.h"
#include "utils/error.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_tcp_client_listening = 1;

int tcp_client_main_loop(struct netc_tcp_client* client)
{
    /** The client socket should be nonblocking when listening for events. */
    tcp_socket_set_non_blocking(client->socket_fd);

    while (netc_tcp_client_listening)
    {
#ifdef __linux__
        int pfd = client->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(EVCREATE);
#elif __APPLE__
        int pfd = client->pfd;
        struct kevent evlist[1];
        int nev = kevent(pfd, NULL, 0, evlist, 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_tcp_client_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            int sockfd = ev.data.fd;

            if (ev.events & EPOLLIN)
                 client->on_data(client);
            else if (ev.events & EPOLLOUT)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (result == -1 || error != 0)
                    return netc_error(CONNECT);
                else
                     client->on_connect(client);

                ev.events = EPOLLOUT;
                if (epoll_ctl(pfd, EPOLL_CTL_DEL, sockfd, &ev) == -1) return netc_error(POLL_FD);
            }
            else if (ev.events & EPOLLERR || ev.events & EPOLLHUP)
            {
                if (tcp_client_close(client, ev.events & EPOLLERR) != 0) return netc_error(CLOSE);
            }
#elif __APPLE__
            struct kevent ev = evlist[i];
            int sockfd = ev.ident;

            if (ev.filter == EVFILT_READ)
                 client->on_data(client);
            else if (ev.filter == EVFILT_WRITE)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (result == -1 || error != 0)
                    return netc_error(CONNECT);
                else
                     client->on_connect(client);

                // deregister event
                EV_SET(&ev, sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
                if (kevent(pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
            }
            else if (ev.flags & EV_ERROR || ev.flags & EV_EOF)
            {
                if (tcp_client_close(client, ev.flags & EV_ERROR) != 0) return netc_error(CLOSE);
            }
#endif
        };
    };

    return 0;
};

int tcp_client_init(struct netc_tcp_client* client, struct netc_tcp_client_config config)
{
    if (client == NULL) return -1; // client is NULL

    int protocol_from = config.ipv6_connect_from ? AF_INET6 : AF_INET;
    int protocol_to = config.ipv6_connect_to ? AF_INET6 : AF_INET;

    client->socket_fd = socket(protocol_from, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (client->socket_fd == -1) return netc_error(SOCKET);

    if (protocol_from == AF_INET6)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&client->address;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(config.port);

        if (inet_pton(protocol_to, config.ip, &addr->sin6_addr) <= 0) return netc_error(INETPTON);
    }
    else
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)&client->address;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(config.port);

        if (inet_pton(protocol_to, config.ip, &addr->sin_addr) <= 0) return netc_error(INETPTON);
    };

    /** The size of the server's address. */
    client->addrlen = sizeof(client->address);

    if (config.non_blocking == 0) return 0; 
    if (tcp_socket_set_non_blocking(client->socket_fd) != 0) return netc_error(FCNTL);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev.data.fd = client->socket_fd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->socket_fd, &ev) == -1) return netc_error(POLL_FD);
#elif __APPLE__
    client->pfd = kqueue();
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev[2];
    int events = 0;
    EV_SET(&ev[events++], client->socket_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    EV_SET(&ev[events++], client->socket_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(client->pfd, ev, events, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int tcp_client_connect(struct netc_tcp_client* client)
{
    int sockfd = client->socket_fd;
    struct sockaddr* addr = &client->address;
    socklen_t addrlen = client->addrlen;

    int result = connect(sockfd, addr, addrlen);
    if (result == -1) return netc_error(CONNECT);

    return 0;
};

int tcp_client_send(struct netc_tcp_client* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = send(sockfd, message, msglen, 0);
    if (result == -1) return netc_error(CLNTSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int tcp_client_receive(struct netc_tcp_client* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = recv(sockfd, message, msglen, 0);
    if (result == -1) return netc_error(CLNTRECV);
    else if (result == 0) return netc_error(BADRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int tcp_client_close(struct netc_tcp_client* client, int is_error)
{
    if (client->on_disconnect) client->on_disconnect(client, is_error);

    int sockfd = client->socket_fd;

    int result = close(sockfd);
    if (result == -1) return netc_error(CLOSE);

    netc_tcp_client_listening = 0;

    return 0;
};