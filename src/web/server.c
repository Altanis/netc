#include "../../include/web/server.h"
#include "../../include/utils/error.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#elif __APPLE__
#include <sys/event.h>
#endif

int _path_matches(const char *path, const char *pattern) 
{
    if (strcmp(path, pattern) == 0) return 1;

    while (*pattern) 
    {
        if (*pattern == '*') 
        {
            /** Check for end of path. */
            if (*(pattern + 1) == '\0') return 1;

            /** Recursively try all positions for the wildcard match. */
            while (*path)
            {
                if (_path_matches(path, pattern + 1)) return 1;
                else ++path;
            }
            
            return 0;
        } 
        else
        {
            if (*path != *pattern) return 0;
            
            ++path;
            ++pattern;
        }
    }

    return (*path == '\0');
};

static void _tcp_on_connect(struct tcp_server *server)
{
    struct web_server *http_server = server->data;

    struct web_client *client = calloc(1, sizeof(struct web_client));
    client->tcp_client = malloc(sizeof(struct tcp_client));
    client->server_close_flag = 0;
    client->connection_type = CONNECTION_HTTP /** default */;

    tcp_server_accept(server, client->tcp_client);

    socket_t sockfd = client->tcp_client->sockfd;

    map_set(&http_server->clients, sockfd, client);

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, client);
};

static void _tcp_on_data(struct tcp_server *server, socket_t sockfd)
{
    struct web_server *web_server = server->data;
    struct web_client *client = map_get(&web_server->clients, sockfd);
    if (client == NULL) return;

    switch (client->connection_type)
    {
        case CONNECTION_WS:
        {
            struct ws_frame_parsing_state *ws_parsing_state = &client->ws_parsing_state;
            struct web_server_route *route = web_server_find_route(web_server, client->path);

            if (route == NULL) return;

            int result = 0;

            if ((result = ws_parse_frame(client, ws_parsing_state, web_server->ws_server_config.max_payload_len ? web_server->ws_server_config.max_payload_len : 65536)) != 0)
            {
                if (result < 0)
                {
                    /** TODO(Altanis): Fix one WS request partitioned into two causing two event calls. */
                    /** Malformed request. */
                    if (route->on_ws_malformed_frame != NULL)
                        route->on_ws_malformed_frame(web_server, client, result);
                    ws_server_close_client(web_server, client, 1002, "Malformed frame.");
                };

                return;
            };

            if (ws_parsing_state->message.opcode == WS_OPCODE_PING || ws_parsing_state->message.opcode == WS_OPCODE_PONG)
            {
                if (route->on_heartbeat != NULL)
                    route->on_heartbeat(web_server, client, &ws_parsing_state->message);
                
                if (
                    (web_server->ws_server_config.record_latency == true && ws_parsing_state->message.opcode == WS_OPCODE_PONG) // pong + record_latency
                    || (ws_parsing_state->message.opcode == WS_OPCODE_PING) // ping
                )
                {
                    struct ws_message message;
                    ws_build_message(&message, ws_parsing_state->message.opcode == WS_OPCODE_PING ? WS_OPCODE_PONG : WS_OPCODE_PING, ws_parsing_state->message.payload_length, ws_parsing_state->message.buffer);
                    ws_send_message(client, &message, NULL, 1);
                };
            }
            else if (ws_parsing_state->message.opcode == WS_OPCODE_CLOSE && route->on_ws_close)
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

                free(client->path);
                client->path = NULL;
                
                tcp_server_close_client(server, client->tcp_client->sockfd, false);
                route->on_ws_close(web_server, client, close_code, message);
            } else if (route->on_ws_message != NULL) route->on_ws_message(web_server, client, &ws_parsing_state->message);

            free(ws_parsing_state->message.buffer);
            memset(ws_parsing_state, 0, sizeof(struct ws_frame_parsing_state));

            break;
        };

        case CONNECTION_HTTP:
        {
            int result = 0;
            if ((result = http_server_parse_request(web_server, client, &client->http_server_parsing_state)) != 0)
            {
                if (result < 0)
                {
                    /** Malformed request. */
                    if (web_server->on_http_malformed_request)
                    {
                        /** TODO(Altanis): Fix one HTTP request partitioned into two causing two event calls. */
                        web_server->on_http_malformed_request(web_server, client, result);
                        tcp_server_close_client(server, client->tcp_client->sockfd, true);
                    }
                }

                // > 0 means the http request is incomplete and waiting for incoming data
                return;
            };

            char *path = strdup(sso_string_get(&client->http_server_parsing_state.request.path));
            /** Check for query strings to parse. */
            char *query_string = strchr(path, '?');
            if (query_string != NULL)
            {
                path[query_string - path] = '\0';

                vector_init(&client->http_server_parsing_state.request.query, 8, sizeof(struct http_query));

                char *token = strtok(query_string + 1, "&");
                while (token != NULL)
                {
                    struct http_query query = {0};
                    sso_string_init(&query.key, "");
                    sso_string_init(&query.value, "");

                    char *value = strchr(token, '=');
                    if (value == NULL) break;

                    *value = '\0';

                    sso_string_set(&query.key, token);
                    sso_string_set(&query.value, value + 1);

                    vector_push(&client->http_server_parsing_state.request.query, &query);
                };
            };

            struct web_server_route *route = web_server_find_route(web_server, path);
            if (route == NULL)
            {
                // That comedian...
                char *notfound_message = "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 9\r\n"
                    "\r\n"
                    "Not Found";

                tcp_server_send(sockfd, (char *)notfound_message, strlen(notfound_message), 0);
                
                free(path);
                http_request_free(&client->http_server_parsing_state.request);
                memset(&client->http_server_parsing_state.request, 0, sizeof(client->http_server_parsing_state.request));
                client->http_server_parsing_state.parsing_state = -1;

                return;        
            };

            if (client->http_server_parsing_state.request.upgrade_websocket == true)
            {
                void (*handshake_request_cb)(struct web_server *server, struct web_client *client, struct http_request *request) = route->on_ws_handshake_request;
                if (handshake_request_cb == NULL)
                {
                    // That comedian...
                    char *badrequest_message = "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 38\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "Upgrade to WebSocket is not supported.";
                    
                    tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
                };

                handshake_request_cb(web_server, client, &client->http_server_parsing_state.request);
            }
            else
            {
                void (*callback)(struct web_server *server, struct web_client *client, struct http_request *request) = route->on_http_message;
                if (callback != NULL) callback(web_server, client, &client->http_server_parsing_state.request);
            };

            free(path);
            http_request_free(&client->http_server_parsing_state.request);
            memset(&client->http_server_parsing_state, 0, sizeof(client->http_server_parsing_state));
            client->http_server_parsing_state.parsing_state = -1;

            break;
        };
    };
};

static void _tcp_on_disconnect(struct tcp_server *server, socket_t sockfd, bool is_error)
{
    struct web_server *web_server = server->data;
    if (web_server->is_closing == 1) return;

    if (sockfd == server->sockfd && web_server->on_disconnect != NULL)
        return web_server->on_disconnect(web_server, sockfd, is_error);

    if (web_server->clients.entries == NULL) return;

    struct web_client *web_client = map_get(&web_server->clients, sockfd);
    if (web_client == NULL) return;

    if (web_client->connection_type == CONNECTION_HTTP)
    {
        if (web_server->on_disconnect != NULL)
            web_server->on_disconnect(web_server, sockfd, is_error);
    }
    else if (web_client->connection_type == CONNECTION_WS && web_client->path != NULL)
    {
        struct web_server_route *route = web_server_find_route(web_server, web_client->path);
        if (route != NULL && route->on_ws_close != NULL)
            route->on_ws_close(web_server, web_client, 0, NULL);

        if (web_server->is_closing == 1) return;
    
        free(web_client->path);
        web_client->path = NULL;
    };

    if (web_server->is_closing == 1) return;
    
    free(web_client->tcp_client->sockaddr);
    free(web_client->tcp_client);
    map_delete(&web_server->clients, sockfd);
};

int web_server_init(struct web_server *http_server, struct sockaddr *address, int backlog)
{
    vector_init(&http_server->routes, 8, sizeof(struct web_server_route));
    map_init(&http_server->clients, 8);

    struct tcp_server *tcp_server = malloc(sizeof(struct tcp_server));
    tcp_server->data = http_server;
    
    int init_result = tcp_server_init(tcp_server, address, 1);
    if (init_result != 0) return init_result;

    if (setsockopt(tcp_server->sockfd, SOL_SOCKET, SO_REUSEADDR, &(char){1}, sizeof(int)) < 0)
    {
        /** Not essential. Do not return -1. */
    };

    /** TODO(Altanis): These may overwrite config changes from before web_server_init() was called. */
    http_server->ws_server_config.record_latency = false;
    http_server->http_server_config.max_body_len = 0;
    http_server->http_server_config.max_header_count = 0;
    http_server->http_server_config.max_header_name_len = 0;
    http_server->http_server_config.max_header_value_len = 0;
    http_server->http_server_config.max_method_len = 0;
    http_server->http_server_config.max_path_len = 0;
    http_server->http_server_config.max_version_len = 0;
    http_server->ws_server_config.max_payload_len = 0;
    http_server->is_closing = 0;

    int bind_result = tcp_server_bind(tcp_server);
    if (bind_result != 0) return bind_result;

    int listen_result = tcp_server_listen(tcp_server, backlog);
    if (listen_result != 0) return listen_result;

    tcp_server->on_connect = _tcp_on_connect;
    tcp_server->on_data = _tcp_on_data;
    tcp_server->on_disconnect = _tcp_on_disconnect;

    http_server->tcp_server = tcp_server;

    return 0;
};

int web_server_start(struct web_server *server)
{
    return tcp_server_main_loop(server->tcp_server);
};

void web_server_create_route(struct web_server *server, struct web_server_route *route)
{
    vector_push(&server->routes, route);
};

struct web_server_route *web_server_find_route(struct web_server *server, const char *path)
{
    if (path == NULL) return NULL;

    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct web_server_route *route = vector_get(&server->routes, i);
        if (_path_matches(path, route->path))
            return route;
    };

    return NULL;
};

void web_server_remove_route(struct web_server *server, const char *path)
{
    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct web_server_route *route = vector_get(&server->routes, i);
        if (strcmp(route->path, path) == 0)
        {
            vector_delete(&server->routes, i);
            break;
        };
    };
};

int web_server_close(struct web_server *server)
{
    server->is_closing = 1;
    server->on_disconnect = NULL;

    for (size_t i = 0; i < server->clients.capacity; ++i)
    {
        struct map_entry entry = server->clients.entries[i];
        if (entry.value == NULL) continue;
        struct web_client *client = entry.value;

        if (client == NULL) continue;        

        if (client->connection_type == CONNECTION_WS)
        {
            struct ws_message message;
            ws_build_message(&message, WS_OPCODE_CLOSE, 0, NULL);
            ws_send_message(client, &message, NULL, 1);
#ifdef _WIN32
            closesocket(client->tcp_client->sockfd);
            for (size_t i = 0; i < server->tcp_server->events.size; ++i)
            {
                WSAPOLLFD *event = vector_get(&server->tcp_server->events, i);
                if (event->fd == client->tcp_client->sockfd)
                {
                    vector_delete(&server->tcp_server->events, i);
                    break;
                };
            };
#else
            close(client->tcp_client->sockfd);
#endif
        };

        free(client->path);
        free(client->tcp_client->sockaddr);
        free(client->tcp_client);
        free(client);
    };

    map_free(&server->clients, false);
    return tcp_server_close_self(server->tcp_server);
};