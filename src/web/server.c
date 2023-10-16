#include "../../include/web/server.h"
#include "../../include/utils/error.h"

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

    struct web_client *client = malloc(sizeof(struct web_client));
    client->client = malloc(sizeof(struct tcp_client));
    client->server_close_flag = 0;
    client->connection_type = CONNECTION_HTTP /** default */;

    tcp_server_accept(server, client->client);

    printf("[sockfd] %d\n", client->client->sockfd);
    map_set(&http_server->clients, &(socket_t){client->client->sockfd}, client, sizeof(client->client->sockfd));

    if (http_server->on_connect != NULL)
        http_server->on_connect(http_server, client);
};

static void _tcp_on_data(struct tcp_server *server, socket_t sockfd)
{
    struct web_server *http_server = server->data;
    printf("[sockfd data] %d\n", sockfd);
    struct web_client *client = map_get(&http_server->clients, &sockfd, sizeof(sockfd));

    if (client == NULL) printf("what.\n");

    switch (client->connection_type)
    {
        case CONNECTION_WS:
        {
            printf("websocket data.\n");
            break;
        };

        case CONNECTION_HTTP:
        {
            struct http_server_parsing_state current_state = client->server_parsing_state;

            int result = 0;
            if ((result = http_server_parse_request(http_server, client, &current_state)) != 0)
            {
                if (result < 0)
                {
                    /** Malformed request. */
                    if (http_server->on_http_malformed_request)
                    {
                        /** TODO(Altanis): Fix one HTTP request partitioned into two causing two event calls. */
                        printf("malformed request: %d\n", result);
                        http_server->on_http_malformed_request(http_server, client, result);
                    }
                };

                // > 0 means the http request is incomplete and waiting for incoming data
                printf("WAITING FOR INCOMING DATA...\n");
                return;
            };

            char *path = strdup(sso_string_get(&current_state.request.path));
            /** Check for query strings to parse. */
            char *query_string = strchr(path, '?');
            if (query_string != NULL)
            {
                path[query_string - path] = '\0';

                vector_init(&current_state.request.query, 8, sizeof(struct http_query));

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

                    vector_push(&current_state.request.query, &query);
                };
            };

            struct web_server_route *route = web_server_find_route(http_server, path);
            if (route == NULL)
            {
                // That comedian...
                const char *notfound_message = "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 9\r\n"
                    "\r\n"
                    "Not Found";

                tcp_server_send(sockfd, (char *)notfound_message, strlen(notfound_message), 0);
                
                memset(&current_state.request, 0, sizeof(current_state.request));
                current_state.parsing_state = -1;

                free(path);

                return;        
            };

            if (current_state.request.upgrade_websocket == 1)
            {
                void (*handshake_request_cb)(struct web_server *server, struct web_client *client, struct http_request request) = route->on_ws_handshake_request;
                if (handshake_request_cb == NULL)
                {
                    // That comedian...
                    const char *badrequest_message = "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 38\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "Upgrade to WebSocket is not supported.";
                    
                    tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
                }
                else handshake_request_cb(http_server, client, current_state.request);
            }
            else
            {
                void (*callback)(struct web_server *server, struct web_client *client, struct http_request request) = route->on_http_message;
                if (callback != NULL) callback(http_server, client, current_state.request);
            };

            memset(&current_state.request, 0, sizeof(current_state.request));
            current_state.parsing_state = -1;

            free(path);

            break;
        };
    };
};

static void _tcp_on_disconnect(struct tcp_server *server, socket_t sockfd, int is_error)
{
    struct web_server *http_server = server->data;
    struct web_client *client = map_get(&http_server->clients, &sockfd, sizeof(sockfd));

    if (http_server->on_disconnect != NULL)
        http_server->on_disconnect(http_server, sockfd, is_error);
};

int web_server_init(struct web_server *http_server, struct sockaddr address, int backlog)
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

    int bind_result = tcp_server_bind(tcp_server);
    if (bind_result != 0) return bind_result;

    int listen_result = tcp_server_listen(tcp_server, backlog);
    if (listen_result != 0) return listen_result;

    tcp_server->on_connect = _tcp_on_connect;
    tcp_server->on_data = _tcp_on_data;
    tcp_server->on_disconnect = _tcp_on_disconnect;

    http_server->server = tcp_server;

    return 0;
};

int web_server_start(struct web_server *server)
{
    return tcp_server_main_loop(server->server);
};

void web_server_create_route(struct web_server *server, struct web_server_route *route)
{
    vector_push(&server->routes, route);
};

struct web_server_route *web_server_find_route(struct web_server *server, const char *path)
{
    struct web_server_route *return_route = NULL;

    for (size_t i = 0; i < server->routes.size; ++i)
    {
        struct web_server_route *route = vector_get(&server->routes, i);
        if (_path_matches(path, route->path))
        {
            return_route = route;
            break;
        };
    };

    return return_route;
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
    return tcp_server_close_self(server->server);
};

int web_server_close_client(struct web_server *server, struct web_client *client)
{
    return tcp_server_close_client(server->server, client->client->sockfd, 0);
};