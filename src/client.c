#include "client.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>

int client_init(struct netc_client_t* client, struct netc_client_config config)
{
    if (client == NULL) return -1; // client is NULL
    int protocol = config.ipv6 ? AF_INET6 : AF_INET;

    client->socket_fd = socket(protocol, SOCK_STREAM, 0); // IPv4, TCP, 0
    if (client->socket_fd == -1) return netc_error(SOCKET);

    if (protocol == AF_INET6)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&client->address;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(config.port);
        addr->sin6_addr = in6addr_any;
    }
    else
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)&client->address;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(config.port);
        addr->sin_addr.s_addr = INADDR_ANY;
    };

    /** The size of the server's address. */
    client->addrlen = sizeof(client->address);

    return 0;
};

int client_connect(struct netc_client_t* client)
{
    int sockfd = client->socket_fd;
    struct sockaddr* addr = &client->address;
    socklen_t addrlen = client->addrlen;
    
    int result = connect(sockfd, addr, addrlen);
    if (result == -1) return netc_error(CONNECT);

    return 0;
};

int client_send(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = send(sockfd, message, msglen, 0);
    if (result == -1) return netc_error(CLNTSEND);
    else if (result != msglen) return -(msglen - result); // message was not sent in full

    return 0;
};

int client_receive(struct netc_client_t* client, char* message, size_t msglen)
{
    int sockfd = client->socket_fd;

    int result = recv(sockfd, message, msglen, 0);
    if (result == -1) return netc_error(CLNTRECV);
    else if (result != msglen) return -(msglen - result); // message was not received in full

    return 0;
};

int client_close(struct netc_client_t* client)
{
    int sockfd = client->socket_fd;

    int result = close(sockfd);
    if (result == -1) return netc_error(CLOSE);

    return 0;
};