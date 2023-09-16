#include "http/client.h"

#include<unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _tcp_on_connect(struct tcp_client *client, void *data)
{
    struct http_client *http_client = (struct http_client*)data;
    if (http_client->on_connect != NULL)
        http_client->on_connect(http_client, http_client->data);
};

static void _tcp_on_data(struct tcp_client *client, void *data)
{
    struct http_client *http_client = (struct http_client*)data;
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

static void _tcp_on_disconnect(struct tcp_client *client, int is_error, void *data)
{
    struct http_client *http_client = (struct http_client*)data;
    if (http_client->on_disconnect != NULL)
        http_client->on_disconnect(http_client, is_error, http_client->data);
};

int http_client_init(struct http_client *client, struct sockaddr address)
{
    struct tcp_client *tcp_client = malloc(sizeof(struct tcp_client));
    tcp_client->data = client;

    int init_result = tcp_client_init(tcp_client, address, 1);
    if (init_result != 0) return init_result;

    if (setsockopt(tcp_client->sockfd, SOL_SOCKET, SO_REUSEADDR, &(char){1}, sizeof(int)) < 0)
    {
        /** Not essential. Do not return -1. */
    };

    int connect_result = tcp_client_connect(tcp_client);
    if (connect_result != 0) return connect_result;

    tcp_client->on_connect = _tcp_on_connect;
    tcp_client->on_data = _tcp_on_data;
    tcp_client->on_disconnect = _tcp_on_disconnect;

    client->client = tcp_client;

    return 0;
};

int http_client_start(struct http_client *client)
{
    return tcp_client_main_loop(client->client);
};

int http_client_send_chunked_data(struct http_client *client, char *data, size_t length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", length);

    int send_result = 0;

    if ((send_result = tcp_client_send(client->client, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (length != 0 && ((send_result = tcp_client_send(client->client, length == 0 ? "" : data, length, 0)) <= 0)) return send_result;    
    if ((send_result = tcp_client_send(client->client, "\r\n", 2, 0)) <= 2) return send_result;

    return 0;
};

int http_client_send_request(struct http_client *client, struct http_request *request, const char *data, size_t data_length)
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
        struct http_header *header = vector_get(&request->headers, i);
        const char *name = sso_string_get(&header->name);
        const char *value = sso_string_get(&header->value);

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

    ssize_t first_send = tcp_client_send(client->client, (char*)sso_string_get(&request_str), request_str.length, 0);
    if (first_send <= 0) return first_send;
    
    if (data_length > 0)
    {
        ssize_t second_send = tcp_client_send(client->client, (char*)data, data_length, 0);
        if (second_send <= 0) return second_send;
    };

    return 0;
};

int http_client_parse_response(struct http_client *client, struct http_response *response)
{   
    char floffy[8192] = {0};
    recv(client->client->sockfd, floffy, 8192, MSG_PEEK);
    printf("floffy:\n");
    print_bytes(floffy, 8192);

    vector_init(&response->headers, 8, sizeof(struct http_header));

    string_t version = {0};
    string_t status_code = {0};
    string_t status_message = {0};

    sso_string_init(&version, "");
    sso_string_init(&status_code, "");
    sso_string_init(&status_message, "");

    enum http_response_parsing_states state = -1;

    while (1)
    {
        // if (socket_recv_until_dynamic(client->client->sockfd, &version, " ", 1, 8 + 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        // if (socket_recv_until_dynamic(client->client->sockfd, &status_code, " ", 1, 3 + 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        // if (socket_recv_until_dynamic(client->client->sockfd, &status_message, "\r\n", 1, 64 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        NETC_HTTP_RESPONSE_PARSE(state, RESPONSE_PARSING_STATE_VERSION, client->client->sockfd, &version, " ", 1, 8 + 1, 0);
        NETC_HTTP_RESPONSE_PARSE(state, RESPONSE_PARSING_STATE_STATUS_CODE, client->client->sockfd, &status_code, " ", 1, 3 + 1, 0);
        NETC_HTTP_RESPONSE_PARSE(state, RESPONSE_PARSING_STATE_STATUS_MESSAGE, client->client->sockfd, &status_message, "\r\n", 1, 64 + 2, 0);
        break;
    };

    char yum[8192] = {0};
    recv(client->client->sockfd, yum, 8192, MSG_PEEK);
    print_bytes(yum, 8192);

    response->version = version;
    response->status_code = atoi(sso_string_get(&status_code));
    response->status_message = status_message;

    int content_length = 0; // < 0 means chunked

    while (1)
    {
        // char fluffy[8192] = {0};
        // recv(client->client->sockfd, fluffy, 8192, MSG_PEEK);
        // printf("fluffy:\n");
        // print_bytes(fluffy, 8192);

        char temp_buffer[2] = {0};
        if (recv(client->client->sockfd, temp_buffer, 2, MSG_PEEK) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        // for (size_t i = 0; i < 2; ++i) printf("%c ", temp_buffer[i]);
        // printf("\n");

        if (temp_buffer[0] == '\r' && temp_buffer[1] == '\n')
        {
            if (recv(client->client->sockfd, temp_buffer, 2, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            break;
        };

        struct http_header header = {0};
        sso_string_init(&header.name, "");
        sso_string_init(&header.value, "");

        // if (socket_recv_until_dynamic(client->client->sockfd, &header.name, ": ", 1, 64 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        // if (socket_recv_until_dynamic(client->client->sockfd, &header.value, "\r\n", 1, 8192 + 2) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        NETC_HTTP_RESPONSE_PARSE(state, RESPONSE_PARSING_STATE_HEADER_NAME, client->client->sockfd, &header.name, ": ", 1, 64 + 2, 0);
        NETC_HTTP_RESPONSE_PARSE(state, RESPONSE_PARSING_STATE_HEADER_VALUE, client->client->sockfd, &header.value, "\r\n", 1, 8192 + 2, 0);

        if (content_length == 0 && strcmp(sso_string_get(&header.name), "Content-Length") == 0)
            content_length = strtoul(sso_string_get(&header.value), NULL, 10);
        else if (content_length == 0 && strcmp(sso_string_get(&header.name), "Transfer-Encoding") == 0 && strcmp(sso_string_get(&header.value), "chunked") == 0)
            content_length = -1;

        vector_push(&response->headers, &header);
    };

    struct vector buffer = {0};
    vector_init(&buffer, 8, sizeof(char));

    if (content_length == 0) return 0;
    else if (content_length == -1)
    {
        char fluffy[8192] = {0};
        recv(client->client->sockfd, fluffy, 8192, MSG_PEEK);
        printf("fluffy:\n");
        print_bytes(fluffy, 8192);

        while (1)
        {
            char chunk_length[18] = {0};
            if (socket_recv_until_fixed(client->client->sockfd, chunk_length, 16 + 2, "\r\n", 1) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            
            size_t chunk_size = strtoul(chunk_length, NULL, 16);
            printf("chunk_size: %zu\n", chunk_size);

            if (chunk_size == 0)
            {
                char POOFY[4096] = {0};
                recv(client->client->sockfd, POOFY, 4096, MSG_PEEK);
                printf("POOFY: %s\n", POOFY);
                // todo crlf not get absorb proper :(
                char crlf[4096] = {0};
                // if (socket_recv_until_fixed(client->client->sockfd, crlf, 2, "\r\n", 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
                if (recv(client->client->sockfd, crlf, 4096, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
                printf("crlf: %s\n", crlf);
                break;
            };

            char chunk_data[chunk_size + 2];
            if (recv(client->client->sockfd, chunk_data, chunk_size + 2, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
            for (size_t i = 0; i < chunk_size; ++i) vector_push(&buffer, chunk_data + i);
        };
    }
    else
    {
        state = RESPONSE_PARSING_STATE_BODY;

        char body_data[content_length + 2];
        if (socket_recv_until_fixed(client->client->sockfd, body_data, content_length + 2, NULL, 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
        for (size_t i = 0; i < content_length; ++i) 
        {
            printf("%c\n", body_data[i]);
            vector_push(&buffer, body_data + i);
        };
    };

    response->body = malloc(buffer.size + 1);
    for (size_t i = 0; i < buffer.size; ++i) response->body[i] = *(char*)vector_get(&buffer, i);
    response->body[buffer.size] = '\0';
    response->body_size = buffer.size;

    vector_free(&buffer);

    return 0;
};

int http_client_close(struct http_client *client)
{
    return tcp_client_close(client->client, 0);
};