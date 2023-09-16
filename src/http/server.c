#include "http/server.h"
#include "utils/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

__thread int netc_http_server_listening = 0;

int _path_matches(const char *path, const char *pattern) 
{
    if (strcmp(path, pattern) == 0) return 1;

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

static void _tcp_on_connect(struct tcp_server *server, void *data)
{
    struct http_server *http_server = data;

    struct tcp_client client;
    tcp_server_accept(server, &client);

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, &client, http_server->data);
};

static void _tcp_on_data(struct tcp_server *server, socket_t sockfd, void *data)
{
    struct http_server *http_server = data;
    struct http_server_parsing_state current_state = http_server->parsing_state;

    int result = 0;
    if ((result = http_server_parse_request(http_server, sockfd, &current_state)) != 0)
    {
        if (result < 0)
        {
            /** Malformed request. */
            if (http_server->on_malformed_request)
            {
                /** TODO(Altanis): Fix one HTTP request partitioned into two causing two event calls. */
                printf("malformed request: %d\n", result);
                http_server->on_malformed_request(http_server, sockfd, result, http_server->data);
            }
        };

        // > 0 means the http request is incomplete and waiting for incoming data
        return;
    };

    printf("finished parsing request.\n");
    printf("method: %s\n", sso_string_get(&current_state.request.method));
    printf("path: %s\n", sso_string_get(&current_state.request.path));
    printf("version: %s\n", sso_string_get(&current_state.request.version));

    for (size_t i = 0; i < current_state.request.headers.size; ++i)
    {
        struct http_header *header = vector_get(&current_state.request.headers, i);
        printf("header: %s: %s\n", sso_string_get(&header->name), sso_string_get(&header->value));
    };

    printf("body: %s\n", current_state.request.body);

    char *path = strdup(sso_string_get(&current_state.request.path));

    /** Check for query strings to parse. */
    char *query_string = strchr(path, '?');
    if (query_string != NULL)
    {
        path[query_string - path] = '\0';

        vector_init(&current_state.request.query, 8, sizeof(struct http_query));

        char *token = strtok(query_string + 1, "&");
        while (token != NULL)
        {
            struct http_query query = {0};
            sso_string_init(&query.key, "");
            sso_string_init(&query.value, "");

            char *value = strchr(token, '=');
            if (value == NULL) break;

            *value = '\0';

            sso_string_set(&query.key, token);
            sso_string_set(&query.value, value + 1);

            vector_push(&current_state.request.query, &query);
        };
    };

    void (*callback)(struct http_server *server, socket_t sockfd, struct http_request request) = http_server_find_route(http_server, path);
    
    if (callback != NULL) callback(http_server, sockfd, current_state.request);
    else 
    {
        // That comedian...
        const char *notfound_message = "\
        HTTP/1.1 404 Not Found\r\n\
        Content-Type: text/plain\r\n\
        Content-Length: 9\r\
        \r\n\
        Not Found";

        tcp_server_send(sockfd, (char*)notfound_message, strlen(notfound_message), 0);
    };

    memset(&current_state.request, 0, sizeof(current_state.request));
    current_state.parsing_state = -1;

    free(path);
};

static void _tcp_on_disconnect(struct tcp_server *server, socket_t sockfd, int is_error, void *data)
{
    struct http_server *http_server = data;
    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error, http_server->data);
};

int http_server_init(struct http_server *http_server, struct sockaddr address, int backlog)
{
    vector_init(&http_server->routes, 8, sizeof(struct http_route));

    struct tcp_server *tcp_server = malloc(sizeof(struct tcp_server));
    tcp_server->data = http_server;
    
    int init_result = tcp_server_init(tcp_server, address, 1);
    if (init_result != 0) return init_result;

    if (setsockopt(tcp_server->sockfd, SOL_SOCKET, SO_REUSEADDR, &(char){1}, sizeof(int)) < 0)
    {
        /** Not essential. Do not return -1. */
    };

    int bind_result = tcp_server_bind(tcp_server);
    if (bind_result != 0) return bind_result;

    int listen_result = tcp_server_listen(tcp_server, backlog);
    if (listen_result != 0) return listen_result;

    tcp_server->on_connect = _tcp_on_connect;
    tcp_server->on_data = _tcp_on_data;
    tcp_server->on_disconnect = _tcp_on_disconnect;

    http_server->server = tcp_server;
    memset(&http_server->parsing_state, 0, sizeof(http_server->parsing_state));

    return 0;
};

int http_server_start(struct http_server *server)
{
    return tcp_server_main_loop(server->server);
};

void http_server_create_route(struct http_server *server, struct http_route *route)
{
    vector_push(&server->routes, route);
};

void (*http_server_find_route(struct http_server *server, const char *path))(struct http_server *server, socket_t sockfd, struct http_request request)
{
    void (*callback)(struct http_server *server, socket_t sockfd, struct http_request request) = NULL;

    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct http_route *route = vector_get(&server->routes, i);
        if (_path_matches(path, route->path))
        {
            callback = route->callback;
            break;
        };
    };

    return callback;
};

void http_server_remove_route(struct http_server *server, const char *path)
{
    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct http_route *route = vector_get(&server->routes, i);
        if (strcmp(route->path, path) == 0)
        {
            vector_delete(&server->routes, i);
            break;
        };
    };
};

int http_server_send_chunked_data(struct http_server *server, socket_t sockfd, char *data, size_t length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_server_send(sockfd, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (length != 0 && ((send_result = tcp_server_send(sockfd, length == 0 ? "" : data, length, 0)) <= 0)) return send_result;    
    if ((send_result = tcp_server_send(sockfd, "\r\n", 2, 0)) <= 2) return send_result;

    // combine them all into one char buffer
    // char buffer[length + strlen(length_str) + 2];
    // memcpy(buffer, length_str, strlen(length_str));
    // memcpy(buffer + strlen(length_str), data, length);
    // memcpy(buffer + strlen(length_str) + length, "\r\n", 2);

    // if ((send_result = tcp_server_send(sockfd, buffer, length + strlen(length_str) + 2, 0)) <= 0) return send_result;
    // printf("sent ");
    // print_bytes(buffer, length + strlen(length_str) + 2);

    return 0;
};

int http_server_send_response(struct http_server *server, socket_t sockfd, struct http_response *response, const char *data, size_t data_length)
{
    string_t response_str = {0};
    sso_string_init(&response_str, "");

    sso_string_concat_buffer(&response_str, sso_string_get(&response->version));
    sso_string_concat_char(&response_str, ' ');

    char status_code[4] = {0};
    sprintf(status_code, "%d", response->status_code);
    sso_string_concat_buffer(&response_str, status_code);
    sso_string_concat_char(&response_str, ' ');

    sso_string_concat_buffer(&response_str, sso_string_get(&response->status_message));
    sso_string_concat_buffer(&response_str, "\r\n");

    int chunked = 0;

    for (size_t i = 0; i < response->headers.size; ++i)
    {
        struct http_header *header = vector_get(&response->headers, i);
        const char *name = sso_string_get(&header->name);
        const char *value = sso_string_get(&header->value);

        if (!chunked && strcmp(name, "Transfer-Encoding") == 0 && strcmp(value, "chunked") == 0)
            chunked = 1;
        else if (!chunked && strcmp(name, "Content-Length") == 0)
            chunked = -1;

        sso_string_concat_buffer(&response_str, name);
        sso_string_concat_buffer(&response_str, ": ");
        sso_string_concat_buffer(&response_str, value);
        sso_string_concat_buffer(&response_str, "\r\n");
    };

    if (chunked == 0)
    {
        char length_str[16] = {0};
        sprintf(length_str, "%zu", data_length);

        sso_string_concat_buffer(&response_str, "Content-Length: ");
        sso_string_concat_buffer(&response_str, length_str);
        sso_string_concat_buffer(&response_str, "\r\n");
    };

    sso_string_concat_buffer(&response_str, "\r\n");

    ssize_t first_send = tcp_server_send(sockfd, (char*)sso_string_get(&response_str), response_str.length, 0);
    if (first_send <= 0) return first_send;

    ssize_t second_send = 0;

    if (data_length > 0)
    {
        second_send = tcp_server_send(sockfd, (char*)data, data_length, 0);
        if (second_send <= 0) return second_send;
    };

    return first_send + second_send;
};

int http_server_parse_request(struct http_server *server, socket_t sockfd, struct http_server_parsing_state *current_state)
{
    char boffy[4096] = {0};
    recv(sockfd, boffy, 4095, MSG_PEEK);
    printf("boffy: %s\n", boffy);

    size_t MAX_HTTP_METHOD_LEN = (server->config.max_method_len ? server->config.max_method_len : 7);
    size_t MAX_HTTP_PATH_LEN = (server->config.max_path_len ? server->config.max_path_len : 2000);
    size_t MAX_HTTP_VERSION_LEN = (server->config.max_version_len ? server->config.max_version_len : 8);
    size_t MAX_HTTP_HEADER_NAME_LEN = (server->config.max_header_name_len ? server->config.max_header_name_len : 256);
    size_t MAX_HTTP_HEADER_VALUE_LEN = (server->config.max_header_value_len ? server->config.max_header_value_len : 4096);
    size_t MAX_HTTP_HEADER_COUNT = server->config.max_header_count ? server->config.max_header_count : 24;
    size_t MAX_HTTP_BODY_LEN = (server->config.max_body_len ? server->config.max_body_len : 65536);

    size_t HTTP_REQUEST_LINE_TIMEOUT_SECONDS = server->config.request_line_timeout_seconds ? server->config.request_line_timeout_seconds : 10;
    size_t HTTP_HEADERS_TIMEOUT_SECONDS = server->config.headers_timeout_seconds ? server->config.headers_timeout_seconds : 10;
    size_t HTTP_BODY_TIMEOUT_SECONDS = server->config.body_timeout_seconds ? server->config.body_timeout_seconds : 10;

    vector_init(&current_state->request.headers, 8, sizeof(struct http_header));

    if (current_state->parsing_state == -1)
        current_state->parsing_state = REQUEST_PARSING_STATE_METHOD;
    
    if (current_state->parsing_state == REQUEST_PARSING_STATE_METHOD)
    {
        if (current_state->request.method.length == 0) sso_string_init(&current_state->request.method, "");

        if (current_state->request.method.length < MAX_HTTP_METHOD_LEN)
        {
            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &current_state->request.method, " ", 1, MAX_HTTP_METHOD_LEN - current_state->request.method.length + 1);

            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return -700;
            };
        };

        current_state->parsing_state = REQUEST_PARSING_STATE_PATH;
    };

    errno = 0;
    
    if (current_state->parsing_state == REQUEST_PARSING_STATE_PATH)
    {
        if (current_state->request.path.length == 0) sso_string_init(&current_state->request.path, "");

        if (current_state->request.path.length < MAX_HTTP_PATH_LEN)
        {
            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &current_state->request.path, " ", 1, MAX_HTTP_PATH_LEN - current_state->request.path.length + 1);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return -800;
            };
        };

        current_state->parsing_state = REQUEST_PARSING_STATE_VERSION;
    };

    errno = 0;

    if (current_state->parsing_state == REQUEST_PARSING_STATE_VERSION)
    {
        if (current_state->request.version.length == 0) sso_string_init(&current_state->request.version, "");

        if (current_state->request.version.length < MAX_HTTP_VERSION_LEN)
        {
            char *bytes = (MAX_HTTP_VERSION_LEN - current_state->request.version.length) == 1 ? "\n" : "\r\n";
            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &current_state->request.version, bytes, 1, MAX_HTTP_VERSION_LEN - current_state->request.version.length + 2);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return -900;
            };
        };

        current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_NAME;
    };

    printf("current val: %s\n", sso_string_get(&current_state->request.version));

    errno = 0;

    while (current_state->parsing_state == REQUEST_PARSING_STATE_HEADER_NAME || current_state->parsing_state == REQUEST_PARSING_STATE_HEADER_VALUE)
    {
        if (current_state->parsing_state == REQUEST_PARSING_STATE_HEADER_NAME)
        {
            /** Test for CRLF (two CRLFs = end of headers). */
            char crlf[2] = {0};

            if (current_state->parsed_crlf == 0)
            {
                ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);
                if (check_crlf == 1 && crlf[0] == '\r')
                {
                    printf("out too hard.\n");
                    if (recv(sockfd, crlf, 1, 0) <= 0) return -1000;
                    current_state->parsed_crlf = 1;
                    return 1;
                }
                else if (check_crlf == 2 && crlf[0] == '\r' && crlf[1] == '\n')
                {
                    printf("die.\n");
                    if (recv(sockfd, crlf, 2, 0) <= 0) return -1100;
                    current_state->parsed_crlf = 2;
                    break;
                };
            }
            else if (current_state->parsed_crlf == 1)
            {
                ssize_t check_crlf = recv(sockfd, crlf, 1, MSG_PEEK);
                if (check_crlf == 1 && crlf[0] == '\n')
                {
                    printf("memories.\n");
                    if (recv(sockfd, crlf, 1, 0) <= 0) return -1200;
                    current_state->parsed_crlf = 2;
                    return 1;
                };
            };

            if (current_state->request.headers.size >= MAX_HTTP_HEADER_COUNT) return REQUEST_PARSE_ERROR_TOO_MANY_HEADERS;

            if (current_state->header.name.length == 0) sso_string_init(&current_state->header.name, "");

            if (current_state->header.name.length < MAX_HTTP_HEADER_NAME_LEN)
            {
                char *bytes = (MAX_HTTP_HEADER_NAME_LEN - current_state->header.name.length) == 1 ? " " : ": ";
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &current_state->header.name, bytes, 1, MAX_HTTP_HEADER_NAME_LEN - current_state->header.name.length + 2);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return -11;
                };

                printf("header name: %s\n", sso_string_get(&current_state->header.name));
                
                current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_VALUE;
            };

            errno = 0;
        }
        else if (current_state->parsing_state == REQUEST_PARSING_STATE_HEADER_VALUE)
        {
            if (current_state->header.value.length == 0) sso_string_init(&current_state->header.value, "");

            if (current_state->header.value.length < MAX_HTTP_HEADER_VALUE_LEN)
            {
                char *bytes = (MAX_HTTP_HEADER_VALUE_LEN - current_state->header.value.length) == 1 ? "\n" : "\r\n";
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &current_state->header.value, bytes, 1, MAX_HTTP_HEADER_VALUE_LEN - current_state->header.value.length + 2);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return -22;
                };
            };

            errno = 0;

            if (strcmp(sso_string_get(&current_state->header.name), "Content-Length") == 0)
            {
                int len = atoi(sso_string_get(&current_state->header.value));
                current_state->content_length = len;

                if (current_state->content_length > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;
            }
            else if (strcmp(sso_string_get(&current_state->header.name), "Transfer-Encoding") == 0 && strcmp(sso_string_get(&current_state->header.value), "chunked") == 0)
            {
                current_state->content_length = -1;
                printf("curstate conlen %d\n", current_state->content_length);
            };

            vector_push(&current_state->request.headers, &current_state->header);

            current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_NAME;
            memset(&current_state->header, 0, sizeof(current_state->header));
        };
    };

    if (current_state->content_length == 0) return 0;
    else if (current_state->content_length == -1)
    {
        if (current_state->request.body == NULL)
        {
            current_state->parsing_state = RESPONSE_PARSING_STATE_CHUNK_SIZE;
            current_state->request.body = calloc(MAX_HTTP_BODY_LEN + 2 /** crlf */ + 1 /** null term */, sizeof(char));
        };

        while (current_state->parsing_state == RESPONSE_PARSING_STATE_CHUNK_SIZE || current_state->parsing_state == RESPONSE_PARSING_STATE_CHUNK_DATA)
        {
            if (current_state->parsing_state == RESPONSE_PARSING_STATE_CHUNK_SIZE)
            {
                size_t chunk_size = strlen(current_state->chunk_length);
                char *bytes = ((16 + 2) - chunk_size) == 1 ? "\n" : "\r\n";

                char bofly[4096];
                recv(sockfd, bofly, 4095, MSG_PEEK);
                printf("BOFLY:\n");
                print_bytes(bofly, 4095);

                ssize_t bytes_received_chunk_len = socket_recv_until_fixed(sockfd, current_state->chunk_length + chunk_size, (16 + 2 /** 16 digits + crlf */) - chunk_size, bytes, 1);
                if (bytes_received_chunk_len <= 0)
                {
                    printf("fire no. result: %d\n", bytes_received_chunk_len);
                    if (errno == EWOULDBLOCK) return 1;
                    else return -33;
                };

                current_state->chunk_size = strtoul(current_state->chunk_length, NULL, 16);
                memset(&current_state->chunk_length, 0, sizeof(current_state->chunk_length));
                printf("chunk size! %d\n", current_state->chunk_size);

                if (current_state->request.body_size + current_state->chunk_size > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;
                
                errno = 0;
            };

            current_state->parsing_state = RESPONSE_PARSING_STATE_CHUNK_DATA;

            if (current_state->chunk_size == 0)
            {
                char crlf[2] = {0};

                if (current_state->parsed_crlf == 2)
                {
                    ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);
                    if (check_crlf == 1 && crlf[0] == '\r')
                    {
                        printf("but here i am.\n");
                        if (recv(sockfd, crlf, 1, 0) <= 0) return -44;
                        current_state->parsed_crlf = 3;
                        return 1;
                    }
                    else if (check_crlf == 2 && crlf[0] == '\r' && crlf[1] == '\n')
                    {
                        printf("knowing theres nowhere to go.\n");
                        if (recv(sockfd, crlf, 2, 0) <= 0) return -55;
                        current_state->parsed_crlf = 4;
                    };
                }
                else if (current_state->parsed_crlf == 3)
                {
                    printf("farmland.\n");
                    ssize_t check_crlf = recv(sockfd, crlf, 1, MSG_PEEK);
                    if (check_crlf == 1 && crlf[0] == '\n')
                    {
                        if (recv(sockfd, crlf, 1, 0) <= 0) return -66;
                        current_state->parsed_crlf = 4;
                        return 1;
                    };
                };

                if (current_state->parsed_crlf == 4) break;
            };

            if (current_state->parsing_state == RESPONSE_PARSING_STATE_CHUNK_DATA)
            {
                char *bytes = (current_state->chunk_size - current_state->request.body_size) == 1 ? "\n" : "\r\n";
                printf("buffer ptr: %p\n", current_state->request.body + current_state->request.body_size);

                int bytes_to_recv = current_state->chunk_size + 2;
                printf("going to recv %d\n", bytes_to_recv);
                ssize_t bytes_received_chunk_data = socket_recv_until_fixed(sockfd, current_state->request.body + current_state->request.body_size, bytes_to_recv, NULL, 0);
                if (bytes_received_chunk_data <= 0)
                {
                    printf("errno: [%d]\n", errno);
                    if (errno == EWOULDBLOCK) return 1;
                    else return -77;
                } else printf("wrote to buffer.\n\n\nnew content: %s\n", current_state->request.body);

                current_state->request.body[current_state->request.body_size + bytes_received_chunk_data - 1] = '\0';

                current_state->request.body_size += bytes_received_chunk_data;
                if (bytes_received_chunk_data == bytes_to_recv) current_state->parsing_state = RESPONSE_PARSING_STATE_CHUNK_SIZE;
            };
        };
    }
    else
    {
        if (current_state->request.body == NULL)
        {
            current_state->parsing_state = RESPONSE_PARSING_STATE_BODY;
            current_state->request.body = calloc(current_state->content_length + 1, sizeof(char));
        };

        current_state->parsing_state = REQUEST_PARSING_STATE_BODY;

        ssize_t bytes_received_body = socket_recv_until_fixed(sockfd, current_state->request.body + current_state->request.body_size, current_state->content_length - current_state->request.body_size + 2, NULL, 0);
        if (bytes_received_body <= 0)
        {
            if (errno == EWOULDBLOCK) return 1;
            else return -78;
        };
        current_state->request.body_size += bytes_received_body;
    };

    return 0;
};

int http_server_close(struct http_server *server)
{
    return tcp_server_close_self(server->server);
};

int http_server_close_client(struct http_server *server, socket_t sockfd)
{
    return tcp_server_close_client(server->server, sockfd, 0);
};