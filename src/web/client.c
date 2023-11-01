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
    struct http_client_parsing_state *http_client_parsing_state = &web_client->http_client_parsing_state;

    switch (web_client->connection_type)
    {
        case CONNECTION_WS:
        {

        };
        case CONNECTION_HTTP:
        {
            int result = 0;
            if ((result = http_client_parse_response(web_client, http_client_parsing_state)) != 0)
            {
                if (result < 0)
                {
                    printf("%d\n", result);
                    if (web_client->on_http_malformed_response != NULL)
                        web_client->on_http_malformed_response(web_client, result);
                                
                    http_response_free(&web_client->http_client_parsing_state.response);

                    memset(&http_client_parsing_state->response, 0, sizeof(http_client_parsing_state->response));
                    memset(http_client_parsing_state, 0, sizeof(*http_client_parsing_state));

                    http_client_parsing_state->parsing_state = -1;
                    
                    return;
                };

                printf("WAITING.\n");
                return;
            };

            if (http_client_parsing_state->response.accept_websocket == true)
            {
                web_client->connection_type = CONNECTION_WS;
                if (web_client->on_ws_connect != NULL)
                    web_client->on_ws_connect(web_client);
            }
            else if (web_client->on_http_response != NULL)
                web_client->on_http_response(web_client, http_client_parsing_state->response);

            if (web_client->client_close_flag)
                tcp_client_close(client, 0);

            http_response_free(&web_client->http_client_parsing_state.response);

            memset(&http_client_parsing_state->response, 0, sizeof(http_client_parsing_state->response));
            memset(http_client_parsing_state, 0, sizeof(*http_client_parsing_state));

            http_client_parsing_state->parsing_state = -1;            
            break;
        };
    };
};

static void _tcp_on_disconnect(struct tcp_client *client, bool is_error)
{
    struct web_client *http_client = client->data;
    if (http_client->on_disconnect != NULL)
        http_client->on_disconnect(http_client, is_error);
};

int web_client_init(struct web_client *client, struct sockaddr address)
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

int web_client_close(struct web_client *client)
{
    return tcp_client_close(client->tcp_client, 0);
};