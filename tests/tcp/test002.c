#ifndef TEST_002
#define TEST_002

/**
 * TEST CASE 2: blocking server and client
*/

#include "tcp/server.h"
#include "tcp/client.h"
#include "utils/error.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define IP "127.0.0.1"
#define PORT 8080
#define BACKLOG 3
#define REUSE_ADDRESS 1
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 0
#define CLIENT_NON_BLOCKING 0

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

/** At the end of this test, all of these values must equal 1. */
static int test002_server_connect = 0;
static int test002_server_data = 0;
static int test002_server_disconnect = 0;
static int test002_client_connect = 0;
static int test002_client_data = 0;
static int test002_client_disconnect = 0;

static void* server_thread_blocking_main(void* arg);
static void* client_thread_blocking_main(void* arg);

static void* server_thread_blocking_main(void* arg)
{
    struct netc_server* server = (struct netc_server*)arg;
    struct netc_client* client = malloc(sizeof(struct netc_client));

    int accept_result;
    if ((accept_result = server_accept(server, client)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to accept incoming connection\nerrno: %d\nerrno reason: %d\n%s", accept_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_server_connect++;
        printf("[TEST CASE 002] new socket connected. socket id: %d\n", client->socket_fd);
    }

    int recv_result;
    char buffer[18];
    if ((recv_result = server_receive(server, client, &buffer, 17)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to receive data from client\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_server_data++;
        printf("[TEST CASE 002] server received data from client! %s\n", buffer);
    }

    int send_result;
    if ((send_result = server_send(client, "hello from server", 17)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to send data to client\nerrno: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        printf("[TEST CASE 002] server sent data to client!\n");
    }

    // check if recv == 0
    int disconnect_result;
    if ((disconnect_result = server_receive(server, client, &buffer, 18)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to disconnect from client\nerrno: %d\nerrno reason: %d\n%s", disconnect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_server_disconnect++;
        printf("[TEST CASE 002] server disconnected from client!\n");
    }

    return NULL;
};

static void* client_thread_blocking_main(void* arg)
{
    struct netc_client* client = (struct netc_client*)arg;

    int connect_result = 0;
    if ((connect_result = client_connect(client)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] client failed to connect\nerrno: %d\nerrno reason: %d\n%s", connect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_client_connect++;
        printf("[TEST CASE 002] client connected to server!\n");
    }

    int send_result = 0;
    if (client_send(client, "hello from client", 17) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] client failed to send data to server\nerrno: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        printf("[TEST CASE 002] client sent data to server!\n");
    }

    int recv_result = 0;
    char buffer[18];
    if ((recv_result = client_receive(client, &buffer, 17)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] client failed to receive data from server\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_client_data++;
        printf("[TEST CASE 002] client received data from server! %s\n", buffer);
    }

    int disconnect_result = 0;
    if ((disconnect_result = client_close(client, 0)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] client failed to disconnect from server\nerrno: %d\nerrno reason: %d\n%s", disconnect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        test002_client_disconnect++;
        printf("[TEST CASE 002] client disconnected from server!\n");
    }

    return NULL;
};

static int test002()
{
    struct netc_server* server = malloc(sizeof(struct netc_server));
    struct netc_server_config server_config = {
        .port = PORT,
        .backlog = BACKLOG,
        .reuse_addr = REUSE_ADDRESS,
        .ipv6 = USE_IPV6,
        .non_blocking = SERVER_NON_BLOCKING
    };

    int init_result = 0;
    if ((init_result = server_init(server, server_config)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = server_bind(server)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to bind\nerrno: %d\nerrno reason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int listen_result = 0;
    if ((listen_result = server_listen(server)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] server failed to listen\nerrno: %d\nerrno reason: %d\n%s", listen_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[TEST CASE 002] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_thread_blocking_main, server);

    struct netc_client* client = malloc(sizeof(struct netc_client));
    struct netc_client_config client_config = {
        .ip = IP,
        .port = PORT,
        .ipv6_connect_from = USE_IPV6,
        .ipv6_connect_to = USE_IPV6,
        .non_blocking = CLIENT_NON_BLOCKING
    };

    int client_init_result = 0;
    if ((client_init_result = client_init(client, client_config)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 002] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", client_init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    client_thread_blocking_main(client);
    pthread_join(server_thread, NULL);

    if (test002_server_connect != 1) printf(ANSI_RED "\n\n\n[SERVER_CONNECT] server failed to accept incoming connection\n" ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[SERVER_CONNECT] server accepted incoming connection\n" ANSI_RESET);

    if (test002_server_data != 1) printf(ANSI_RED "[SERVER_DATA] server failed to receive data from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DATA] server received data from client\n" ANSI_RESET);

    if (test002_server_disconnect != 1) printf(ANSI_RED "[SERVER_DISCONNECT] server failed to disconnect from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DISCONNECT] server disconnected from client\n" ANSI_RESET);

    if (test002_client_connect != 1) printf(ANSI_RED "[CLIENT_CONNECT] client failed to connect to server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_CONNECT] client connected to server\n" ANSI_RESET);

    if (test002_client_data != 1) printf(ANSI_RED "[CLIENT_DATA] client failed to receive data from server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DATA] client received data from server\n" ANSI_RESET);

    if (test002_client_disconnect != 1) printf(ANSI_RED "[CLIENT_DISCONNECT] client failed to disconnect from server\n\n\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DISCONNECT] client disconnected from server\n\n\n" ANSI_RESET);

    return 
        (int)(!(test002_server_connect == 1 && test002_server_data == 1 && test002_server_disconnect == 1 && test002_client_connect == 1 && test002_client_data == 1 && test002_client_disconnect == 1));
};

#endif // TEST_002