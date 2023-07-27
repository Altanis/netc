#include "server.h"
#include "client.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

const int MAX_CONNECTIONS = 1;

void on_ready(struct netc_server_t* server)
{
    printf("server is ready.\n");
};

void on_connect(struct netc_server_t* server)
{
    struct netc_client_t* client = malloc(sizeof(struct netc_client_t));
    server_accept(server, client);

    printf("new socket connected. socket id: %d\n", client->socket_fd);
};

void on_disconnect(struct netc_server_t* server, struct netc_client_t* client, int is_error)
{
    printf("socket disconnected. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
};

void on_data(struct netc_server_t* server, struct netc_client_t* client)
{
    printf("message received. socket id: %d\n", client->socket_fd);

    server_send(client, "hello from server", 17);
};

void* thread_main(void* arg)
{
    int r = server_listen((struct netc_server_t*)arg);
    printf("server failed to listen %d\n", r);

    return NULL;
};

int main()
{
    // create a server
    struct netc_server_t* server = malloc(sizeof(struct netc_server_t));
    server->on_ready = on_ready;
    server->on_connect = on_connect;
    server->on_disconnect = on_disconnect;
    server->on_data = on_data;

    struct netc_server_config config = { .port = 8080, .backlog = 3, .reuse_addr = 1 };


    int re = server_init(server, config);
    if (re != 0) printf("server failed to init %d\n", re);
    int reee = server_bind(server);
    if (reee != 0) printf("server failed to bind %d\n", reee);

    printf("server listening on port %d\n", config.port);
    thread_main(server);
    // pthread_t thread;
    // pthread_create(&thread, NULL, thread_main, server);
};