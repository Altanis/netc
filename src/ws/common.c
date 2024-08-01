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

static __thread int seed = 0;

void ws_build_masking_key(uint8_t masking_key[4])
{
    masking_key[0] = seed++ * 97;
    masking_key[1] = seed++ * 97;
    masking_key[2] = seed++ * 97;
    masking_key[3] = seed++ * 97;
};

void ws_build_message(struct ws_message *message, uint8_t opcode, uint64_t payload_length, uint8_t *payload_data)
{
    message->opcode = opcode;
    message->payload_length = payload_length;
    message->buffer = payload_data;
};

int ws_send_message(struct web_client *client, struct ws_message *message, uint8_t masking_key[4], size_t num_frames)
{
    socket_t sockfd = client->tcp_client->sockfd;

    bool mask = masking_key != NULL;

    uint64_t frame_sizes[num_frames];
    uint64_t remainder = message->payload_length % num_frames;
    uint64_t split_payload_len = message->payload_length / num_frames;

    for (size_t i = 0; i < num_frames; ++i)
    {
        frame_sizes[i] = i == num_frames - 1 ? split_payload_len + remainder : split_payload_len;
    };

    size_t num_bytes_passed = 0;

    for (size_t i = 0; i < num_frames; ++i)
    {
        uint8_t header = 0;
        header |= (((i + 1 == num_frames) ? 1 : 0) << 7);
        header |= (0 << 6);
        header |= (0 << 5);
        header |= (0 << 4);
        header |= i == 0 ? message->opcode : WS_OPCODE_CONTINUE;

        uint64_t frame_payload_length = frame_sizes[i];
        uint8_t payload_encoded = (frame_payload_length <= 125 ? frame_payload_length : (frame_payload_length <= 0xFFFF ? 126 : 127));

        uint8_t payload_length = 0;
        payload_length |= (mask << 7);
        payload_length |= payload_encoded;

        // TODO(Altanis): Ensure endianness.
        uint8_t payload_length_encoded[8] = {0};
        if (payload_encoded == 126)
        {
            payload_length_encoded[0] = (frame_payload_length >> 8) & 0xFF;
            payload_length_encoded[1] = frame_payload_length & 0xFF;
        } 
        else if (payload_encoded == 127)
        {
            for (int i = 0; i < 8; ++i)
            {
                payload_length_encoded[i] = (frame_payload_length >> (8 * (7 - i))) & 0xFF;
            };
        };

        uint8_t *payload_masking_key = NULL;
        char *payload_data_encoded = NULL;

        if (message->payload_length != 0)
        {
            payload_masking_key = mask ? (uint8_t *)masking_key : NULL;
            payload_data_encoded = (char *)message->buffer;
            
            if (payload_masking_key != NULL)
            {
                if (message->opcode == WS_OPCODE_TEXT)
                {
                    payload_data_encoded = strdup((char *)message->buffer);
                }
                else
                {
                    payload_data_encoded = malloc(payload_length);
                    memcpy((void *)payload_data_encoded, message->buffer, frame_payload_length);
                }

                for (size_t i = 0; i < frame_payload_length; ++i)
                {
                    ((uint8_t *)payload_data_encoded)[i] ^= payload_masking_key[i % 4];
                };
            };
        };

        ssize_t result = 0;
        char frame_data[sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)) + (payload_masking_key != NULL ? 4 : 0) + frame_payload_length];

        memcpy(frame_data, &header, sizeof(header));
        memcpy(frame_data + sizeof(header), &payload_length, sizeof(payload_length));

        if (payload_encoded == 126) memcpy(frame_data + sizeof(header) + sizeof(payload_length), payload_length_encoded, 2);
        else if (payload_encoded == 127) memcpy(frame_data + sizeof(header) + sizeof(payload_length), payload_length_encoded, 8);
        if (payload_masking_key != NULL) memcpy(frame_data + sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)), payload_masking_key, 4);
        
        if (payload_data_encoded != NULL)
        {
            memcpy(frame_data + sizeof(header) + sizeof(payload_length) + (payload_encoded == 126 ? 2 : (payload_encoded == 127 ? 8 : 0)) + (payload_masking_key != NULL ? 4 : 0), payload_data_encoded + num_bytes_passed, frame_payload_length);
            num_bytes_passed += frame_payload_length;
        };

        if (payload_masking_key != NULL) free((void *)payload_data_encoded);
        if ((result = tcp_server_send(sockfd, frame_data, sizeof(frame_data), 0)) <= 0) return result;
    };

    return 1;
};

int ws_parse_frame(struct web_client *client, struct ws_frame_parsing_state *current_state, size_t MAX_PAYLOAD_LENGTH)
{
    socket_t sockfd = client->tcp_client->sockfd;

parse_start:
    switch (current_state->parsing_state)
    {
        case WS_FRAME_NIL:
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
            void *ptr_to_null = memchr((void *)&current_state->real_payload_length, 0x00, sizeof(uint64_t));

            if (ptr_to_null == NULL)
            {
                if (current_state->frame.mask == 1)
                {
                    memset(current_state->frame.masking_key, 0, sizeof(current_state->frame.masking_key));
                    current_state->parsing_state = WS_FRAME_PARSING_STATE_MASKING_KEY;
                } else current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;

                goto parse_start;
            };

            size_t length = (uint8_t *)ptr_to_null - (uint8_t *)&current_state->real_payload_length;
            size_t num_bytes_recv = 0;

            if (current_state->frame.payload_length <= 125) 
            {
                current_state->real_payload_length = current_state->frame.payload_length;

                if (current_state->real_payload_length + current_state->payload_data.size > MAX_PAYLOAD_LENGTH)
                    return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;
                
                if (current_state->payload_data.size == 0) 
                    vector_init(&current_state->payload_data, current_state->real_payload_length + (current_state->message.opcode == WS_OPCODE_TEXT ? 1 : 0), sizeof(uint8_t));

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
                if (errno == EWOULDBLOCK) return 1;
                else return WS_FRAME_PARSE_ERROR_RECV;
            };

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
                    return WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG;

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

            if (length + bytes_received == 4)
            {
                current_state->parsing_state = WS_FRAME_PARSING_STATE_PAYLOAD_DATA;
            }

            goto parse_start;
        };
        case WS_FRAME_PARSING_STATE_PAYLOAD_DATA:
        {
            if (current_state->message.payload_length == 0) break;

            uint64_t received_length = current_state->received_length;

            vector_resize(&current_state->payload_data, current_state->payload_data.size + current_state->real_payload_length - received_length);
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

            if (bytes_received == current_state->real_payload_length - received_length)
            {
                break;
            }
            else return 1;
        };
    };

    uint8_t old_fin = current_state->frame.header.fin;

    current_state->parsing_state = -1;
    current_state->real_payload_length = 0;
    current_state->received_length = 0;
    memset(&current_state->frame, 0, sizeof(current_state->frame));

    if (old_fin == 1)
    {
        if (current_state->message.opcode == WS_OPCODE_TEXT) vector_push(&current_state->payload_data, &(char){'\0'});
        current_state->message.payload_length = current_state->payload_data.size;
        current_state->message.buffer = current_state->payload_data.elements;

        return 0;
    } else return 1;
};
