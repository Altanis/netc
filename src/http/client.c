#include "http/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _vector_push_str(struct vector* vec, char* string)
{
    for (size_t i = 0; i < strlen(string); ++i)
        vector_push(vec, &string[i]);
};

static void _tcp_on_connect(struct tcp_client* client, void* data)
{
    struct http_client* http_client = (struct http_client*)data;
    if (http_client->on_connect != NULL)
        http_client->on_connect(http_client, http_client->data);
};

static void _tcp_on_data(struct tcp_client* client, void* data)
{
    struct http_client* http_client = (struct http_client*)data;
    struct http_response response = {0};

    int result = 0;
    if ((result = http_client_parse_response(http_client, &response)) != 0)
    {
        if (http_client->on_malformed_response != NULL)
            http_client->on_malformed_response(http_client, result, http_client->data);
        return;
    };

    if (http_client->on_data != NULL)
        http_client->on_data(http_client, response, http_client->data);
};

static void _tcp_on_disconnect(struct tcp_client* client, int is_error, void* data)
{
    struct http_client* http_client = (struct http_client*)data;
    if (http_client->on_disconnect != NULL)
        http_client->on_disconnect(http_client, is_error, http_client->data);
};

int http_client_init(struct http_client* client, int ipv6, struct sockaddr* address, socklen_t addrlen)
{
    struct tcp_client tcp_client = {0};
    tcp_client.data = client;

    int init_result = tcp_client_init(&tcp_client, ipv6, 1);
    if (init_result != 0) return init_result;

    int connect_result = tcp_client_connect(&tcp_client, address, addrlen);
    if (connect_result != 0) return connect_result;

    tcp_client.on_connect = _tcp_on_connect;
    tcp_client.on_data = _tcp_on_data;
    tcp_client.on_disconnect = _tcp_on_disconnect;

    client->client = tcp_client;

    return 0;
};

int http_client_start(struct http_client* client)
{
    return tcp_client_main_loop(&client->client);
};

int http_client_send_chunked_data(struct http_client* client, char* data)
{
    size_t length = strlen(data);
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_client_send(&client->client, length_str, strlen(length_str), 0)) != strlen(length_str)) return send_result;
    if (length != 0 && (send_result = tcp_client_send(&client->client, data, length, 0)) != length) return send_result;

    return 0;
};

int http_client_send_request(struct http_client* client, struct http_request* request)
{
    struct vector request_str = {0};
    vector_init(&request_str, 128, sizeof(char));

    _vector_push_str(&request_str, request->method);
    _vector_push_str(&request_str, " ");
    _vector_push_str(&request_str, request->path);
    _vector_push_str(&request_str, " ");
    _vector_push_str(&request_str, request->version);
    _vector_push_str(&request_str, "\r\n");

    int chunked = 0;

    if (request->headers != NULL)
    {
        for (size_t i = 0; i < request->headers->size; ++i)
        {
            struct http_header* header = vector_get(request->headers, i);
            if (!chunked && strcmp(header->name, "Transfer-Encoding") == 0 && strcmp(header->value, "chunked") == 0)
                chunked = 1;

            _vector_push_str(&request_str, header->name);
            _vector_push_str(&request_str, ": ");
            _vector_push_str(&request_str, header->value);
            _vector_push_str(&request_str, "\r\n");
        };
    };

    _vector_push_str(&request_str, "\r\n");

    if (!chunked && request->body != NULL)
    {
        _vector_push_str(&request_str, request->body);
    };
    
    char request_str_c[request_str.size];
    for (size_t i = 0; i < request_str.size; ++i)
        request_str_c[i] = *(char*)vector_get(&request_str, i);
    
    return tcp_client_send(&client->client, request_str_c, request_str.size, 0);
};

int http_client_parse_response(struct http_client* client, struct http_response* response)
{
    response->headers = malloc(sizeof(struct vector));
    vector_init(response->headers, 8, sizeof(struct http_header));

    response->version = malloc(9 * sizeof(char));
    char status_code_str[4] = {0};
    response->status_message = malloc(64 * sizeof(char));

    if (socket_recv_until(client->client.sockfd, response->version, 9, " ", 1) <= 0) return -1;
    if (socket_recv_until(client->client.sockfd, status_code_str, 3, " ", 1) <= 0) return -1;
    if (socket_recv_until(client->client.sockfd, response->status_message, 63, "\r\n", 2) <= 0) return -1;

    response->status_code = atoi(status_code_str);

    size_t content_length = 0; // < 0 means chunked

    char* buffer = malloc((256 + 4096 + 2) * sizeof(char));
    while (1)
    {
        if (socket_recv_until(client->client.sockfd, buffer, 256 + 4096 + 2, "\r\n", 1) <= 0) return -1;
        
        if (buffer[0] == '\0') break;

        struct http_header* header = malloc(sizeof(struct http_header));
        header->name = malloc(256 * sizeof(char));
        header->value = malloc(4096 * sizeof(char));

        char* value = strchr(buffer, ':');
        if (value == NULL) return -2;

        strncpy(header->name, buffer, value - buffer);
        strncpy(header->value, value + 2, strlen(value));

        header->name[value - buffer] = '\0';
        header->value[strlen(value)] = '\0';

        if (content_length == 0 && strcmp(header->name, "Content-Length") == 0)
            content_length = strtoul(header->value, NULL, 10);
        else if (content_length == 0 && strcmp(header->name, "Transfer-Encoding") == 0 && strcmp(header->value, "chunked") == 0)
            content_length = -1;

        vector_push(response->headers, header);
        memset(buffer, 0, (256 + 4096 + 2) * sizeof(char));
    };

    if (content_length == -1)
    {
        struct vector body = {0};
        vector_init(&body, 4096, sizeof(char));

        while (1)
        {
            if (socket_recv_until(client->client.sockfd, buffer, 256 + 4096 + 2, "\r\n", 1) <= 0) return -1;

            size_t chunk_length = strtoul(buffer, NULL, 16);
            if (chunk_length == 0) break;

            if (socket_recv_until(client->client.sockfd, buffer, chunk_length + 2, "\r\n", 1) <= 0) return -1;

            for (size_t i = 0; i < chunk_length; ++i)
                vector_push(&body, &buffer[i]);

            memset(buffer, 0, (256 + 4096 + 2) * sizeof(char));
        };

        response->body = malloc(body.size * sizeof(char));
        for (size_t i = 0; i < body.size; ++i)
            response->body[i] = *(char*)vector_get(&body, i);
    }
    else if (content_length > 0)
    {
        response->body = malloc(content_length * sizeof(char));
        if (socket_recv_until(client->client.sockfd, response->body, content_length + 2, "\r\n", 1) <= 0) return -1;
    };

    free(buffer);

    return 0;
};