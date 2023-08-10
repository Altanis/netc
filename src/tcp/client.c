#include "tcp/client.h"
#include "utils/error.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_tcp_client_listening = 0;

int tcp_client_main_loop(struct netc_tcp_client* client)
{
    /** The client socket should be nonblocking when listening for events. */
    socket_set_non_blocking(client->sockfd);
    netc_tcp_client_listening = 1;

    while (netc_tcp_client_listening)
    {
#ifdef __linux__
        int pfd = client->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(EVCREATE);
#elif _WIN32
        int pfd = client->pfd;
        int nev = WSAWaitForMultipleEvents(1, &pfd, FALSE, -1, FALSE);
        if (nev == WSA_WAIT_FAILED) return netc_error(EVCREATE);
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

            if (ev.events & EPOLLIN && client->on_data != NULL)
                 client->on_data(client);
            else if (ev.events & EPOLLOUT)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (result == -1 || error != 0)
                    return netc_error(CONNECT);
                else if (client->on_connect != NULL)
                     client->on_connect(client);

                ev.events = EPOLLOUT;
                if (epoll_ctl(pfd, EPOLL_CTL_DEL, sockfd, &ev) == -1) return netc_error(POLL_FD);
            }
            else if (ev.events & EPOLLERR || ev.events & EPOLLHUP)
            {
                if (tcp_client_close(client, ev.events & EPOLLERR) != 0) return netc_error(CLOSE);
            }
#elif _WIN32
            SOCKET sockfd = client->sockfd;

            if (WSAEnumNetworkEvents(sockfd, pfd, &client->events) == SOCKET_ERROR) return netc_error(POLL_FD);

            if (client->events.lNetworkEvents & FD_READ && client->on_data != NULL)
                 client->on_data(client);
            else if (client->events.lNetworkEvents & FD_WRITE)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (result == -1 || error != 0)
                    return netc_error(CONNECT);
                else if (client->on_connect != NULL)
                     client->on_connect(client);

                if (WSAResetEvent(pfd) == FALSE) return netc_error(POLL_FD);
            }
            else if (client->events.lNetworkEvents & FD_CLOSE)
            {
                if (tcp_client_close(client, client->events.iErrorCode[FD_CLOSE_BIT]) != 0) return netc_error(CLOSE);
            }
#elif __APPLE__
            struct kevent ev = evlist[i];
            int sockfd = ev.ident;

            if (ev.filter == EVFILT_READ && client->on_data != NULL)
                 client->on_data(client);
            else if (ev.filter == EVFILT_WRITE)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (result == -1 || error != 0)
                    return netc_error(CONNECT);
                else if (client->on_connect != NULL)
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

int tcp_client_init(struct netc_tcp_client* client, int ipv6, int non_blocking)
{
    if (client == NULL) return -1; 
    
    int protocol = ipv6 ? AF_INET6 : AF_INET;

    client->sockfd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (client->sockfd == -1) return netc_error(SOCKET);

    if (non_blocking == 0) return 0; 
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FCNTL);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    client->pfd = WSACreateEvent();
    if (client->pfd == WSA_INVALID_EVENT) return netc_error(EVCREATE);

    if (WSAEventSelect(client->sockfd, client->pfd, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) return netc_error(POLL_FD);
#elif __APPLE__
    client->pfd = kqueue();
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev[2];
    int events = 0;
    EV_SET(&ev[events++], client->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    EV_SET(&ev[events++], client->sockfd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(client->pfd, ev, events, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int tcp_client_connect(struct netc_tcp_client* client, struct sockaddr* addr, socklen_t addrlen)
{
    client->sockaddr = addr;
    client->addrlen = addrlen;

    socket_t sockfd = client->sockfd;

    int result = connect(sockfd, addr, addrlen);
    if (result == -1) return netc_error(CONNECT);

    return 0;
};

int tcp_client_send(struct netc_tcp_client* client, char* message, size_t msglen)
{
    socket_t sockfd = client->sockfd;
    int flags = 0;

    int result = send(sockfd, message, msglen, flags);
    if (result == -1) return netc_error(CLNTSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int tcp_client_receive(struct netc_tcp_client* client, char* message, size_t msglen)
{
    socket_t sockfd = client->sockfd;
    int flags = 0;

    int result = recv(sockfd, message, msglen, flags);
    if (result == -1) return netc_error(CLNTRECV);
    else if (result == 0) return netc_error(BADRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int tcp_client_close(struct netc_tcp_client* client, int is_error)
{
    if (client->on_disconnect != NULL) client->on_disconnect(client, is_error);

    socket_t sockfd = client->sockfd;

    int result = close(sockfd);
    if (result == -1) return netc_error(CLOSE);

    netc_tcp_client_listening = 0;

    return 0;
};