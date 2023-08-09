#include "http/server.h"
#include "utils/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__thread int netc_http_server_listening = 0;

int http_server_main_loop(struct http_server* server)
{
    netc_http_server_listening = 1;
};

int http_server_init(struct http_server* server, struct netc_tcp_server_config config)
{
    if (server == NULL) return -1;
    server->server = malloc(sizeof(struct netc_tcp_server));

    int init_result = 0;
    if ((init_result = tcp_server_init(server->server, config)) != 0) return init_result; 
};

int http_server_start(struct http_server* server)
{
    if (server == NULL) return -1;

    if (tcp_server_bind(server) != 0) return netc_error(BIND);
    if (tcp_server_listen(server) != 0) return netc_error(LISTEN);

    if (server->server->non_blocking) return tcp_server_main_loop(server->server);

    return 0;
};

int http_server_parse_request(struct http_server* server, char* buffer, struct http_request* request)
{
    if (server == NULL || buffer == NULL || request == NULL) return -1;

    char* line = strtok(buffer, "\r\n");
    if (line == NULL) return -1;

    char* method = strtok(line, " ");
    if (method == NULL) return -1;

    char* path = strtok(NULL, " ");
    if (path == NULL) return -1;

    char* version = strtok(NULL, " ");
    if (version == NULL) return -1;

    request->method = http_method_from_string(method);
    request->path = path;
    request->version = version;

    while ((line = strtok(NULL, "\r\n")) != NULL)
    {
        char* name = strtok(line, ":");
        if (name == NULL) return -1;

        char* value = strtok(NULL, ":");
        if (value == NULL) return -1;

        struct http_header* header = malloc(sizeof(struct http_header));
        header->name = name;
        header->value = value;

        vector_push(request->headers, header);
    }

    return 0;
};