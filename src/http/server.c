#include "http/server.h"
#include "utils/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_http_server_listening = 0;

void __tcp_on_connect__(struct netc_tcp_server* server, void* data)
{
    struct http_server* http_server = data;

    struct netc_tcp_client client;
    tcp_server_accept(server, &client);

    /** TODO(Altanis): Work on TLS/SSL. */

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, &client, http_server->data);
};

void __tcp_on_data__(struct netc_tcp_server* server, socket_t sockfd, void* data)
{
    struct http_server* http_server = data;
    if (http_server->on_request != NULL)
        http_server->on_request(http_server, sockfd, http_server->data);
};

void __tcp_on_disconnect__(struct netc_tcp_server* server, socket_t sockfd, int is_error, void* data)
{
    /** Who asked? */
    struct http_server* http_server = data;
    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error, http_server->data);
};

int http_server_init(struct http_server* http_server, int ipv6, int reuse_addr, struct sockaddr* address, socklen_t addrlen, int backlog)
{
    struct netc_tcp_server tcp_server = {0};
    tcp_server.data = http_server;
    
    int init_result = tcp_server_init(&tcp_server, ipv6, reuse_addr, 1);
    if (init_result != 0) return init_result;

    int bind_result = tcp_server_bind(&tcp_server, address, addrlen);
    if (bind_result != 0) return bind_result;

    int listen_result = tcp_server_listen(&tcp_server, backlog);
    if (listen_result != 0) return listen_result;

    tcp_server.on_connect = __tcp_on_connect__;
    tcp_server.on_data = __tcp_on_data__;
    tcp_server.on_disconnect = __tcp_on_disconnect__;

    http_server->server = tcp_server;

    return 0;
};

int http_server_start(struct http_server* server)
{
    return tcp_server_main_loop(&server->server);
};

int http_server_parse_request(struct http_server* server, socket_t sockfd, struct http_request* request)
{
    if (socket_recv_until(sockfd, request->method, MAX_HTTP_METHOD_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->path, MAX_HTTP_PATH_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->version, MAX_HTTP_VERSION_LEN, "\r", 1) <= 0) return -1;

    request->headers = malloc(sizeof(struct vector));
    if (request->headers == NULL)
    {
        printf("netc ran out of memory while trying to allocate space for a vector.\n");
        return -1;
    };

    vector_init(request->headers, 8, sizeof(struct http_header));

    char* buffer = calloc(MAX_HTTP_HEADER_LEN, sizeof(char));
    if (buffer == NULL) return -1;

    while (socket_recv_until(sockfd, buffer, MAX_HTTP_HEADER_LEN, "\r", 1) > 0)
    {
        struct http_header* header = calloc(1, sizeof(struct http_header));
        header->name = calloc(MAX_HTTP_HEADER_NAME_LEN, sizeof(char));
        header->value = calloc(MAX_HTTP_HEADER_VALUE_LEN, sizeof(char));

        if (header->name == NULL || header->value == NULL) return -1;

        char* value = strchr(buffer, ':');
        if (value == NULL) return -1;

        strncpy(header->name, buffer, value - buffer);
        strncpy(header->value, value + 1, strlen(value) - 1);

        vector_push(request->headers, header);

        memset(buffer, 0, MAX_HTTP_HEADER_LEN);
    };

    return 0;
};