#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <openssl/sha.h>


#include "../../include/web/server.h"
#include "../../include/ws/server.h"
#include "../../include/tcp/server.h"

int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request)
{
    socket_t sockfd = client->tcp_client->sockfd;

    struct http_header *sec_websocket_key = http_request_get_header(request, "Sec-WebSocket-Key");
    struct http_header *sec_websocket_version = http_request_get_header(request, "Sec-WebSocket-Version");
    struct http_header *sec_websocket_protocol = http_request_get_header(request, "Sec-WebSocket-Protocol");

    if (sec_websocket_key == NULL)
    {
        // That comedian...
        const char *badrequest_message = "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 33\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Invalid Sec-WebSocket-Key header.";

        tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
        tcp_server_close_client(server->tcp_server, sockfd, 0);

        return -1;
    };

    if (sec_websocket_version == NULL || strcmp(sso_string_get(&sec_websocket_version->value), WEBSOCKET_VERSION) != 0)
    {
        // That comedian...
        const char *badrequest_message = "HTTP/1.1 426 Upgrade Required\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 41\r\n"
            "Sec-WebSocket-Version: 13\r\n" // reminder to change this
            "Connection: close\r\n"
            "\r\n"
            "Unsupported Sec-WebSocket-Version header.";

        tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
        tcp_server_close_client(server->tcp_server, sockfd, 0);

        return -1;
    };

    size_t guid_len = strlen(WEBSOCKET_HANDSHAKE_GUID);
    char websocket_key_id[sec_websocket_key->value.length + guid_len];
    sso_string_copy_buffer(websocket_key_id, &sec_websocket_key->value);
    for (size_t i = 0; i < guid_len; ++i)
    {
        websocket_key_id[sec_websocket_key->value.length + i] = WEBSOCKET_HANDSHAKE_GUID[i];
    };

    char websocket_accept_id[SHA_DIGEST_LENGTH];
    SHA1(websocket_key_id, sizeof(websocket_key_id), websocket_accept_id);


    char websocket_accept_id_base64[((4 * sizeof(websocket_accept_id) / 3) + 3) & ~3];
    http_base64_encode(websocket_accept_id, sizeof(websocket_accept_id), websocket_accept_id_base64);

    char* headers[][2] =
    {
        { "Connection", "upgrade" },
        { "Upgrade", "websocket" },
        { "Sec-WebSocket-Accept", websocket_accept_id_base64 }
    };

    struct http_response response;
    http_response_build(
        &response,
        "HTTP/1.1", 
        101, 
        headers,
        3
    );

    if (sec_websocket_protocol != NULL)
    {
        struct http_header protocol;
        http_header_init(&protocol, "Sec-WebSocket-Protocol", sso_string_get(&sec_websocket_protocol->value));
        vector_push(&response.headers, &protocol);
    };

    int result = http_server_send_response(server, client, &response, NULL, 0);
    if (result == 0)
    {
        client->path = strdup(sso_string_get(&request->path));
        client->connection_type = CONNECTION_WS;
        return 0;
    } else return -1;
};

int ws_server_parse_frame(struct web_server *server, struct web_client *client, struct ws_frame_parsing_state *current_state)
{
    socket_t sockfd = client->tcp_client->sockfd;

    size_t MAX_PAYLOAD_LENGTH = server->ws_server_config.max_payload_len || 65536;

    char lookahead[4096];
    int size = recv(sockfd, lookahead, 4096, MSG_PEEK);
    for (int i = 0; i < size; ++i) printf("%x ", (unsigned char)lookahead[i]);
    printf("\n");

parse_start:
    printf("%d\n", current_state->parsing_state);
    switch (current_state->parsing_state)
    {
        case -1:
        {
            current_state->parsing_state = WS_FRAME_PARSING_STATE_FIRST_BYTE;
            goto parse_start;
        };
        case WS_FRAME_PARSING_STATE_FIRST_BYTE:
        {
            uint8_t byte;
            ssize_t byte_received = recv(sockfd, &byte, 1, 0);

            if (byte_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            current_state->frame.fin = (byte & 0b10000000) >> 7;
            current_state->frame.rsv1 = (byte & 0b01000000) >> 6;
            current_state->frame.rsv2 = (byte & 0b00100000) >> 5;
            current_state->frame.rsv3 = (byte & 0b00010000) >> 4;
            current_state->frame.opcode = byte & 0b00001111;

            if (current_state->frame.opcode != WS_OPCODE_CONTINUE)
                current_state->message.opcode = current_state->frame.opcode;

            current_state->parsing_state = WS_FRAME_PARSING_STATE_SECOND_BYTE;
            goto parse_start;
        };
        case WS_FRAME_PARSING_STATE_SECOND_BYTE:
        {
            uint8_t byte;
            ssize_t byte_received = recv(sockfd, &byte, 1, 0);

            if (byte_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            current_state->frame.mask = (byte & 0b10000000) >> 7;
            current_state->frame.payload_length = byte & 0b01111111;

            current_state->real_payload_length = 0;
            current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_LENGTH;

            goto parse_start;
        };
        case WS_FRAME_PARSING_STATE_PAYLOAD_LENGTH:
        {
            void *ptr_to_null = memchr((const void *)&current_state->real_payload_length, 0x00, sizeof(uint64_t));

            if (ptr_to_null == NULL)
            {
                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            };

            size_t length = (const uint8_t *)ptr_to_null - (const uint8_t *)&current_state->real_payload_length;
            printf("[length[ %d\n", length);
            size_t num_bytes_recv = 0;

            if (current_state->frame.payload_length <= 125) 
            {
                current_state->real_payload_length = current_state->frame.payload_length;
                
                if (current_state->frame.opcode == WS_OPCODE_TEXT) current_state->message.buffer = calloc(current_state->real_payload_length + 1, sizeof(char));
                else current_state->message.buffer = calloc(current_state->real_payload_length, sizeof(char));
                
                current_state->message.payload_length = current_state->real_payload_length;

                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            }
            else if (current_state->frame.payload_length == 126) num_bytes_recv = 2; // not work
            else if (current_state->frame.payload_length == 127) num_bytes_recv = 8; // not work
            else return WS_FRAME_PARSE_ERROR_INVALID_FRAME_LENGTH;

            if (num_bytes_recv - length <= 0)
            {
                current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                goto parse_start;
            };

            uint64_t value = 0;
            ssize_t bytes_received = recv(sockfd, &value, num_bytes_recv - length, 0);

            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            for (size_t i = 0; i < bytes_received; ++i)
            {
                current_state->real_payload_length <<= 8;
                current_state->real_payload_length |= (value >> (8 * (bytes_received - i - 1))) & 0xFF;
            };

            if (bytes_received == num_bytes_recv - length)
            {
                if (current_state->real_payload_length > MAX_PAYLOAD_LENGTH) return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;
                current_state->message.buffer = calloc(current_state->real_payload_length, sizeof(char));
                current_state->message.payload_length = current_state->real_payload_length;

                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            };
        };
        case WS_FRAME_PARSING_STATE_MASKING_KEY:
        {
            uint8_t *masking_key = current_state->frame.masking_key;
            ssize_t length = masking_key[0] == 0 ? 0 : (masking_key[1] == 0 ? 1 : (masking_key[2] == 0 ? 2 : (masking_key[3] == 0 ? 3 : 4)));

            if (length == -1)
            {
                current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;
                goto parse_start;
            };

            ssize_t bytes_received = recv(sockfd, masking_key, 4 - length, 0);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            if (length + bytes_received == 4) current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

            goto parse_start;
        };
        case WS_FRAME_PARSING_STATE_PAYLOAD_DATA:
        {
            uint64_t received_length = current_state->received_length;

            ssize_t bytes_received = recv(sockfd, current_state->message.buffer, current_state->real_payload_length - received_length, 0);
            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            if (current_state->frame.mask == 1)
            {
                for (size_t i = 0; i < bytes_received; ++i)
                {
                    current_state->message.buffer[i] ^= current_state->frame.masking_key[(received_length + i) % 4];
                };
            };

            printf("[wow!] %s\n", current_state->message.buffer);

            current_state->received_length += bytes_received;
            if (bytes_received == current_state->real_payload_length - received_length) break;
            else return 1;
        };
    };

    uint8_t old_fin = current_state->frame.fin;

    current_state->parsing_state = -1;
    current_state->real_payload_length = 0;
    current_state->received_length = 0;
    memset(&current_state->frame, 0, sizeof(current_state->frame));

    if (old_fin == 1) return 0;
    else return 1;
};