#include "client.h"
#include "error.h"

#include <stdlib.h>

struct netc_client_t* client_init()
{
    struct netc_client_t* client = malloc(sizeof(struct netc_client_t));
    if (client == NULL) error("unable to allocate memory for netc_server_t struct");

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (client->socket_fd == -1)
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
    client->address.sin_family = AF_INET;
    /** Binds to any available IP. */
    client->address.sin_addr.s_addr = INADDR_ANY;
    /** The port in network byte order. */
    client->address.sin_port = htons(PORT);
    /** The size of the server's address. */
    client->addrlen = sizeof(client->address);

    return client;
};

int client_connect(struct netc_client_t* client)
{
    int sockfd = client->socket_fd;
    struct sockaddr* addr = (struct sockaddr*)&client->address;
    socklen_t addrlen = client->addrlen;

    int result = connect(sockfd, addr, addrlen);
    if (result == -1) return errno;

    return 0;
};

int client_send_message(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = send(sockfd, message, msglen, 0);
    if (result == -1) return errno;

    return 0;
};

int client_receive_message(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = recv(sockfd, message, msglen, 0);
    if (result == -1) return errno;

    return 0;
};

int client_close(struct netc_client_t* client)
{
    int sockfd = client->socket_fd;

    int result = close(sockfd);
    if (result == -1) return errno;

    return 0;
};