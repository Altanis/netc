#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <openssl/sha.h>

#if __has_include(<endian.h>)
    #include <endian.h>
#elif __has_include(<machine/endian.h>)
    #include <machine/endian.h>
#endif

#include "../../include/web/server.h"
#include "../../include/ws/server.h"
#include "../../include/tcp/server.h"

void ws_build_masking_key(uint8_t masking_key[4])
{
    __thread uint8_t seed = 0;

    masking_key[0] = seed++ * 97;
    masking_key[1] = seed++ * 97;
    masking_key[2] = seed++ * 97;
    masking_key[3] = seed++ * 97;
};

void ws_build_frame(struct ws_frame *frame, uint8_t fin, uint8_t rsv1, uint8_t rsv2, uint8_t rsv3, uint8_t opcode, uint8_t mask, uint8_t masking_key[4], uint64_t payload_length)
{
    frame->header.fin = fin;
    frame->header.rsv1 = rsv1;
    frame->header.rsv2 = rsv2;
    frame->header.rsv3 = rsv3;
    frame->header.opcode = opcode;
    frame->mask = mask;
    frame->payload_length = payload_length;

    if (mask == 1)
    {
        memcpy(frame->masking_key, masking_key, sizeof(frame->masking_key));
    };
};

int ws_send_frame(struct web_client *client, struct ws_frame *frame, const char *payload_data, size_t num_frames)
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

int ws_parse_frame(struct web_client *client, struct ws_frame_parsing_state *current_state, size_t MAX_PAYLOAD_LENGTH)
{
    printf("isolation. %d\n", current_state->payload_data.size);
    
    socket_t sockfd = client->tcp_client->sockfd;

    char lookahead[4096];
    int size = recv(sockfd, lookahead, 4096, MSG_PEEK);
    printf("lookahead %d:\n", size);
    for (int i = 0; i < size; ++i) printf("%x ", (uint8_t)lookahead[i]);
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

                if (current_state->real_payload_length + current_state->payload_data.size > MAX_PAYLOAD_LENGTH)
                {
                    printf("%d %d\n", current_state->real_payload_length + current_state->payload_data.size, MAX_PAYLOAD_LENGTH);
                    return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;
                };
                
                if (current_state->payload_data.size == 0) 
                {
                    vector_init(&current_state->payload_data, current_state->real_payload_length + (current_state->message.opcode == WS_OPCODE_TEXT ? 1 : 0), sizeof(uint8_t));
                    printf("initialised!!!\n");
                }
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
                if (current_state->real_payload_length + current_state->payload_data.size > MAX_PAYLOAD_LENGTH)
                {
                    printf("%d %d\n", current_state->real_payload_length + current_state->payload_data.size, MAX_PAYLOAD_LENGTH);
                    return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;
                };

                if (current_state->payload_data.size == 0) 
                    vector_init(&current_state->payload_data, current_state->real_payload_length + (current_state->message.opcode == WS_OPCODE_TEXT ? 1 : 0), sizeof(uint8_t));

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
            printf("wow.\n");
            if (current_state->message.payload_length == 0) break;

            uint64_t received_length = current_state->received_length;

            printf("payload size: %D\n", current_state->payload_data.size);
            
            char *buffer_ptr = current_state->payload_data.elements + current_state->payload_data.size;
            ssize_t bytes_received = recv(sockfd, buffer_ptr, current_state->real_payload_length - received_length, 0);
            
            current_state->payload_data.size += bytes_received;
            current_state->received_length += bytes_received;

            if (bytes_received <= 0)
            {
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

            if (current_state->frame.mask == 1)
            {
                for (size_t i = 0; i < bytes_received; ++i)
                {
                    buffer_ptr[i] ^= current_state->frame.masking_key[(received_length + i) % 4];
                };
            };

            printf("FOREVER %s\n", vector_get(&current_state->payload_data, 0));
            printf("[wow!] currecv: %s notcurrrecv: %s\n", buffer_ptr, current_state->payload_data.elements);
            printf("%d %d\n", bytes_received, current_state->real_payload_length, received_length);

            if (bytes_received == current_state->real_payload_length - received_length) break;
            else return 1;
        };
    };

    uint8_t old_fin = current_state->frame.header.fin;

    printf("wow %d\n", current_state->payload_data.size);
    current_state->parsing_state = -1;
    current_state->real_payload_length = 0;
    current_state->received_length = 0;
    memset(&current_state->frame, 0, sizeof(current_state->frame));
    printf("so %d\n", current_state->payload_data.size);

    if (old_fin == 1)
    {
        printf("FOREVER %s\n", vector_get(&current_state->payload_data, 0));
        if (current_state->message.opcode == WS_OPCODE_TEXT) vector_push(&current_state->payload_data, &(char){'\0'});
        current_state->message.payload_length = current_state->payload_data.size;
        current_state->message.buffer = current_state->payload_data.elements;
        printf("%d\n", current_state->message.buffer);

        return 0;
    } else {
        printf("ull stay with me. %s\n", current_state->payload_data.elements);
        return 1;
    };
};
