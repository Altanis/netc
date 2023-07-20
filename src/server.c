#include "server.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>

struct netc_server_t* server_init()
{
    struct netc_server_t* server = malloc(sizeof(struct netc_server_t));
    if (server == NULL) error("unable to allocate memory for netc_server_t struct");

    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (server->socket_fd == -1)
    {
        switch (errno)
        {
            case EACCES: error("unable to create socket file descriptor: permission denied"); 
            case EAFNOSUPPORT: error("unable to create socket file descriptor: address family not supported");
            case EINVAL: error("unable to create socket file descriptor: invalid argument");
            case EMFILE: error("unable to create socket file descriptor: too many open files");
            case ENFILE: error("unable to create socket file descriptor: too many open files in system");
            case ENOBUFS: error("unable to create socket file descriptor: no buffer space available");
            case ENOMEM: error("unable to create socket file descriptor: insufficient memory available");
            case EPROTONOSUPPORT: error("unable to create socket file descriptor: protocol not supported");
            default: error("unable to create socket file descriptor");
        };
    }

    /** The family the socket's address belongs to. */
    server->address.sin_family = AF_INET;
    /** Binds to any available IP. */
    server->address.sin_addr.s_addr = INADDR_ANY;
    /** The port in network byte order. */
    server->address.sin_port = htons(PORT);
    /** The size of the server's address. */
    server->addrlen = sizeof(server->address);
    /** The number of clients in the server. */
    server->client_count = 0;

    return server;
};

int server_bind(struct netc_server_t* server)
{
    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&server->address;
    socklen_t addrlen = server->addrlen;

    int result = bind(sockfd, addr, addrlen);
    if (result == -1) return errno;

    return 0;
};

int server_listen(struct netc_server_t* server)
{
    int sockfd = server->socket_fd;

    int result = listen(sockfd, BACKLOG);
    if (result == -1) return errno;

    return 0;
};

int server_accept(struct netc_server_t* server, struct netc_client_t* client)
{
    if (server->client_count >= MAX_CONNECTIONS) error("unable to accept connection: too many connections");

    int sockfd = server->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&client->address;
    socklen_t* addrlen = (socklen_t*)&client->addrlen;

    int result = accept(sockfd, addr, addrlen);
    if (result == -1) return errno;

    client->socket_fd = result;
    server->client_fds[++server->client_count] = result;

    return 0;
};

int server_send_message(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    int result = send(sockfd, message, msglen, flags);
    if (result == -1) return errno;

    return 0;
};

int server_receive_message(struct netc_client_t* client, char* message)
{
    int sockfd = client->socket_fd;
    int flags = 0;

    printf("Wtf\n");
    int result = recv(sockfd, message, MAX_BUFFER_SIZE, flags);
    printf("Wtf2\n"); 

    if (result == -1) return errno;
    else if (result == 0) return -1; // client disconnected

    return 0;
};

int server_close_self(struct netc_server_t* server)
{
    int result = close(server->socket_fd);
    if (result == -1) return errno;

    return 0;
};

int server_close_client(struct netc_server_t* server, struct netc_client_t* client)
{
    int result = close(client->socket_fd);
    if (result == -1) return errno;

    size_t arrlen = sizeof(server->client_fds) / sizeof(server->client_fds[0]);
    for (size_t i = 0; i < arrlen; ++i)
    {
        if (server->client_fds[i] == client->socket_fd)
        {
            server->client_fds[i] = -1;
            break;
        };
    };

    --server->client_count;

    return 0;
};

int client_set_non_blocking(struct netc_client_t* client)
{
    int flags = fcntl(client->socket_fd, F_GETFL, 0);
    if (flags == -1) return errno;
    else if (flags & O_NONBLOCK) return 0; // socket is already non-blocking

    int result = fcntl(client->socket_fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) return errno;

    return 0;
};