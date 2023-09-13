#include "udp/client.h"

#include <stdio.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_udp_client_listening = 0;

int udp_client_main_loop(struct udp_client *client)
{
    /** The client socket should be nonblocking when listening for events. */
    socket_set_non_blocking(client->sockfd);
    netc_udp_client_listening = 1;

    while (netc_udp_client_listening)
    {
#ifdef __linux__
        int pfd = client->pfd;
        struct epoll_event events[1];
        int nev = epoll_wait(pfd, events, 1, -1);
        if (nev == -1) return netc_error(EVCREATE);
#elif _WIN32
        SOCKET sockfd = client->sockfd;
        WSAEVENT event = client->event;

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
        int pfd = client->pfd;
        struct kevent events[1];
        int nev = kevent(pfd, NULL, 0, events, 1, NULL);
        if (nev == -1) return netc_error(EVCREATE);
#endif

        if (netc_udp_client_listening == 0) break;

        for (int i = 0; i < nev; ++i)
        {
#ifdef __linux__
            struct epoll_event ev = events[i];
            socket_t sockfd = ev.data.fd;
            if (ev.events & EPOLLIN && client->on_data != NULL) client->on_data(client, client->data);
#elif _WIN32
            if (events.lNetworkEvents & FD_READ && client->on_data != NULL)
                client->on_data(client, client->data);
#elif __APPLE__
            socket_t sockfd = client->sockfd;
            if (events[i].flags & EVFILT_READ && client->on_data != NULL) client->on_data(client, client->data);
#endif
        };
    };

    return 0;
};

int udp_client_init(struct udp_client *client, struct sockaddr addr, int non_blocking)
{
    if (client == NULL) return -1;

    client->sockaddr = addr;
    int protocol = addr.sa_family;

    client->sockfd = socket(protocol, SOCK_DGRAM, 0);
    if (client->sockfd == -1) return netc_error(SOCKET_C);

    if (non_blocking == 0) return 0;
    if (socket_set_non_blocking(client->sockfd) != 0) return netc_error(FD_CTL);

    /** Register events for a nonblocking socket. */
#ifdef __linux__
    client->pfd = epoll_create1(0);
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client->sockfd;
    if (epoll_ctl(client->pfd, EPOLL_CTL_ADD, client->sockfd, &ev) == -1) return netc_error(POLL_FD);
#elif _WIN32
    client->event = WSACreateEvent();
    if (client->event == WSA_INVALID_EVENT)
    {
        WSACloseEvent(client->event);
        WSACleanup();
        return netc_error(EVCREATE);
    };

    if (WSAEventSelect(client->sockfd, client->pfd, FD_READ) == SOCKET_ERROR)
    {
        WSACloseEvent(client->event);
        WSACleanup();
        return netc_error(EVENT_SELECT);
    };
#elif __APPLE__
    client->pfd = kqueue();
    if (client->pfd == -1) return netc_error(EVCREATE);

    struct kevent ev;
    EV_SET(&ev, client->sockfd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(client->pfd, &ev, 1, NULL, 0, NULL) == -1) return netc_error(POLL_FD);
#endif

    return 0;
};

int udp_client_connect(struct udp_client *client)
{
    socket_t sockfd = client->sockfd;
    struct sockaddr addr = client->sockaddr;
    socklen_t addrlen = sizeof(addr);

    int result = connect(sockfd, &addr, addrlen);
    if (result == -1) return netc_error(CONNECT);

    return 0;
};

int udp_client_send(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t server_addrlen)
{
    socket_t sockfd = client->sockfd;

    int result = sendto(sockfd, message, msglen, flags, server_addr, server_addrlen);
    if (result == -1) netc_error(BADSEND);

    return result;
};

int udp_client_receive(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t *server_addrlen)
{
    socket_t sockfd = client->sockfd;

    int result = recvfrom(sockfd, message, msglen, flags, server_addr, server_addrlen);
    if (result == -1) netc_error(BADRECV);

    return result;
};

int udp_client_close(struct udp_client *client)
{
    socket_t sockfd = client->sockfd;

#ifdef _WIN32
    int result = closesocket(sockfd);
#else
    int result = close(sockfd);
#endif

    if (result == -1) return netc_error(CLOSE);

    netc_udp_client_listening = 0;

    return 0;
};