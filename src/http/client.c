#include "http/client.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _tcp_on_connect(struct tcp_client *client, void *data)
{
    struct http_client *http_client = (struct http_client *)data;
    if (http_client->on_connect != NULL)
        http_client->on_connect(http_client, http_client->data);
};

static void _tcp_on_data(struct tcp_client *client, void *data)
{
    struct http_client *http_client = (struct http_client *)data;
    struct http_client_parsing_state current_state = http_client->client_parsing_state;

    int result = 0;
    if ((result = http_client_parse_response(http_client, &current_state)) != 0)
    {
        if (result < 0)
        {
            if (http_client->on_malformed_response != NULL)
            http_client->on_malformed_response(http_client, result, http_client->data);
            return;
        };

        printf("WAITING.\n");
        return;
    };

    if (http_client->on_data != NULL)
        http_client->on_data(http_client, current_state.response, http_client->data);
};

static void _tcp_on_disconnect(struct tcp_client *client, int is_error, void *data)
{
    struct http_client *http_client = (struct http_client *)data;
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

int http_client_send_chunked_data(struct http_client *client, char *data, size_t data_length)
{
    char length_str[16] = {0};
    sprintf(length_str, "%zx\r\n", data_length);

    int send_result = 0;

    if ((send_result = tcp_client_send(client->client, length_str, strlen(length_str), 0)) <= 0) return send_result;
    if (data_length != 0 && ((send_result = tcp_client_send(client->client, data_length == 0 ? "" : data, data_length, 0)) <= 0)) return send_result;    
    if ((send_result = tcp_client_send(client->client, "\r\n", 2, 0)) <= 2) return send_result;

    return 0;
};

int http_client_send_request(struct http_client *client, struct http_request *request, const char *data, size_t data_length)
{
    string_t request_str = {0};
    sso_string_init(&request_str, "");

    char encoded[request->path.length * 3 + 1];
    http_url_percent_encode((char *)sso_string_get(&request->path), encoded);

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

        if (!chunked && strcasecmp(name, "Transfer-Encoding") == 0 && strcasecmp(value, "chunked") == 0)
            chunked = 1;
        else if (!chunked && strcasecmp(name, "Content-Length") == 0)
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

    ssize_t first_send = tcp_client_send(client->client, (char *)sso_string_get(&request_str), request_str.length, 0);
    if (first_send <= 0) return first_send;
    
    if (data_length > 0)
    {
        ssize_t second_send = tcp_client_send(client->client, (char *)data, data_length, 0);
        if (second_send <= 0) return second_send;
    };

    return 0;
};

int http_client_parse_response(struct http_client *client, struct http_client_parsing_state *current_state)
{   
    char our_story[4096] = {0};
    recv(client->client->sockfd, our_story, 4095, MSG_PEEK);
    printf("LOFFY:\n");
    print_bytes(our_story, 4095);

    socket_t sockfd = client->client->sockfd;
    vector_init(&current_state->response.headers, 8, sizeof(struct http_header));

parse_start:
    printf("state %d\n", current_state->parsing_state);
    errno = 0;
    switch (current_state->parsing_state)
    {
        case -1:
        {
            current_state->parsing_state = RESPONSE_PARSING_STATE_VERSION;
            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_VERSION:
        {
            string_t *version = &current_state->response.version;
            if (version->length == 0) sso_string_init(version, "");
            
            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, version, " ", 1, 8 + 1);
            printf("ssize_t %d\n", bytes_received);
            if (bytes_received <= 0)
            {
                if (bytes_received == -2 || errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            current_state->parsing_state = RESPONSE_PARSING_STATE_STATUS_CODE;
            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_STATUS_CODE:
        {
            char buffer[4] = {0};
            if (recv(sockfd, buffer, sizeof(buffer), MSG_PEEK) <= 0) return RESPONSE_PARSE_ERROR_RECV;

            if (buffer[3] == ' ')
            {
                buffer[3] = '\0';
                current_state->response.status_code = atoi(buffer);
            } else return 1;

            current_state->parsing_state = RESPONSE_PARSING_STATE_STATUS_MESSAGE;
            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_STATUS_MESSAGE:
        {
            string_t *status_message = &current_state->response.status_message;
            if (status_message->length == 0) sso_string_init(status_message, "");

            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, status_message, "\r\n", 1, 64 + 2);
            if (bytes_received <= 0)
            {
                if (bytes_received == -2 || errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            current_state->parsing_state = RESPONSE_PARSING_STATE_HEADER_NAME;
            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_HEADER_NAME:
        {
            char crlf[2] = {0};
            ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);

            if (crlf[0] == '\r')
            {
                if (crlf[1] == '\n')
                {
                    if (recv(sockfd, crlf, sizeof(crlf), 0) <= 0) return REQUEST_PARSE_ERROR_RECV;

                    if (current_state->content_length == 0) break;
                    else
                    {
                        current_state->parsing_state = 
                            current_state->content_length == -1 ?
                            RESPONSE_PARSING_STATE_CHUNK_SIZE :
                            RESPONSE_PARSING_STATE_BODY;
                        goto parse_start;
                    };
                } else return 1;
            };

            struct http_header *header = &current_state->header;
            if (header->name.length == 0) sso_string_init(&header->name, "");

            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &header->name, ": ", 1, 256 + 2);
            if (bytes_received <= 0)
            {
                if (bytes_received == -2 || errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            current_state->parsing_state = RESPONSE_PARSING_STATE_HEADER_VALUE;
            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_HEADER_VALUE:
        {
            struct vector *headers = &current_state->response.headers;
            struct http_header *header = &current_state->header;
            if (header->value.length == 0) sso_string_init(&header->value, "");

            ssize_t bytes_received = socket_recv_until_dynamic(sockfd, &header->value, "\r\n", 1, 4096 + 2);
            if (bytes_received <= 0)
            {
                if (bytes_received == -2 || errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            if (strcasecmp(sso_string_get(&header->name), "Content-Length") == 0)
                current_state->content_length = atoi(sso_string_get(&header->value));
            else if (strcasecmp(sso_string_get(&header->name), "Transfer-Encoding") == 0 && strcasecmp(sso_string_get(&header->value), "chunked") == 0)
                current_state->content_length = -1;

            vector_push(headers, header);
            memset(header, 0, sizeof(*header));

            current_state->parsing_state = RESPONSE_PARSING_STATE_HEADER_NAME;
            goto parse_start;  
        };
        case RESPONSE_PARSING_STATE_CHUNK_SIZE:
        {
                        char woffywoffy[4096] = {0};
            int x = recv(sockfd, woffywoffy, 4096, MSG_PEEK);
            printf("worg up.\n");
            print_bytes(woffywoffy, x);

            if (current_state->chunk_data.size == 0)
            {
                current_state->chunk_size = -1;
                vector_init(&current_state->chunk_data, 128, sizeof(char));
            };

            if (current_state->chunk_size == -1)
            {
                // todo: make chunk length variable.
                size_t preexisting_chunk_length = strlen(current_state->chunk_length);
                size_t length = 16 + 2 - preexisting_chunk_length;
                char *delimiter = length == 1 ? "\n" : "\r\n";

                ssize_t bytes_received = socket_recv_until_fixed(sockfd, current_state->chunk_length + preexisting_chunk_length, length, delimiter, 1);
                if (bytes_received <= 0)
                {
                    if (errno == EWOULDBLOCK) return 1;
                    else return RESPONSE_PARSE_ERROR_RECV;
                };

                current_state->chunk_size = strtoul(current_state->chunk_length, NULL, 16);
                memset(&current_state->chunk_length, 0, sizeof(current_state->chunk_length));
            };

            if (current_state->chunk_size == 0)
            {
                char crlf[2] = {0};
                ssize_t check_crlf = recv(sockfd, crlf, sizeof(crlf), MSG_PEEK);

                if (memcmp(crlf, "\r\n", 2) == 0)
                {
                    if (recv(sockfd, crlf, sizeof(crlf), 0) <= 0) return RESPONSE_PARSE_ERROR_RECV;
                    break;
                } else return 1;
            };

            current_state->parsing_state = RESPONSE_PARSING_STATE_CHUNK_DATA;
        };
        case RESPONSE_PARSING_STATE_CHUNK_DATA:
        {
            size_t preexisting_chunk_data = current_state->chunk_data.size - current_state->response.body_size;
            size_t length = current_state->chunk_size + 2 - preexisting_chunk_data;

            char buffer[length];
            ssize_t bytes_received = recv(sockfd, buffer, length, 0);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            for (size_t i = 0; i < bytes_received; ++i) vector_push(&current_state->chunk_data, buffer + i);
            if (bytes_received == length)
            {
                printf("[women]:\n");
                for (size_t i = 0; i < current_state->chunk_data.size; ++i)
                {
                    print_bytes((char *)vector_get(&current_state->chunk_data, i), 1);
                };

                printf("trying! to delete: %d\n", current_state->chunk_data.size - 1);
                printf("the location of this data:");
                print_bytes((char *)vector_get(&current_state->chunk_data, current_state->chunk_data.size - 1), 1);

                size_t vec_size = current_state->chunk_data.size;

                vector_delete(&current_state->chunk_data, vec_size - 1); // delete \n
                vector_delete(&current_state->chunk_data, vec_size - 2); // delete \r
            } else return 1;

            current_state->response.body_size += current_state->chunk_size;
            current_state->chunk_size = -1;
            current_state->parsing_state = RESPONSE_PARSING_STATE_CHUNK_SIZE;

            goto parse_start;
        };
        case RESPONSE_PARSING_STATE_BODY:
        {
            if (current_state->chunk_data.size == 0)
                vector_init(&current_state->chunk_data, 128, sizeof(char));

            size_t preexisting_body_data = current_state->chunk_data.size;
            size_t length = current_state->content_length - preexisting_body_data;

            char buffer[length];
            ssize_t bytes_received = recv(sockfd, buffer, length, 0);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return RESPONSE_PARSE_ERROR_RECV;
            };

            for (size_t i = 0; i < bytes_received; ++i) vector_push(&current_state->chunk_data, buffer + i);

            if (bytes_received == length) break;
            else return 1;
        };
    };

    if (current_state->response.body_size > 0)
    {
        vector_push(&current_state->chunk_data, &(char){'\0'});
        current_state->response.body = (char *)current_state->chunk_data.elements;
    };

    return 0;
};

int http_client_close(struct http_client *client)
{
    return tcp_client_close(client->client, 0);
};