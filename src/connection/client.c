#include "connection/client.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _tcp_on_connect(struct tcp_client *client)
{
    struct client_connection *http_client = client->data;
    if (http_client->on_connect != NULL)
        http_client->on_connect(http_client);
};

static void _tcp_on_data(struct tcp_client *client)
{
    struct client_connection *http_client = client->data;
    struct http_client_parsing_state current_state = http_client->client_parsing_state;

    int result = 0;
    if ((result = http_client_parse_response(http_client, &current_state)) != 0)
    {
        if (result < 0)
        {
            if (http_client->on_malformed_response != NULL)
            http_client->on_malformed_response(http_client, result);
            return;
        };

        printf("WAITING.\n");
        return;
    };

    if (http_client->on_data != NULL)
        http_client->on_data(http_client, current_state.response);

    if (http_client->client_close_flag)
        tcp_client_close(client, 0);
};

static void _tcp_on_disconnect(struct tcp_client *client, int is_error)
{
    struct client_connection *http_client = client->data;
    if (http_client->on_disconnect != NULL)
        http_client->on_disconnect(http_client, is_error);
};

int client_init(struct client_connection *client, struct sockaddr address, enum connection_types connection_type)
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
    client->client_close_flag = 0;
    client->connection_type = connection_type;

    return 0;
};

int client_start(struct client_connection *client)
{
    return tcp_client_main_loop(client->client);
};

int client_close(struct client_connection *client)
{
    return tcp_client_close(client->client, 0);
};