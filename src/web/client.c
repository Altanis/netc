#include "../../include/web/client.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _tcp_on_connect(struct tcp_client *client)
{
    struct web_client *http_client = client->data;
    if (http_client->on_http_connect != NULL)
        http_client->on_http_connect(http_client);
};

static void _tcp_on_data(struct tcp_client *client)
{
    struct web_client *web_client = client->data;
    switch (web_client->connection_type)
    {
        case CONNECTION_WS:
        {
            struct ws_frame_parsing_state *ws_parsing_state = &web_client->ws_parsing_state;
            
            enum ws_frame_parsing_errors result = 0;
            if ((result = ws_parse_frame(web_client, ws_parsing_state, -1 /** overflows to SIZE_MAX*/)))
            {
                if (result < 0)
                {
                    if (web_client->on_ws_malformed_frame != NULL)
                        web_client->on_ws_malformed_frame(web_client, result);
                };

                return;
            };

            if (ws_parsing_state->message.opcode == WS_OPCODE_PING || ws_parsing_state->message.opcode == WS_OPCODE_PONG)
            {
                if (web_client->on_heartbeat != NULL)
                    web_client->on_heartbeat(web_client, &ws_parsing_state->message);
                
                if (
                    (web_client->ws_client_config.record_latency == true && ws_parsing_state->message.opcode == WS_OPCODE_PONG) // pong + record_latency
                    || (ws_parsing_state->message.opcode == WS_OPCODE_PING) // ping
                )
                {
                    struct ws_message message;
                    ws_build_message(&message, ws_parsing_state->message.opcode == WS_OPCODE_PING ? WS_OPCODE_PONG : WS_OPCODE_PING, ws_parsing_state->message.payload_length, ws_parsing_state->message.buffer);
                    ws_send_message(web_client, &message, NULL, 1);
                };
            }
            else if (ws_parsing_state->message.opcode == WS_OPCODE_CLOSE && web_client->on_ws_disconnect)
            {
                size_t message_size = ws_parsing_state->message.payload_length - 2 + 1;
                uint16_t close_code = 0;
                char message[message_size];

                if (ws_parsing_state->message.payload_length >= 2)
                {
                    close_code = (uint16_t)ws_parsing_state->message.buffer[0] << 8 | (uint16_t)ws_parsing_state->message.buffer[1];

                    if (ws_parsing_state->message.payload_length > 2)
                    {
                        memcpy(message, ws_parsing_state->message.buffer + 2, ws_parsing_state->message.payload_length - 2);
                        message[ws_parsing_state->message.payload_length - 2] = '\0';
                    } else message[0] = '\0';
                };

                tcp_client_close(client, false);
                web_client->on_ws_disconnect(web_client, close_code, message);
                web_client->is_closed = true;
            } else if (web_client->on_ws_message != NULL) web_client->on_ws_message(web_client, &ws_parsing_state->message);

            free(ws_parsing_state->message.buffer);
            memset(ws_parsing_state, 0, sizeof(struct ws_frame_parsing_state));
    
            break;
        };
        case CONNECTION_HTTP:
        {
            struct http_client_parsing_state *http_client_parsing_state = &web_client->http_client_parsing_state;

            int result = 0;
            if ((result = http_client_parse_response(web_client, http_client_parsing_state)) != 0)
            {
                if (result < 0)
                {
                    if (web_client->on_http_malformed_response != NULL)
                        web_client->on_http_malformed_response(web_client, result);
                                
                    http_response_free(&web_client->http_client_parsing_state.response);

                    memset(&http_client_parsing_state->response, 0, sizeof(http_client_parsing_state->response));
                    memset(http_client_parsing_state, 0, sizeof(struct http_client_parsing_state));

                    http_client_parsing_state->parsing_state = -1;
                    return;
                };

                return;
            };

            if (http_client_parsing_state->response.accept_websocket == true)
            {
                web_client->connection_type = CONNECTION_WS;
                if (web_client->on_ws_connect != NULL)
                {
                    web_client->on_ws_connect(web_client);
                    if (web_client->ws_client_config.record_latency == true)
                    {
                        struct ws_message message;
                        ws_build_message(&message, WS_OPCODE_PING, 0, NULL);
                        /** TOOO(Altanis): Notify if sending message fails. */
                        (void) ws_send_message(web_client, &message, NULL, 1);
                    };
                }
            }
            else if (web_client->on_http_response != NULL)
                web_client->on_http_response(web_client, &http_client_parsing_state->response);

            if (web_client->client_close_flag)
                tcp_client_close(client, 0);

            http_response_free(&web_client->http_client_parsing_state.response);

            memset(&http_client_parsing_state->response, 0, sizeof(http_client_parsing_state->response));
            memset(http_client_parsing_state, 0, sizeof(struct http_client_parsing_state));

            http_client_parsing_state->parsing_state = -1;            
            break;
        };
    };
};

static void _tcp_on_disconnect(struct tcp_client *client, bool is_error)
{
    struct web_client *web_client = client->data;

    if (web_client->on_http_disconnect != NULL && web_client->connection_type == CONNECTION_HTTP)
        web_client->on_http_disconnect(web_client, is_error);
    else if (web_client->on_ws_disconnect != NULL && web_client->is_closed == false && web_client->connection_type == CONNECTION_WS)
    {
        web_client->on_ws_disconnect(web_client, 0, NULL);
        web_client->is_closed = true;
    };
};

int web_client_init(struct web_client *client, struct sockaddr *address)
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

    client->tcp_client = tcp_client;
    client->client_close_flag = 0;
    client->connection_type = CONNECTION_HTTP /** default */;

    return 0;
};

int web_client_start(struct web_client *client)
{
    return tcp_client_main_loop(client->tcp_client);
};

int web_client_close(struct web_client *client, uint16_t code, const char *reason)
{
    if (client->connection_type == CONNECTION_WS)
    {
        size_t reason_length = reason != NULL ? strlen(reason) : 0;
        if (code != 0)
        {
            reason_length += 2;
        };

        char reason_buffer[reason_length];
        if (code != 0)
        {
            reason_buffer[0] = (code >> 8) & 0xFF;
            reason_buffer[1] = code & 0xFF;
            memcpy(reason_buffer + 2, reason, reason_length - 2);
        } else memcpy(reason_buffer, reason, reason_length);

        struct ws_message message;
        ws_build_message(&message, WS_OPCODE_CLOSE, strlen(reason_buffer), (uint8_t *)reason_buffer);

        (void) ws_send_message(client, &message, NULL, 1); // Doesn't matter too much if this fails.
    };

    return tcp_client_close(client->tcp_client, false);
};