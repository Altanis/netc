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

int _path_matches(const char* path, const char* pattern) 
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
        {
            /** TODO(Altanis): Fix one HTTP request partitioned into two causing two event calls. */
            printf("malformed request: %d\n", result);
            http_server->on_malformed_request(http_server, sockfd, result, http_server->data);
        }

        return;
    };

    char* path = strdup(sso_string_get(&request.path));

    /** Check for query strings to parse. */
    char* query_string = strchr(path, '?');
    if (query_string != NULL)
    {
        path[query_string - path] = '\0';

        vector_init(&request.query, 8, sizeof(struct http_query));

        char* token = strtok(query_string + 1, "&");
        while (token != NULL)
        {
            struct http_query query = {0};
            sso_string_init(&query.key, "");
            sso_string_init(&query.value, "");

            char* value = strchr(token, '=');
            if (value == NULL) break;

            *value = '\0';

            sso_string_set(&query.key, token);
            sso_string_set(&query.value, value + 1);

            vector_push(&request.query, &query);
        };
    };

    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request) = http_server_find_route(http_server, path);
    
    if (callback != NULL) callback(http_server, sockfd, request);
    else 
    {
        // That comedian...
        const char* notfound_message = "\
        HTTP/1.1 404 Not Found\r\n\
        Content-Type: text/plain\r\n\
        Content-Length: 9\r\
        \r\n\
        Not Found";

        tcp_server_send(sockfd, (char*)notfound_message, strlen(notfound_message), 0);
    };

    free(path);
};

static void _tcp_on_disconnect(struct tcp_server* server, socket_t sockfd, int is_error, void* data)
{
    struct http_server* http_server = data;
    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error, http_server->data);
};

int http_server_init(struct http_server* http_server, struct sockaddr address, int backlog)
{
    vector_init(&http_server->routes, 8, sizeof(struct http_route));

    struct tcp_server* tcp_server = malloc(sizeof(struct tcp_server));
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

int http_server_start(struct http_server* server)
{
    return tcp_server_main_loop(server->server);
};

void http_server_create_route(struct http_server* server, struct http_route* route)
{
    vector_push(&server->routes, route);
};

void (*http_server_find_route(struct http_server* server, const char* path))(struct http_server* server, socket_t sockfd, struct http_request request)
{
    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request) = NULL;

    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct http_route* route = vector_get(&server->routes, i);
        if (_path_matches(path, route->path))
        {
            callback = route->callback;
            break;
        };
    };

    return callback;
};

void http_server_remove_route(struct http_server* server, const char* path)
{
    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct http_route* route = vector_get(&server->routes, i);
        if (strcmp(route->path, path) == 0)
        {
            vector_delete(&server->routes, i);
            break;
        };
    };
};

int http_server_send_chunked_data(struct http_server* server, socket_t sockfd, char* data, size_t length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_server_send(sockfd, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (length != 0 && ((send_result = tcp_server_send(sockfd, length == 0 ? "" : data, length, 0)) <= 0)) return send_result;    
    if ((send_result = tcp_server_send(sockfd, "\r\n", 2, 0)) <= 2) return send_result;

    return 0;
};

int http_server_send_response(struct http_server* server, socket_t sockfd, struct http_response* response, const char* data, size_t data_length)
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
        struct http_header* header = vector_get(&response->headers, i);
        const char* name = sso_string_get(&header->name);
        const char* value = sso_string_get(&header->value);

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

    ssize_t second_send = tcp_server_send(sockfd, (char*)data, data_length, 0);
    if (second_send <= 0) return second_send;

    return first_send + second_send;
};

int http_server_parse_request(struct http_server* server, socket_t sockfd, struct http_request* request)
{
    size_t MAX_HTTP_METHOD_LEN = (server->config.max_method_len ? server->config.max_method_len : 7);
    size_t MAX_HTTP_PATH_LEN = (server->config.max_path_len ? server->config.max_path_len : 2000);
    size_t MAX_HTTP_VERSION_LEN = (server->config.max_version_len ? server->config.max_version_len : 8);
    size_t MAX_HTTP_HEADER_NAME_LEN = (server->config.max_header_name_len ? server->config.max_header_name_len : 256);
    size_t MAX_HTTP_HEADER_VALUE_LEN = (server->config.max_header_value_len ? server->config.max_header_value_len : 8192);
    size_t MAX_HTTP_HEADER_COUNT = server->config.max_header_count ? server->config.max_header_count : 24;
    size_t MAX_HTTP_BODY_LEN = (server->config.max_body_len ? server->config.max_body_len : 65536);

    size_t HTTP_REQUEST_LINE_TIMEOUT_SECONDS = server->config.request_line_timeout_seconds ? server->config.request_line_timeout_seconds : 10;
    size_t HTTP_HEADERS_TIMEOUT_SECONDS = server->config.headers_timeout_seconds ? server->config.headers_timeout_seconds : 10;
    size_t HTTP_BODY_TIMEOUT_SECONDS = server->config.body_timeout_seconds ? server->config.body_timeout_seconds : 10;

    vector_init(&request->headers, 8, sizeof(struct http_header));

    string_t method = {0};
    string_t path = {0};
    string_t version = {0};

    sso_string_init(&method, "");
    sso_string_init(&path, "");
    sso_string_init(&version, "");

    /** The initial time of when parsing starts happening. */
    time_t start_time = time(NULL);
    /** The state of parsing. */
    enum http_request_parsing_states state = -1; 

    while (1)
    {
        if (time(NULL) - start_time > HTTP_REQUEST_LINE_TIMEOUT_SECONDS) return REQUEST_PARSE_ERROR_TIMEOUT;

        NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_METHOD, sockfd, &method, " ", 1, MAX_HTTP_METHOD_LEN + 2, 0);
        NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_PATH, sockfd, &path, " ", 1, MAX_HTTP_PATH_LEN + 2, 0);
        NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_VERSION, sockfd, &version, "\r\n", 1, MAX_HTTP_VERSION_LEN + 2, 0);

        break;
    };
    
    char decoded[path.length];
    http_url_percent_decode((char*)sso_string_get(&path), decoded);

    sso_string_set(&path, decoded);

    request->method = method;
    request->path = path;
    request->version = version;

    int content_length = 0; // < 0 means chunked
    size_t header_count = 0;

    start_time = time(NULL);
    while (1)
    {
        if (time(NULL) - start_time > HTTP_HEADERS_TIMEOUT_SECONDS) return REQUEST_PARSE_ERROR_TIMEOUT;

        char temp_buffer[2] = {0};
        if (recv(sockfd, temp_buffer, 2, MSG_PEEK) <= 0) return REQUEST_PARSE_ERROR_RECV;

        if (temp_buffer[0] == '\r' && temp_buffer[1] == '\n')
        {
            if (recv(sockfd, temp_buffer, 2, 0) <= 0) return REQUEST_PARSE_ERROR_RECV;
            break;
        };

        struct http_header header = {0};
        sso_string_init(&header.name, "");
        sso_string_init(&header.value, "");

        NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_HEADER_NAME, sockfd, &header.name, ": ", 1, MAX_HTTP_HEADER_NAME_LEN + 2, 0);
        NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_HEADER_VALUE, sockfd, &header.value, "\r\n", 1, MAX_HTTP_HEADER_VALUE_LEN + 2, 0);

        if (content_length == 0 && strcmp(sso_string_get(&header.name), "Content-Length") == 0)
        {
            size_t len = strtoul(sso_string_get(&header.value), NULL, 10);
            content_length = len;

            if (len > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;
            else MAX_HTTP_BODY_LEN = len;
        }
        else if (content_length == 0 && strcmp(sso_string_get(&header.name), "Transfer-Encoding") == 0 && strcmp(sso_string_get(&header.value), "chunked") == 0)
            content_length = -1;

        printf("%s: %s\n", sso_string_get(&header.name), sso_string_get(&header.value));

        vector_push(&request->headers, &header);

        if (++header_count > MAX_HTTP_HEADER_COUNT) return REQUEST_PARSE_ERROR_TOO_MANY_HEADERS;
    };

    start_time = time(NULL);
    
    char buffer[MAX_HTTP_BODY_LEN];
    size_t buffer_length = 0;

    if (content_length == 0) return 0;
    else if (content_length == -1)
    {
        state = REQUEST_PARSING_STATE_CHUNK_DATA;
        while (1)
        {
            if (time(NULL) - start_time > HTTP_BODY_TIMEOUT_SECONDS) return REQUEST_PARSE_ERROR_TIMEOUT;

            char chunk_length[18] = {0};

            NETC_HTTP_REQUEST_PARSE(state, REQUEST_PARSING_STATE_CHUNK_SIZE, sockfd, chunk_length, "\r\n", 1, 16 + 2, 1);

            size_t chunk_size = strtoul(chunk_length, NULL, 16);
            if (chunk_size == 0)
            {
                printf("dissolving...\n");
                char temp_buffer[2] = {0};
                if (recv(sockfd, temp_buffer, 2, 0) <= 0)
                {
                    printf("[ser] recv failed, errno: \n", errno);
                    return REQUEST_PARSE_ERROR_RECV;
                };
                break;
            };

            if (buffer_length + chunk_size > MAX_HTTP_BODY_LEN) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;

            char chunk_data[chunk_size + 2];
            if (state == REQUEST_PARSING_STATE_CHUNK_SIZE)
            {
                state = REQUEST_PARSING_STATE_CHUNK_DATA;
                if (recv(sockfd, chunk_data, chunk_size + 2, 0) <= 0) return REQUEST_PARSE_ERROR_RECV;
            };

            memcpy(buffer + buffer_length, chunk_data, chunk_size);
            buffer_length += chunk_size;
        };
    }
    else
    {
        state = REQUEST_PARSING_STATE_BODY;
        size_t bytes_left = MAX_HTTP_BODY_LEN;
        
        while (1)
        {
            if (bytes_left <= 0)
            {
                buffer_length = MAX_HTTP_BODY_LEN;
                break;
            };

            if (time(NULL) - start_time > HTTP_BODY_TIMEOUT_SECONDS) return REQUEST_PARSE_ERROR_TIMEOUT;
            
            ssize_t result = recv(sockfd, buffer + buffer_length, bytes_left, 0);
            if (result == -1) return REQUEST_PARSE_ERROR_RECV;
            else if (result == 0)
            {
                buffer_length = MAX_HTTP_BODY_LEN - bytes_left;
                break;
            };

            bytes_left -= result;
        };

        char temp_buffer[2] = {0};
        if (recv(sockfd, temp_buffer, 2, MSG_PEEK) > 0) return REQUEST_PARSE_ERROR_BODY_TOO_BIG;
    };

    request->body = malloc(buffer_length + 1);
    memcpy(request->body, buffer, buffer_length);
    request->body[buffer_length] = '\0';
    request->body_size = buffer_length;

    return 0;
};

int http_server_close(struct http_server* server)
{
    return tcp_server_close_self(server->server);
};

int http_server_close_client(struct http_server* server, socket_t sockfd)
{
    return tcp_server_close_client(server->server, sockfd, 0);
};