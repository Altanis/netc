#include "http/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        printf("malformed response: %d\n", result);
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
    string_t request_str = {0};
    sso_string_init(&request_str, "");

    char encoded[request->path.length * 3 + 1];
    http_url_percent_encode((char*)sso_string_get(&request->path), encoded);

    sso_string_concat_buffer(&request_str, sso_string_get(&request->method));
    sso_string_concat_char(&request_str, ' ');
    sso_string_concat_buffer(&request_str, encoded);
    sso_string_concat_char(&request_str, ' ');
    sso_string_concat_buffer(&request_str, sso_string_get(&request->version));
    sso_string_concat_buffer(&request_str, "\r\n");

    const char* len = sso_string_get(&request_str);

    int chunked = 0;

    for (size_t i = 0; i < request->headers.size; ++i)
    {
        struct http_header* header = vector_get(&request->headers, i);
        const char* name = sso_string_get(&header->name);
        const char* value = sso_string_get(&header->value);

        if (!chunked && strcmp(name, "Transfer-Encoding") == 0 && strcmp(value, "chunked") == 0)
            chunked = 1;

        sso_string_concat_buffer(&request_str, name);
        sso_string_concat_buffer(&request_str, ": ");
        sso_string_concat_buffer(&request_str, value);
        sso_string_concat_buffer(&request_str, "\r\n");
    };

    sso_string_concat_buffer(&request_str, "\r\n");

    const char* body = sso_string_get(&request->body);
    if (!chunked && body != NULL)
    {
        sso_string_concat_buffer(&request_str, body);
    };
    
    printBytes(sso_string_get(&request_str), request_str.length);
    // printf("%s\n", sso_string_get(&request_str));

    return tcp_client_send(&client->client, (char*)sso_string_get(&request_str), request_str.length, 0);
};

int http_client_parse_response(struct http_client* client, struct http_response* response)
{
    char buffer[4096];
    recv(client->client.sockfd, buffer, 4096, MSG_PEEK);
    printf("%s\n", buffer);

    vector_init(&response->headers, 8, sizeof(struct http_header));

    string_t version = {0};
    string_t status_code = {0};
    string_t status_message = {0};

    sso_string_init(&version, "");
    sso_string_init(&status_code, "");
    sso_string_init(&status_message, "");

    if (socket_recv_until_dynamic(client->client.sockfd, &version, " ", 1, 9) <= 0) return -1;
    if (socket_recv_until_dynamic(client->client.sockfd, &status_code, " ", 1, 4) <= 0) return -1;
    if (socket_recv_until_dynamic(client->client.sockfd, &status_message, "\r\n", 1, 63) <= 0) return -1;

    response->version = version;
    response->status_code = atoi(sso_string_get(&status_code));
    response->status_message = status_message;

    printf("version: %s\n", sso_string_get(&version));
    printf("status: %s\n", sso_string_get(&status_code));
    printf("msg: %s\n", sso_string_get(&status_message));

    size_t content_length = 0; // < 0 means chunked

    while (1)
    {
        struct http_header header = {0};
        sso_string_init(&header.name, "");
        sso_string_init(&header.value, "");

        if (socket_recv_until_dynamic(client->client.sockfd, &header.name, ": ", 1, 63) <= 0) return -1;
        if (socket_recv_until_dynamic(client->client.sockfd, &header.value, "\r\n", 1, 8191) <= 0) return -1;
    
        if (content_length == 0 && strcmp(sso_string_get(&header.name), "Content-Length") == 0)
            content_length = strtoul(sso_string_get(&header.value), NULL, 10);
        else if (content_length == 0 && strcmp(sso_string_get(&header.name), "Transfer-Encoding") == 0 && strcmp(sso_string_get(&header.value), "chunked") == 0)
            content_length = -1;

        vector_push(&response->headers, &header);

        char temp_buffer[2] = {0};
        if (recv(client->client.sockfd, temp_buffer, 2, MSG_PEEK) <= 0) return -1;

        if (temp_buffer[0] == '\r' && temp_buffer[1] == '\n')
        {
            if (recv(client->client.sockfd, temp_buffer, 2, 0) <= 0) return -1;
            break;
        };
    };

    sso_string_init(&response->body, "");

    if (content_length == -1)
    {
        while (1)
        {
            string_t chunk_length = {0};
            sso_string_init(&chunk_length, "");

            if (socket_recv_until_dynamic(client->client.sockfd, &chunk_length, "\r\n", 1, 64) <= 0) return -1;

            size_t chunk_size = strtoul(sso_string_get(&chunk_length), NULL, 16);
            if (chunk_size == 0) break;

            if (socket_recv_until_dynamic(client->client.sockfd, &response->body, "\r\n", 1, chunk_size) <= 0) return -1;

            char temp_buffer[2] = {0};
            if (socket_recv_until_fixed(client->client.sockfd, temp_buffer, 2, "\r\n", 1) <= 0) return -1;
            if (temp_buffer[0] != '\r' || temp_buffer[1] != '\n') return -4;
        };
    }
    else if (socket_recv_until_dynamic(client->client.sockfd, &response->body, "\r\n", 1, content_length) <= 0) return -1;

    return 0;
};