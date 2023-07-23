#include "server.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int server_main_loop(struct netc_server_t* server)
{
    while (1)
    {
        int kq = server->kq;


        /** There are multiple clients and one server socket attached to this kq. */
        struct kevent evlist[server->clients->size + 1];
        int nev = kevent(kq, NULL, 0, evlist, server->clients->size + 1, NULL);
        if (nev == -1) return netc_error(KEVENT);

        printf("number of events: %d\n", nev);
        for (int i = 0; i < nev; ++i)
        {
            struct kevent ev = evlist[i];
            int sockfd = ev.ident;

            /**
             * Check for:
             * - Incoming connection to server socket
             * - Incoming data from client socket
             * - Disconnection from client socket
            */

            if (sockfd == server->socket_fd)
            {
                // incoming connection to server socket
                struct netc_client_t* client = malloc(sizeof(struct netc_client_t));
                
                int result = server_accept(server, client);
                if (result != 0) return netc_error(CLOSE);
            }
            else
            {
                struct netc_client_t* client = server_get_client(server, sockfd);
                char* buf = malloc(18);
                
                int result = server_receive(server, client, buf, 18);
                if (result != 0) return netc_error(SERVRECV);

                printf("server received message: %s\n", buf);
            };
        };
    };
};

struct netc_client_t* server_get_client(struct netc_server_t* server, int sockfd)
{
    for (size_t i = 0; i < server->clients->size; ++i)
    {
        struct netc_client_t* client = server->clients->data[i];
        if (client->socket_fd == sockfd)
            return client;
    };

    return NULL;
};

int server_init(struct netc_server_t* server, struct netc_server_config config)
{
    if (server == NULL) return -1; // server is NULL
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    server->port = config.port;
    server->max_connections = config.max_connections;
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

    server->clients = malloc(sizeof(struct vector));

    /** Register event for the server socket. */
    server->kq = kqueue();
    if (server->kq == -1) return netc_error(KQUEUE);

    struct kevent ev;
    EV_SET(&ev, server->socket_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(server->kq, &ev, 1, NULL, 0, NULL) == -1) return netc_error(KEVENT);

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
    if (server->clients->size >= server->max_connections) return -1; // max connections reached

    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&client->address;
    socklen_t* addrlen = (socklen_t*)&client->addrlen;

    int result = accept(sockfd, addr, addrlen);
    if (result == -1) return netc_error(ACCEPT);

    client->socket_fd = result;
    vector_push(server->clients, client);

    struct kevent ev;
    EV_SET(&ev, client->socket_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    EV_SET(&ev, client->socket_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);

    server->on_connect(server, client);

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

    server->on_message(server, client, message);

    return 0;
};

int server_close_self(struct netc_server_t* server)
{
    int result = close(server->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    return 0;
};

int server_close_client(struct netc_server_t* server, struct netc_client_t* client)
{
    int result = close(client->socket_fd);
    if (result == -1) return netc_error(CLOSE);

    for (size_t i = 0; i < server->clients->size; ++i)
    {
        int* client_fd = server->clients->data[i];
        if (*client_fd == client->socket_fd)
        {
            vector_delete(server->clients, i);
            break;
        };
    };

    return 0;
};

int client_set_non_blocking(struct netc_client_t* client)
{
    int flags = fcntl(client->socket_fd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);
    else if (flags & O_NONBLOCK) return 0; // socket is already non-blocking

    int result = fcntl(client->socket_fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) return netc_error(FCNTL);

    return 0;
};