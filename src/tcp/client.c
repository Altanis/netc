#include "tcp/client.h"
#include "utils/error.h"

#include <stdlib.h>
#include <unistd.h>

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

__thread int netc_tcp_client_listening = 0;

int tcp_client_main_loop(struct tcp_client *client)
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
        if (nev == -1) return netc_error(POLL_FD);
#elif _WIN32
        WSAPOLLFD events[1];
        events[0].fd = client->sockfd;
        events[0].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
        int nev = WSAPoll(events, sizeof(events), -1);
        if (nev == -1) return netc_error(POLL_FD);
#elif __APPLE__
        int pfd = client->pfd;
        struct kevent events[1];
        int nev = kevent(pfd, NULL, 0, events, 1, NULL);
        if (nev == -1) return netc_error(POLL_FD);
#endif

        if (netc_tcp_client_listening == 0) break;

#ifdef __linux__
        struct epoll_event ev = events[0];
        socket_t sockfd = ev.data.fd;

        if (ev.events & EPOLLIN && client->on_data != NULL)
                client->on_data(client, client->data);
        else if (ev.events & EPOLLOUT)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

            if (result == -1 || error != 0)
                return netc_error(HANGUP);
            else if (client->on_connect != NULL)
                    client->on_connect(client, client->data);

            ev.events = EPOLLOUT;
            if (epoll_ctl(pfd, EPOLL_CTL_DEL, sockfd, &ev) == -1) return netc_error(POLL_FD);
        }
        else if (ev.events & EPOLLERR || ev.events & EPOLLHUP)
        {
            if (tcp_client_close(client, ev.events & EPOLLERR) != 0) return netc_error(CLOSE);
        }
#elif _WIN32
        WSAPOLLFD event = events[0];
        SOCKET sockfd = event.fd;

        if (event.revents & POLLIN && client->on_data != NULL)
                client->on_data(client, client->data);
        else if (event.revents & POLLOUT)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

            if (result == -1 || error != 0)
                return netc_error(HANGUP);
            else if (client->on_connect != NULL)
                    client->on_connect(client, client->data);
        }
        else if (event.revents & POLLERR || event.revents & POLLHUP)
        {
            if (tcp_client_close(client, event.revents & POLLERR) != 0) return netc_error(CLOSE);
        };
#elif __APPLE__
        struct kevent ev = events[0];
        socket_t sockfd = ev.ident;

        if (ev.filter == EVFILT_READ && client->on_data != NULL)
                client->on_data(client, client->data);
        else if (ev.filter == EVFILT_WRITE)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            int result = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

            if (result == -1 || error != 0)
                return netc_error(HANGUP);
            else if (client->on_connect != NULL)
                    client->on_connect(client, client->data);

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

    return 0;
};

int tcp_client_init(struct tcp_client *client, struct sockaddr addr, int non_blocking)
{
    if (client == NULL) return -1; 
    
    client->sockaddr = addr;
    int protocol = addr.sa_family;

    client->sockfd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (client->sockfd == -1) return netc_error(SOCKET_C);

    if (non_blocking == 0) return 0; 
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FD_CTL);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
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

int tcp_client_connect(struct tcp_client *client)
{
    socket_t sockfd = client->sockfd;
    struct sockaddr addr = client->sockaddr;
    socklen_t addrlen = sizeof(addr);

    int result = connect(sockfd, &addr, addrlen);
    if (result == -1 && errno != EINPROGRESS) return netc_error(CONNECT);

    return 0;
};

int tcp_client_send(struct tcp_client *client, char *message, size_t msglen, int flags)
{
    socket_t sockfd = client->sockfd;

    int result = send(sockfd, message, msglen, flags);
    if (result == -1) netc_error(BADSEND);

    return result;
};

int tcp_client_receive(struct tcp_client *client, char *message, size_t msglen, int flags)
{
    socket_t sockfd = client->sockfd;

    int result = recv(sockfd, message, msglen, flags);
    if (result == -1) netc_error(BADRECV);

    return result;
};

int tcp_client_close(struct tcp_client *client, int is_error)
{
    if (client->on_disconnect != NULL) client->on_disconnect(client, is_error, client->data);

    socket_t sockfd = client->sockfd;
    netc_tcp_client_listening = 0;
    
#ifdef _WIN32
    int result = closesocket(sockfd);
#else
    int result = close(sockfd);
#endif

    if (result == -1) return netc_error(CLOSE);

    return 0;
};