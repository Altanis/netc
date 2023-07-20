#include "server.h"
#include "client.h"
#include "error.h"

#include "stdlib.h"
#include <stdio.h>

int main()
{
    // create a server
    struct netc_server_t* server = server_init();
    server_bind(server);
    server_listen(server);
    printf("server listening on port %d\n", PORT);

    struct netc_client_t* client = client_init();
    printf("%s\n", "waiting for client to connect...");
    client_connect(client);
    server_accept(server, client);
    client_set_non_blocking(client);
    printf("client connected\n");

    // send msg to server
    client_send_message(client, "hello from client", 18);
    printf("client sent message\n");

    // receive msg from server
    char* buf = malloc(MAX_BUFFER_SIZE);
    int r = server_receive_message(client, buf);
    printf("%d\n", r);
    printf("client received message: %s\n", buf);
};