#include "server.h"
#include "client.h"
#include "error.h"

#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>

void on_ready(struct netc_server_t* server)
{
    printf("server is ready.\n");
};

void on_connect(struct netc_server_t* server, struct netc_client_t* client)
{
    printf("new socket connected.\n");
};

void on_disconnect(struct netc_server_t* server, struct netc_client_t* client)
{
    printf("socket disconnected.\n");
};

void on_message(struct netc_server_t* server, struct netc_client_t* client, char* message)
{
    printf("message received.\n");
};

void* thread_main(void* arg)
{
    server_listen((struct netc_server_t*)arg);
    return NULL;
};

int main()
{
    // create a server
    struct netc_server_t* server = malloc(sizeof(struct netc_server_t));
    server->on_ready = on_ready;
    server->on_connect = on_connect;
    server->on_disconnect = on_disconnect;
    server->on_message = on_message;

    struct netc_server_config config = { .port = 8080, .max_connections = 10, .backlog = 3 };

    server_init(server, config);
    server_bind(server);

    // create a thread
    pthread_t thread;
    pthread_create(&thread, NULL, thread_main, NULL);

    printf("server listening on port %d\n", config.port);

    struct netc_client_t* client = malloc(sizeof(struct netc_client_t));
    client_init(client, (struct netc_client_config){ .port = 8080, .ipv6 = 0 });
    client_connect(client);
    printf("client connected to server\n");

    // // send msg to server
    // int rs = client_send(client, "hello from client", 18);
    // printf("%d\n", rs);
    // printf("client sent message\n");
};