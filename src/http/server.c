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

    struct http_client *client = malloc(sizeof(struct http_client));
    client->client = malloc(sizeof(struct tcp_client));

    tcp_server_accept(server, client->client);

    printf("[sockfd] %d\n", client->client->sockfd);
    map_set(&http_server->clients, &(socket_t){client->client->sockfd}, client, sizeof(client->client->sockfd));

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, client, http_server->data);
};

static void _tcp_on_data(struct tcp_server *server, socket_t sockfd, void *data)
{
    struct http_server *http_server = data;
    printf("[sockfd data] %d\n", sockfd);
    struct http_client *client = map_get(&http_server->clients, &sockfd, sizeof(sockfd));

    if (client == NULL) printf("what.\n");

    struct http_server_parsing_state current_state = client->server_parsing_state;

    int result = 0;
    if ((result = http_server_parse_request(http_server, client, &current_state)) != 0)
    {
        if (result < 0)
        {
            /** Malformed request. */
            if (http_server->on_malformed_request)
            {
                /** TODO(Altanis): Fix one HTTP request partitioned into two causing two event calls. */
                printf("malformed request: %d\n", result);
                http_server->on_malformed_request(http_server, client, result, http_server->data);
            }
        };

        // > 0 means the http request is incomplete and waiting for incoming data
        printf("WAITING FOR INCOMING DATA...\n");
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

    void (*callback)(struct http_server *server, struct http_client *client, struct http_request request) = http_server_find_route(http_server, path);
    
    if (callback != NULL) callback(http_server, client, current_state.request);
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
    struct http_client *client = map_get(&http_server->clients, &sockfd, sizeof(sockfd));

    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error, http_server->data);
};

int http_server_init(struct http_server *http_server, struct sockaddr address, int backlog)
{
    vector_init(&http_server->routes, 8, sizeof(struct http_route));
    map_init(&http_server->clients, 8);

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

void (*http_server_find_route(struct http_server *server, const char *path))(struct http_server *server, struct http_client *client, struct http_request request)
{
    void (*callback)(struct http_server *server, struct http_client *client, struct http_request request) = NULL;

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

int http_server_send_chunked_data(struct http_server *server, struct http_client *client, char *data, size_t data_length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", data_length);

    socket_t sockfd = client->client->sockfd;

    int send_result = 0;

    if ((send_result = tcp_server_send(sockfd, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (data_length != 0 && ((send_result = tcp_server_send(sockfd, data_length == 0 ? "" : data, data_length, 0)) <= 0)) return send_result;    
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

int http_server_send_response(struct http_server *server, struct http_client *client, struct http_response *response, const char *data, size_t data_length)
{
    socket_t sockfd = client->client->sockfd;

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

        if (!chunked && strcasecmp(name, "Transfer-Encoding") == 0 && strcasecmp(value, "chunked") == 0)
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

// todo: optimize by using msg_peek calls
int http_server_parse_request(struct http_server *server, struct http_client *client, struct http_server_parsing_state *current_state)
{
    socket_t sockfd = client->client->sockfd;

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

parse_start:
    printf("current state: %d\n", current_state->parsing_state);
    errno = 0;
    switch (current_state->parsing_state)
    {
        case -1:
        {
            current_state->parsing_state = REQUEST_PARSING_STATE_METHOD;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_METHOD:
        {
            string_t *method = &current_state->request.method;
            
            if (method->length == 0) sso_string_init(method, "");
            if (method->length < MAX_HTTP_METHOD_LEN)
            {
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, method, " ", 1, MAX_HTTP_METHOD_LEN - method->length + 1);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };
            };

            current_state->parsing_state = REQUEST_PARSING_STATE_PATH;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_PATH:
        {
            string_t *path = &current_state->request.path;
            
            if (path->length == 0) sso_string_init(path, "");
            if (path->length < MAX_HTTP_PATH_LEN)
            {
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, path, " ", 1, MAX_HTTP_PATH_LEN - path->length + 1);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };
            };

            current_state->parsing_state = REQUEST_PARSING_STATE_VERSION;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_VERSION:
        {
            string_t *version = &current_state->request.version;
            
            if (version->length == 0) sso_string_init(version, "");
            if (version->length < MAX_HTTP_VERSION_LEN)
            {
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, version, "\r\n", 1, MAX_HTTP_VERSION_LEN - version->length + 2);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };
            };

            current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_NAME;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_HEADER_NAME:
        {
            /** Test for CRLF. */
            char crlf[2] = {0};
            ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);
            printf("bit bots:\n");
            print_bytes(crlf, 2);

            if (crlf[0] == '\r')
            {
                if (crlf[1] == '\n')
                {
                    if (recv(sockfd, crlf, sizeof(crlf), 0) <= 0) return REQUEST_PARSE_ERROR_RECV;
                
                    if (current_state->content_length == 0) break;

                    current_state->parsing_state = 
                        current_state->content_length == -1 ?
                        REQUEST_PARSING_STATE_CHUNK_SIZE : 
                        REQUEST_PARSING_STATE_BODY;
                    goto parse_start;
                } else return 1;
            };

            struct vector *headers = &current_state->request.headers;
            struct http_header *header = &current_state->header;

            if (headers->size >= MAX_HTTP_HEADER_COUNT) return REQUEST_PARSE_ERROR_TOO_MANY_HEADERS;
            if (header->name.length == 0) sso_string_init(&header->name, "");
            
            if (header->name.length < MAX_HTTP_HEADER_NAME_LEN)
            {
                size_t length = MAX_HTTP_HEADER_NAME_LEN - header->name.length + 2 /** crlf */;
                char *delimiter = length == 1 ? " " : ": ";

                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &header->name, delimiter, 1, length + 2);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };
            };

            printf("[header name]: %s\n", sso_string_get(&header->name));

            current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_VALUE;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_HEADER_VALUE:
        {
            struct vector *headers = &current_state->request.headers;
            struct http_header *header = &current_state->header;

            if (header->value.length == 0) sso_string_init(&header->value, "");
            if (header->value.length < MAX_HTTP_HEADER_VALUE_LEN)
            {
                size_t length = MAX_HTTP_HEADER_VALUE_LEN - current_state->header.value.length + 2 /** crlf */;
                char *delimiter = length == 1 ? "\n" : "\r\n";
                
                ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &header->value, delimiter, 1, length);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };
            };

            if (strcasecmp(sso_string_get(&header->name), "Content-Length") == 0)
            {
                current_state->content_length = atoi(sso_string_get(&header->value));
                if (current_state->content_length > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;
            }
            else if (strcasecmp(sso_string_get(&header->name), "Transfer-Encoding") == 0 && strcasecmp(sso_string_get(&header->value), "chunked") == 0)
            {
                current_state->content_length = -1;
            };

            printf("[header value]: %s\n", sso_string_get(&header->value));

            vector_push(headers, header);
            memset(header, 0, sizeof(*header));

            current_state->parsing_state = REQUEST_PARSING_STATE_HEADER_NAME;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_CHUNK_SIZE:
        {
            if (current_state->request.body == NULL) 
            {
                current_state->chunk_size = -1;
                current_state->request.body = calloc(MAX_HTTP_BODY_LEN + 2 /** crlf */ + 1 /** \0 */, sizeof(char));
            };

            if (current_state->chunk_size == -1)
            {
                size_t preexisting_chunk_length = strlen(current_state->chunk_length);
                size_t length = 16 + 2 - preexisting_chunk_length;
                char *delimiter = length == 1 ? "\n" : "\r\n";

                ssize_t bytes_received_chunk_length = socket_recv_until_fixed(sockfd, current_state->chunk_length + preexisting_chunk_length, length, delimiter, 1);
                printf("bytes recv chunk len");
                print_bytes(current_state->chunk_length, length);
                if (bytes_received_chunk_length <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return REQUEST_PARSE_ERROR_RECV;
                };

                current_state->chunk_size = strtoul(current_state->chunk_length, NULL, 16);
                memset(&current_state->chunk_length, 0, sizeof(current_state->chunk_length));
            };

            if (current_state->chunk_size == 0)
            {
                /** Test for CRLF. */
                char crlf[2] = {0};
                ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);
                if (memcmp(crlf, "\r\n", 2) == 0)
                {
                    if (recv(sockfd, crlf, sizeof(crlf), 0) <= 0) return REQUEST_PARSE_ERROR_RECV;
                    break;
                } else return 1;
            };

            if (current_state->request.body_size + current_state->chunk_size > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;

            current_state->parsing_state = REQUEST_PARSING_STATE_CHUNK_DATA;
            goto parse_start;
        };
        case REQUEST_PARSING_STATE_CHUNK_DATA:
        {
            size_t preexisting_chunk_data = current_state->request.body_size + current_state->incomplete_chunk_data_size;
            size_t length = current_state->chunk_size + 2 - (preexisting_chunk_data - current_state->request.body_size);

            ssize_t bytes_received_chunk_data = recv(sockfd, current_state->request.body + preexisting_chunk_data, length, 0);
            
            if (bytes_received_chunk_data <= 0)
            {
                printf("OH OH OH> %d %d %s?\n", bytes_received_chunk_data, errno, strerror(errno));
                current_state->incomplete_chunk_data_size += bytes_received_chunk_data;
                if (errno == EWOULDBLOCK) return 1;
                else return REQUEST_PARSE_ERROR_RECV;
            } 
            else
            {
                current_state->incomplete_chunk_data_size = 0;
                current_state->request.body_size += bytes_received_chunk_data;
            };

            if (bytes_received_chunk_data < length) return 1;
            else
            {
                printf("[sofarsogood] %s\n", current_state->request.body);
                current_state->request.body_size -= 2 /** crlf */;
                printf("current body size %d\n", current_state->request.body_size);
                current_state->request.body[current_state->request.body_size] = '\0';
                current_state->parsing_state = REQUEST_PARSING_STATE_CHUNK_SIZE;
            };

            printf("[cstr] %s\n", current_state->request.body);
            current_state->chunk_size = -1;

            goto parse_start;
        };
        case REQUEST_PARSING_STATE_BODY:
        {
            if (current_state->request.body == NULL)
                current_state->request.body = calloc(current_state->content_length + 1, sizeof(char));

            size_t preexisting_body_data = current_state->request.body_size;
            size_t length = current_state->content_length - preexisting_body_data;

            ssize_t bytes_received_body = recv(sockfd, current_state->request.body + preexisting_body_data, length, 0);
            
            if (bytes_received_body == -1)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return REQUEST_PARSE_ERROR_RECV;
            } else current_state->request.body_size += bytes_received_body;

            printf("body: %s\n", current_state->request.body);
            printf("bytes_recieved_body vs length: %d vs %d\n", bytes_received_body, length);

            if (bytes_received_body < length) return 1;
            else break;
        };
    };

    return 0;
};

int http_server_close(struct http_server *server)
{
    return tcp_server_close_self(server->server);
};

int http_server_close_client(struct http_server *server, struct http_client *client)
{
    return tcp_server_close_client(server->server, client->client->sockfd, 0);
};