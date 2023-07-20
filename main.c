#include "server.h"
#include "client.h"

#include <stdio.h>

int main()
{
    // create a server
    struct netc_server_t* server = server_init();
    if (server_bind(server) != 0) error("unable to bind server to socket");
    if (server_listen(server) != 0) error("unable to listen on socket");

    struct netc_client_t* client = client_init();
    printf("%s\n", "waiting for client to connect...");
    if (client_connect(client) != 0) error("unable to connect to server");
    if (server_accept(server, client) != 0) error("unable to accept connection");
    
    printf("client connected\n");
    printf("server listening on port %d\n", PORT);
}; 