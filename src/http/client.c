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

int http_client_init(struct http_client* client, struct sockaddr address)
{
    struct tcp_client tcp_client = {0};
    tcp_client.data = client;

    int init_result = tcp_client_init(&tcp_client, address, 1);
    if (init_result != 0) return init_result;

    if (setsockopt(client->client.sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        /** Not essential. Do not return -1. */
    };

    int connect_result = tcp_client_connect(&tcp_client);
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

int http_client_send_chunked_data(struct http_client* client, char* data, size_t length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_client_send(&client->client, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (length != 0 && ((send_result = tcp_client_send(&client->client, length == 0 ? "" : data, length, 0)) <= 0)) return send_result;    
    if ((send_result = tcp_client_send(&client->client, "\r\n", 2, 0)) <= 2) return send_result;

    return 0;
};

int http_client_send_request(struct http_client* client, struct http_request* request, const char* data, size_t data_length)
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

    int chunked = 0;

    for (size_t i = 0; i < request->headers.size; ++i)
    {
        struct http_header* header = vector_get(&request->headers, i);
        const char* name = sso_string_get(&header->name);
        const char* value = sso_string_get(&header->value);

        if (!chunked && strcmp(name, "Transfer-Encoding") == 0 && strcmp(value, "chunked") == 0)
            chunked = 1;
        else if (!chunked && strcmp(name, "Content-Length") == 0)
            chunked = -1;

        sso_string_concat_buffer(&request_str, name);
        sso_string_concat_buffer(&request_str, ": ");
        sso_string_concat_buffer(&request_str, value);
        sso_string_concat_buffer(&request_str, "\r\n");
    };

    if (chunked == 0)
    {
        char length_str[16] = {0};
        sprintf(length_str, "%zu", data_length);

        sso_string_concat_buffer(&request_str, "Content-Length: ");
        sso_string_concat_buffer(&request_str, length_str);
        sso_string_concat_buffer(&request_str, "\r\n");
    };

    sso_string_concat_buffer(&request_str, "\r\n");

    ssize_t first_send = tcp_client_send(&client->client, (char*)sso_string_get(&request_str), request_str.length, 0);
    if (first_send <= 0) return first_send;

    printf("data 2besent: %s\n", data);
    printf("data length: %zu\n", data_length);
    
    if (data_length > 0)
    {
        ssize_t second_send = tcp_client_send(&client->client, (char*)data, data_length, 0);
        if (second_send <= 0) return second_send;
    };

    return 0;
};

int http_client_parse_response(struct http_client* client, struct http_response* response)
{   
    vector_init(&response->headers, 8, sizeof(struct http_header));

    string_t version = {0};
    string_t status_code = {0};
    string_t status_message = {0};

    sso_string_init(&version, "");
    sso_string_init(&status_code, "");
    sso_string_init(&status_message, "");

    if (socket_recv_until_dynamic(client->client.sockfd, &version, " ", 1, 8 + 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
    if (socket_recv_until_dynamic(client->client.sockfd, &status_code, " ", 1, 3 + 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
    if (socket_recv_until_dynamic(client->client.sockfd, &status_message, "\r\n", 1, 64 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;

    response->version = version;
    response->status_code = atoi(sso_string_get(&status_code));
    response->status_message = status_message;

    size_t content_length = 0; // < 0 means chunked

    while (1)
    {
        char temp_buffer[2] = {0};
        if (recv(client->client.sockfd, temp_buffer, 2, MSG_PEEK) <= 0) return RESPONSE_PARSE_ERROR_RECV;

        if (temp_buffer[0] == '\r' && temp_buffer[1] == '\n')
        {
            if (recv(client->client.sockfd, temp_buffer, 2, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            break;
        };

        struct http_header header = {0};
        sso_string_init(&header.name, "");
        sso_string_init(&header.value, "");

        if (socket_recv_until_dynamic(client->client.sockfd, &header.name, ": ", 1, 64 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        if (socket_recv_until_dynamic(client->client.sockfd, &header.value, "\r\n", 1, 8192 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
    
        if (content_length == 0 && strcmp(sso_string_get(&header.name), "Content-Length") == 0)
            content_length = strtoul(sso_string_get(&header.value), NULL, 10);
        else if (content_length == 0 && strcmp(sso_string_get(&header.name), "Transfer-Encoding") == 0 && strcmp(sso_string_get(&header.value), "chunked") == 0)
            content_length = -1;

        vector_push(&response->headers, &header);
    };

    char buffer[content_length + 1];
    size_t buffer_length = 0;

    /** The server is required to send content length. */
    if (content_length == 0) return 0;
    else if (content_length == -1)
    {
        while (1)
        {
            char chunk_length[18] = {0};

            // if (socket_recv_until_dynamic(client->client.sockfd, &chunk_length, "\r\n", 1, 16 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            if (socket_recv_until_fixed(client->client.sockfd, chunk_length, 16 + 2, "\r\n", 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;

            size_t chunk_size = strtoul(sso_string_get(&chunk_length), NULL, 16);
            if (chunk_size == 0)
            {
                char temp_buffer[2] = {0};
                if (recv(client->client.sockfd, temp_buffer, 2, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
                break;
            };

            buffer_length += chunk_size;
            
            char chunk_data[chunk_size + 2];
            // if (socket_recv_until_dynamic(client->client.sockfd, &response->body, "\r\n", 1, chunk_size + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            if (socket_recv_until_fixed(client->client.sockfd, chunk_data, chunk_size + 2, "\r\n", 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            chunk_data[chunk_size] = '\0';

            strcat(buffer, chunk_data);
        };
    }
    else
    {
        size_t bytes_left = content_length;

        while (1)
        {
            if (bytes_left <= 0)
            {
                buffer_length = content_length;
                break;
            };
            
            ssize_t result = tcp_client_receive(&client->client, buffer, bytes_left, 0);
            if (result == -1) return RESPONSE_PARSE_ERROR_RECV;
            else if (result == 0)
            {
                buffer_length = content_length - bytes_left;
                break;                
            };

            bytes_left -= result;
        };
    };

    response->body = malloc(sizeof(buffer) + 1);
    memcpy(response->body, buffer, sizeof(buffer) + 1);
    response->body[buffer_length] = '\0';
    response->body_size = buffer_length;

    return 0;
};

int http_client_close(struct http_client* client)
{
    return tcp_client_close(&client->client, 0);
};