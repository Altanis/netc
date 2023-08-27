#include "http/server.h"
#include "utils/error.h"
#include "utils/map.h"

#include <zlib.h>
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

int _path_matches(const char* path, const char* pattern) 
{
    while (*pattern) 
    {
        if (*pattern == '*') 
        {
            /** Check for end of path. */
            if (*(pattern + 1) == '\0') return 1;

            /** Recursively try all positions for the wildcard match. */
            while (*path) 
            {
                if (_path_matches(path, pattern + 1)) return 1;
                else ++path;
            }
            
            return 0;
        } 
        else
        {
            if (*path != *pattern) return 0;
            
            ++path;
            ++pattern;
        }
    }

    return (*path == '\0');
};

static void _vector_push_str(struct vector* vec, char* string)
{
    for (size_t i = 0; i < strlen(string); ++i)
        vector_push(vec, &string[i]);
};

static void _tcp_on_connect(struct tcp_server* server, void* data)
{
    struct http_server* http_server = data;

    struct tcp_client client;
    tcp_server_accept(server, &client);

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, &client, http_server->data);
};

static void _tcp_on_data(struct tcp_server* server, socket_t sockfd, void* data)
{
    struct http_server* http_server = data;
    struct http_request request = {0};

    int result = 0;
    if ((result = http_server_parse_request(http_server, sockfd, &request)) != 0 && http_server->on_malformed_request != NULL)
    {
        if (http_server->on_malformed_request)
            http_server->on_malformed_request(http_server, sockfd, result, http_server->data);
        return;
    };

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

    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request) = http_server_find_route(http_server, path);
    
    if (callback != NULL) callback(http_server, sockfd, request);
    else 
    {
        struct http_response response = {0};
        response.status_code = 404;
        response.status_message = (char*)http_status_messages[HTTP_STATUS_CODE_404];
        response.version = "1.1";

        char content_length[16] = {0};
        sprintf(content_length, "%lu", strlen(http_status_messages[HTTP_STATUS_CODE_404]));

        struct vector headers = {0};
        vector_init(&headers, 2, sizeof(struct http_header));
        vector_push(&headers, &(struct http_header){"Content-Type", "text/plain"});
        vector_push(&headers, &(struct http_header){"Content-Length", content_length});

        response.headers = &headers;
        response.body = (char*)http_status_messages[HTTP_STATUS_CODE_400];

        http_server_send_response(http_server, sockfd, &response);
    };

    free(path);
};

static void _tcp_on_disconnect(struct tcp_server* server, socket_t sockfd, int is_error, void* data)
{
    struct http_server* http_server = data;
    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error, http_server->data);
};

int http_server_init(struct http_server* http_server, int ipv6, int reuse_addr, struct sockaddr* address, socklen_t addrlen, int backlog)
{
    http_server->routes = malloc(sizeof(struct vector));
    vector_init(http_server->routes, 8, sizeof(struct http_route));

    struct tcp_server tcp_server = {0};
    tcp_server.data = http_server;
    
    int init_result = tcp_server_init(&tcp_server, ipv6, reuse_addr, 1);
    if (init_result != 0) return init_result;

    int bind_result = tcp_server_bind(&tcp_server, address, addrlen);
    if (bind_result != 0) return bind_result;

    int listen_result = tcp_server_listen(&tcp_server, backlog);
    if (listen_result != 0) return listen_result;

    tcp_server.on_connect = _tcp_on_connect;
    tcp_server.on_data = _tcp_on_data;
    tcp_server.on_disconnect = _tcp_on_disconnect;

    http_server->server = tcp_server;

    return 0;
};

int http_server_start(struct http_server* server)
{
    return tcp_server_main_loop(&server->server);
};

void http_server_create_route(struct http_server* server, struct http_route* route)
{
    vector_push(server->routes, route);
};

void (*http_server_find_route(struct http_server* server, const char* path))(struct http_server* server, socket_t sockfd, struct http_request request)
{
    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request) = NULL;

    for (size_t i = 0; i < server->routes->size; ++i)
    {
        struct http_route* route = vector_get(server->routes, i);
        if (_path_matches(path, route->path)) callback = route->callback;
    };

    return callback;
};

void http_server_remove_route(struct http_server* server, const char* path)
{
    for (size_t i = 0; i < server->routes->size; ++i)
    {
        struct http_route* route = vector_get(server->routes, i);
        if (strcmp(route->path, path) == 0)
        {
            vector_delete(server->routes, i);
            break;
        };
    };
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

    request->headers = malloc(sizeof(struct vector));
    vector_init(request->headers, 8, sizeof(struct http_header));

    request->method = malloc(MAX_HTTP_METHOD_LEN * sizeof(char));
    request->path = malloc(MAX_HTTP_PATH_LEN * sizeof(char));
    request->version = malloc(MAX_HTTP_VERSION_LEN * sizeof(char));
    
    if (socket_recv_until(sockfd, request->method, MAX_HTTP_METHOD_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->path, MAX_HTTP_PATH_LEN, " ", 1) <= 0) return -1;
    if (socket_recv_until(sockfd, request->version, MAX_HTTP_VERSION_LEN, "\r", 1) <= 0) return -1;

    request->method[strlen(request->method)] = '\0';
    request->path[strlen(request->path)] = '\0';
    request->version[strlen(request->version)] = '\0';

    char* decoded = http_url_percent_decode(request->path);
    strcpy(request->path, decoded);
    free(decoded);

    int chunked = 0;

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
            else if (content_length > MAX_HTTP_BODY_LEN) return -5;
        } 
        else if (strcmp(header->name, "Transfer-Encoding") == 0 && strcmp(header->value, "chunked") == 0)
        {
            chunked = 1;
        };

        vector_push(request->headers, header);
        memset(buffer, 0, MAX_HTTP_HEADER_LEN);
    };

    if (chunked == 1)
    {
        request->body = malloc(MAX_HTTP_BODY_LEN * sizeof(char));
        size_t body_len = 0;

        while (1)
        {
            if (time(NULL) - start_time > HTTP_BODY_TIMEOUT_SECONDS) return -3;
            if (socket_recv_until(sockfd, buffer, 64, "\r\n", 1) <= 0) return -1;

            size_t chunk_size = strtoul(buffer, NULL, 16);
            if (chunk_size == 0) break;

            body_len += chunk_size;
            if (body_len > MAX_HTTP_BODY_LEN) return -5;

            if (socket_recv_until(sockfd, request->body, chunk_size, "\r\n", 1) <= 0) return -1;
            if (socket_recv_until(sockfd, buffer, 2, "\r\n", 1) <= 0) return -1;

            memset(buffer, 0, 64);
        };
    }
    else
    {
        if (tcp_server_receive(sockfd, buffer, MAX_HTTP_BODY_LEN, 0) > 0)
        {
            request->body = malloc(MAX_HTTP_BODY_LEN * sizeof(char));
            strncpy(request->body, buffer, MAX_HTTP_BODY_LEN);
        };
    }

    free(buffer);

    return 0;
};

int http_server_send_chunked_data(struct http_server* server, socket_t sockfd, char* data)
{
    size_t length = strlen(data);
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_server_send(sockfd, length_str, strlen(length_str), 0)) != strlen(length_str)) return send_result;
    if (length != 0 && (send_result = tcp_server_send(sockfd, data, length, 0)) != length) return send_result;

    return 0;
};

int http_server_send_response(struct http_server* server, socket_t sockfd, struct http_response* response)
{
    struct vector response_str = {0};
    vector_init(&response_str, 128, sizeof(char));

    _vector_push_str(&response_str, response->version ? response->version : "HTTP/1.1");
    vector_push(&response_str, " ");

    char status_code[4] = {0};
    sprintf(status_code, "%d", response->status_code ? response->status_code : 200);
    _vector_push_str(&response_str, status_code);
    vector_push(&response_str, " ");

    _vector_push_str(&response_str, (char*)(response->status_message ? response->status_message : http_status_messages[HTTP_STATUS_CODE_200]));
    _vector_push_str(&response_str, "\r\n");

    int chunked = 0;

    if (response->headers != NULL)
    {
        for (size_t i = 0; i < response->headers->size; ++i)
        {
            struct http_header* header = vector_get(response->headers, i);
            if (!chunked && strcmp(header->name, "Transfer-Encoding") == 0 && strcmp(header->value, "chunked") == 0)
                chunked = 1;

            _vector_push_str(&response_str, header->name);
            _vector_push_str(&response_str, ": ");
            _vector_push_str(&response_str, header->value);
            _vector_push_str(&response_str, "\r\n");
        };
    }

    _vector_push_str(&response_str, "\r\n");

    if (!chunked && response->body != NULL)
    {
        _vector_push_str(&response_str, response->body);
    }

    char full_response_str[response_str.size];
    for (size_t i = 0; i < response_str.size; ++i)
        full_response_str[i] = *(char*)vector_get(&response_str, i);

    return tcp_server_send(sockfd, full_response_str, response_str.size, 0);
};

int http_server_close(struct http_server* server)
{
    return tcp_server_close_self(&server->server);
};