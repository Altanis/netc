#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <openssl/sha.h>

#include "../../include/web/server.h"
#include "../../include/ws/server.h"
#include "../../include/tcp/server.h"

#if __has_include(<endian.h>)
    #include <endian.h>
#elif __has_include(<machine/endian.h>)
    #include <machine/endian.h>
#endif

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
    if (result > 0)
    {
        client->path = strdup(sso_string_get(&request->path));
        client->connection_type = CONNECTION_WS;
        return 0;
    } else return -1;
};

int ws_server_send_frame(struct web_server *server, struct web_client *client, struct ws_frame *frame, const char *payload_data, size_t num_frames)
{
    socket_t sockfd = client->tcp_client->sockfd;

    uint64_t frame_sizes[num_frames];
    uint64_t remainder = frame->payload_length % num_frames;
    uint64_t split_payload_len = frame->payload_length / num_frames;

    for (size_t i = 0; i < num_frames; ++i)
    {
        frame_sizes[i] = i == num_frames - 1 ? split_payload_len + remainder : split_payload_len;
    };

    size_t num_bytes_passed = 0;

    for (size_t i = 0; i < num_frames; ++i)
    {
        uint8_t header = 0;
        header |= (((i + 1 == num_frames) ? 1 : 0) << 7);
        header |= (frame->header.rsv1 << 6);
        header |= (frame->header.rsv2 << 5);
        header |= (frame->header.rsv3 << 4);
        header |= i == 0 ? frame->header.opcode : WS_OPCODE_CONTINUE;

        printf("header: %x\n", header);

        uint64_t frame_payload_length = frame_sizes[i];
        uint8_t payload_encoded = (frame_payload_length <= 125 ? frame_payload_length : (frame_payload_length <= 0xFFFF ? 126 : 127));

        uint8_t payload_length = 0;
        payload_length |= (frame->mask << 7);
        payload_length |= payload_encoded;

        // TODO(Altanis): Ensure endianness.
        uint8_t payload_length_encoded[8] = {0};
        if (payload_encoded == 126)
        {
            payload_length_encoded[0] = (frame_payload_length >> 8) & 0xFF;
            payload_length_encoded[1] = frame_payload_length & 0xFF;
        } else if (payload_encoded == 127)
        {
            for (int i = 0; i < 8; ++i)
            {
                payload_length_encoded[i] = (frame_payload_length >> (8 * (7 - i))) & 0xFF;
            };
        };

        const char *payload_masking_key = frame->mask == 1 ? (const char *)frame->masking_key : NULL;
        const char *payload_data_encoded = payload_data;
        
        if (payload_masking_key != NULL)
        {
            payload_data_encoded = strdup(payload_data);
            for (size_t i = 0; i < frame_payload_length; ++i)
            {
                ((char *)payload_data_encoded)[i] ^= payload_masking_key[i % 4];
            };
        };

        ssize_t result = 0;
        char frame_data[sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)) + (payload_masking_key != NULL ? 4 : 0) + frame_payload_length];

        memcpy(frame_data, &header, sizeof(header));
        memcpy(frame_data + sizeof(header), &payload_length, sizeof(payload_length));

        if (payload_encoded == 126) memcpy(frame_data + sizeof(header) + sizeof(payload_length), payload_length_encoded, 2);
        else if (payload_encoded == 127) memcpy(frame_data + sizeof(header) + sizeof(payload_length), payload_length_encoded, 8);
        if (payload_masking_key != NULL) memcpy(frame_data + sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)), payload_masking_key, 4);
        
        memcpy(frame_data + sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)) + (payload_masking_key != NULL ? 4 : 0), payload_data_encoded + num_bytes_passed, frame_payload_length);
        num_bytes_passed += frame_payload_length;

        if (payload_masking_key != NULL) free(payload_data_encoded);
        if ((result = tcp_server_send(sockfd, frame_data, sizeof(frame_data), 0)) <= 0) return result;
    };

    return 1;
};

// error frames fail (reproduce by sending an invalid frame)
int ws_server_parse_frame(struct web_server *server, struct web_client *client, struct ws_frame_parsing_state *current_state)
{
    socket_t sockfd = client->tcp_client->sockfd;

    size_t MAX_PAYLOAD_LENGTH = server->ws_server_config.max_payload_len ? server->ws_server_config.max_payload_len : 65536;

    char lookahead[4096];
    int size = recv(sockfd, lookahead, 4096, MSG_PEEK);
    printf("lookahead: ");
    for (int i = 0; i < size; ++i) printf("%x ", (unsigned char)lookahead[i]);
    printf("\n");

parse_start:
    printf("state: %d\n", current_state->parsing_state);
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

            current_state->frame.header.fin = (byte & 0b10000000) >> 7;
            current_state->frame.header.rsv1 = (byte & 0b01000000) >> 6;
            current_state->frame.header.rsv2 = (byte & 0b00100000) >> 5;
            current_state->frame.header.rsv3 = (byte & 0b00010000) >> 4;
            current_state->frame.header.opcode = byte & 0b00001111;

            if (current_state->frame.header.opcode != WS_OPCODE_CONTINUE)
                current_state->message.opcode = current_state->frame.header.opcode;

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
                printf("payload len: %d\n", current_state->frame.payload_length);
                current_state->real_payload_length = current_state->frame.payload_length;
                
                if (current_state->frame.header.opcode == WS_OPCODE_TEXT) current_state->message.buffer = calloc(current_state->real_payload_length + 1, sizeof(char));
                else current_state->message.buffer = calloc(current_state->real_payload_length, sizeof(char));
                
                current_state->message.payload_length = current_state->real_payload_length;

                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            }
            else if (current_state->frame.payload_length == 126) num_bytes_recv = 2;
            else if (current_state->frame.payload_length == 127) num_bytes_recv = 8;
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
                printf("recv <= 0\n");
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            printf("current payload len: %d\n", current_state->real_payload_length);

#if __BYTE_ORDER__ == __BIG_ENDIAN
            for (ssize_t i = 0; i < bytes_received; ++i)
            {
                current_state->real_payload_length <<= 8;
                current_state->real_payload_length |= (value >> (8 * i)) & 0xFF;
            };
#else
            for (ssize_t i = bytes_received - 1; i >= 0; --i)
            {
                current_state->real_payload_length |= ((uint64_t)(value & 0xFF) << (8 * i));
                value >>= 8;
            };
#endif

            if (bytes_received == num_bytes_recv - length)
            {
                if (current_state->real_payload_length > MAX_PAYLOAD_LENGTH)
                {
                    printf("%d %d\n", current_state->real_payload_length, MAX_PAYLOAD_LENGTH);
                    return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;
                };

                if (current_state->frame.header.opcode == WS_OPCODE_TEXT) current_state->message.buffer = calloc(current_state->real_payload_length + 1, sizeof(char));
                else current_state->message.buffer = calloc(current_state->real_payload_length, sizeof(char));

                current_state->message.payload_length = current_state->real_payload_length;

                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            } else return 1;
        };
        case WS_FRAME_PARSING_STATE_MASKING_KEY:
        {
            uint8_t *masking_key = current_state->frame.masking_key;
            ssize_t length = masking_key[0] == 0 ? 0 : (masking_key[1] == 0 ? 1 : (masking_key[2] == 0 ? 2 : (masking_key[3] == 0 ? 3 : 4)));

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
            if (current_state->message.payload_length == 0) break;

            uint64_t received_length = current_state->received_length;
            printf("recvvvvv length: %d\n", current_state->real_payload_length);

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
            printf("%d %d\n", bytes_received, current_state->real_payload_length, received_length);

            current_state->received_length += bytes_received;
            if (bytes_received == current_state->real_payload_length - received_length) break;
            else return 1;
        };
    };

    uint8_t old_fin = current_state->frame.header.fin;

    current_state->parsing_state = -1;
    current_state->real_payload_length = 0;
    current_state->received_length = 0;
    memset(&current_state->frame, 0, sizeof(current_state->frame));

    if (old_fin == 1) return 0;
    else return 1;
};