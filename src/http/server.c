#include "http/server.h"
#include "utils/error.h"
#include "utils/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    struct http_request request = {0};

    int result = 0;
    if ((result = http_server_parse_request(http_server, sockfd, &request)) != 0)
    {
        // todo send request 
        switch (result)
        {
            case -1:
                printf("error: failed to receive data\n"); return;
            case -2:
                printf("error: too many headers\n"); return;
            case -3:
                printf("error: timed out\n"); return;
            case -4:
                printf("error: malformed header\n"); return;
        };
    };

    printf("method: %s\n", request.method);
    printf("path: %s\n", request.path);
    printf("version: %s\n", request.version);

    char* path = strdup(request.path);

    /** Check for query strings to parse. */
    char* query_string = strchr(path, '?');
    if (query_string != NULL)
    {
        path[query_string - path] = '\0';

        request.query = malloc(sizeof(struct vector));
        vector_init(request.query, 8, sizeof(struct http_query));

        char* token = strtok(query_string + 1, "&");
        while (token != NULL)
        {
            struct http_query query = {0};
            char* value = strchr(token, '=');
            if (value == NULL) break;

            query.key = strndup(token, value - token);
            query.value = strdup(value + 1);

            vector_push(request.query, &query);

            token = strtok(NULL, "&");
        };
    };

    printf("%s\n", path);
    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request) = http_server_find_route(http_server, path);
    
    if (callback != NULL) callback(http_server, sockfd, request);
    else 
    {
        printf("404\n");
        // check for wildcards
        /** 404 error or user handle uknown route (wildcard). */
    };

    free(path);
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
    http_server->routes = malloc(sizeof(struct map));
    map_init(http_server->routes, 8);

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
    size_t MAX_HTTP_METHOD_LEN = (server->config.max_method_len ? server->config.max_method_len : 7) + 1;
    size_t MAX_HTTP_PATH_LEN = (server->config.max_path_len ? server->config.max_path_len : 2000) + 1;
    size_t MAX_HTTP_VERSION_LEN = (server->config.max_version_len ? server->config.max_version_len : 8) + 1;
    size_t MAX_HTTP_HEADER_NAME_LEN = (server->config.max_header_name_len ? server->config.max_header_name_len : 256) + 1;
    size_t MAX_HTTP_HEADER_VALUE_LEN = (server->config.max_header_value_len ? server->config.max_header_value_len : 4096) + 1;
    size_t MAX_HTTP_HEADER_LEN = (
        server->config.max_header_len ? 
            server->config.max_header_len : 
            (MAX_HTTP_HEADER_NAME_LEN - 1) + (MAX_HTTP_HEADER_VALUE_LEN - 1)
    ) + 1;
    size_t MAX_HTTP_HEADER_COUNT = server->config.max_header_count ? server->config.max_header_count : 24;
    size_t MAX_HTTP_BODY_LEN = (server->config.max_body_len ? server->config.max_body_len : 65536) + 1;
    size_t HTTP_HEADERS_TIMEOUT_SECONDS = server->config.headers_timeout_seconds ? server->config.headers_timeout_seconds : 10;
    size_t HTTP_BODY_TIMEOUT_SECONDS = server->config.body_timeout_seconds ? server->config.body_timeout_seconds : 10;

    request->method = malloc(MAX_HTTP_METHOD_LEN * sizeof(char));
    request->path = malloc(MAX_HTTP_PATH_LEN * sizeof(char));
    request->version = malloc(MAX_HTTP_VERSION_LEN * sizeof(char));

    if (socket_recv_until(sockfd, request->method, MAX_HTTP_METHOD_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->path, MAX_HTTP_PATH_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->version, MAX_HTTP_VERSION_LEN, "\r", 1) <= 0) return -1;

    request->method[strlen(request->method)] = '\0';
    request->path[strlen(request->path)] = '\0';
    request->version[strlen(request->version)] = '\0';

    request->headers = malloc(sizeof(struct vector));
    vector_init(request->headers, 8, sizeof(struct http_header));

    char* buffer = malloc(MAX_HTTP_HEADER_LEN * sizeof(char));
    if (socket_recv_until(sockfd, buffer, 2, "\r\n", 1) <= 0) return -1;

    time_t start_time = time(NULL);
    while (1)
    {
        if (time(NULL) - start_time > HTTP_HEADERS_TIMEOUT_SECONDS) return -3;

        if (socket_recv_until(sockfd, buffer, MAX_HTTP_HEADER_LEN, "\r\n", 1) <= 0) return -1;

        if (buffer[0] == '\0') break;
        if (request->headers->size >= MAX_HTTP_HEADER_COUNT) return -2;

        struct http_header* header = malloc(sizeof(struct http_header));
        header->name = malloc(MAX_HTTP_HEADER_NAME_LEN * sizeof(char));
        header->value = malloc(MAX_HTTP_HEADER_VALUE_LEN * sizeof(char));

        char* value = strchr(buffer, ':');
        if (value == NULL) return -4;

        strncpy(header->name, buffer, value - buffer);
        strncpy(header->value, value + 2, strlen(value));

        header->name[value - buffer] = '\0';
        header->value[strlen(value)] = '\0';

        if (strcmp(header->name, "Content-Length") == 0)
        {
            size_t content_length = strtoul(header->value, NULL, 10);
            if (content_length <= MAX_HTTP_BODY_LEN) MAX_HTTP_BODY_LEN = content_length;
        };

        vector_push(request->headers, header);
        memset(buffer, 0, MAX_HTTP_HEADER_LEN);
    };

    if (tcp_server_receive(sockfd, buffer, MAX_HTTP_BODY_LEN, 0) > 0)
    {
        request->body = malloc(MAX_HTTP_BODY_LEN * sizeof(char));
        strncpy(request->body, buffer, MAX_HTTP_BODY_LEN);
    };

    free(buffer);

    return 0;
};

void http_server_create_route(struct http_server* server, const char* path, void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request))
{
    map_set(server->routes, (void*)path, callback, strlen(path));
};

void (*http_server_find_route(struct http_server* server, const char* path))(struct http_server* server, socket_t sockfd, struct http_request request)
{
    void (*callback)(struct http_server*, socket_t, struct http_request) = map_get(server->routes, (void*)path, strlen(path));
    if (callback != NULL) return callback;

    /** Check for wildcards. */
    for (size_t i = 0; i < server->routes->size; ++i)
    {
        struct map_entry* entry = &server->routes->entries[i];
        if (entry->key == NULL) continue;
    };
};

void http_server_remove_route(struct http_server* server, const char* path)
{
    map_delete(server->routes, (void*)path, strlen(path));
};