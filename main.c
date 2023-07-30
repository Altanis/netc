#include "tcp/server.h"
#include "tcp/client.h"
#include "utils/error.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

struct netc_server* server = NULL;

void server_on_connect(struct netc_server* server)
{
    struct netc_client* client = malloc(sizeof(struct netc_client));
    server_accept(server, client);

    printf("new socket connected. socket id: %d\n", client->socket_fd);
};

void client_on_connect(struct netc_client* client)
{
    printf("client connected to server! wowee!!!!!\n");

    client_send(client, "hello from client", 17);
};

void server_on_disconnect(struct netc_server* server, struct netc_client* client, int is_error)
{
    printf("socket disconnected. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
};

void client_on_disconnect(struct netc_client* client, int is_error)
{
    printf("client disconnected from server. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
};

void server_on_data(struct netc_server* server, struct netc_client* client)
{
    printf("message received. socket id: %d\n", client->socket_fd);

    server_send(client, "hello from server", 17);
};

void client_on_data(struct netc_client* client)
{
    printf("message received from server!\n");
    // if (client_close(client, 0) != 0) printf("client failed to close %d\n", errno);
    // else printf("client closed\n");
    server_close_client(server, client, 0);
};

void* server_thread_nonblocking_main(void* arg)
{
    int r = server_main_loop((struct netc_server*)arg);
    printf("server failed to listen %d %d\n", r, netc_errno_reason);

    return NULL;
};

void* client_thread_nonblocking_main(void* arg)
{
    int r = client_main_loop((struct netc_client*)arg);
    printf("client failed to listen %d %d\n", r, netc_errno_reason);

    return NULL;
};

void* server_thread_blocking_main(void* arg)
{
    struct netc_server* server = (struct netc_server*)arg;
    struct netc_client* client = malloc(sizeof(struct netc_client));
    server_accept(server, client);

    printf("new socket connected. socket id: %d\n", client->socket_fd);

    char* buffer = malloc(17);
    server_receive(server, client, buffer, 17);
    printf("message received from client: %s\n", buffer);

    char* str = "hi";
    server_send(client, str, 3);

    return NULL;
};

void* client_thread_blocking_main(void* arg)
{
    // struct netc_client* client = malloc(sizeof(struct netc_client));
    // server_accept(server, client);
    // printf("new socket connected. socket id: %d\n", client->socket_fd);
    // char* buffer = malloc(2);
    // server_receive(server, client, buffer, 2);
    // printf("message received from client: %s\n", buffer);
    // char* str = "hello from server";
    // server_send(client, str, 18);
    // printf("message sent to client: %s\n", str);

    return NULL;
};


int main()
{
    server = malloc(sizeof(struct netc_server));

    // create a server
    server->on_connect = server_on_connect;
    server->on_disconnect = server_on_disconnect;
    server->on_data = server_on_data;

    struct netc_server_config config = { .port = 8080, .backlog = 3, .reuse_addr = 1, .ipv6 = 0, .non_blocking = 1 };


    if (server_init(server, config) != 0) printf("server failed to init %d\n", errno);
    if (server_bind(server) != 0) printf("server failed to bind %d\n", errno);
    if (server_listen(server) != 0) printf("server failed to listen %d\n", errno);

    printf("server listening on port %d\n", config.port);
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread_nonblocking_main, server);

    struct netc_client* client = malloc(sizeof(struct netc_client));
    client->on_connect = client_on_connect;
    client->on_disconnect = client_on_disconnect;
    client->on_data = client_on_data;

    struct netc_client_config client_config = {
        .ipv6_connect_from = 0,
        .ipv6_connect_to = 0,
        .ip = "127.0.0.1",
        .port = 8080,
        .non_blocking = 1
    };

    if (client_init(client, client_config) != 0) printf("client failed to init\n");

    pthread_t thread2;
    pthread_create(&thread2, NULL, client_thread_nonblocking_main, client);

    if (client_connect(client) != 0) printf("client failed to connect %d\n", errno);

    while (1) {};
};