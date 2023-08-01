#ifndef TEST_001
#define TEST_001

/**
 * TEST CASE 1: non-blocking server and client
*/

#include "tcp/server.h"
#include "tcp/client.h"
#include "utils/error.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define IP "127.0.0.1"
#define PORT 8080
#define BACKLOG 3
#define REUSE_ADDRESS 1
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 1
#define CLIENT_NON_BLOCKING 1

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

/** At the end of this test, all of these values must equal 1. */
static int test001_server_connect = 0;
static int test001_server_data = 0;
static int test001_server_disconnect = 0;
static int test001_client_connect = 0;
static int test001_client_data = 0;
static int test001_client_disconnect = 0;

static void* test001_server_thread_nonblocking_main(void* arg);
static void test001_server_on_connect(struct netc_tcp_server* server);
static void test001_server_on_data(struct netc_tcp_server* server, struct netc_tcp_client* client);
static void test001_on_disconnect(struct netc_tcp_server* server, struct netc_tcp_client* client, int is_error);

static void* test001_client_thread_nonblocking_main(void* arg);
static void tet001_client_on_connect(struct netc_tcp_client* client);
static void test001_client_on_data(struct netc_tcp_client* client);
static void test001_client_on_disconnect(struct netc_tcp_client* client, int is_error);

static void* test001_server_thread_nonblocking_main(void* arg)
{
    struct netc_tcp_server* server = (struct netc_tcp_server*)arg;
    int r = tcp_server_main_loop(server);
    if (r != 0) printf(ANSI_RED "[TEST CASE 001] client main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void test001_server_on_connect(struct netc_tcp_server* server)
{
    struct netc_tcp_client* client = malloc(sizeof(struct netc_tcp_client));
    tcp_server_accept(server, client);

    printf("[TEST CASE 001] new socket connected. socket id: %d\n", client->socket_fd);
    test001_server_connect++;
};

static void test001_server_on_data(struct netc_tcp_server* server, struct netc_tcp_client* client)
{    
    char* buffer = malloc(18);
    int recv_result = 0;
    if ((recv_result = tcp_server_receive(server, client, buffer, 17)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] server failed to receive\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return;
    };
    printf("[TEST CASE 001] message received. socket id: %d, message: %s\n", client->socket_fd, buffer);

    tcp_server_send(client, "hello from server", 17);
    test001_server_data++;
};

static void test001_on_disconnect(struct netc_tcp_server* server, struct netc_tcp_client* client, int is_error)
{
    printf("[TEST CASE 001] socket disconnected. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
    test001_server_disconnect++;
    tcp_server_close_self(server);
};

static void* test001_client_thread_nonblocking_main(void* arg)
{
    struct netc_tcp_client* client = (struct netc_tcp_client*)arg;
    int r = tcp_client_main_loop(client);
    if (r != 0) printf(ANSI_RED "[TEST CASE 001] client main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void tet001_client_on_connect(struct netc_tcp_client* client)
{
    printf("[TEST CASE 001] client connected to server!\n");
    test001_client_connect++;

    tcp_client_send(client, "hello from client", 17);
};

static void test001_client_on_data(struct netc_tcp_client* client)
{
    int recv_result = 0;
    char* buffer = malloc(18);
    if ((recv_result = tcp_client_receive(client, buffer, 17)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] client failed to receive\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return;
    };

    printf("[TEST CASE 001] message received from server! message: %s\n", buffer);
    test001_client_data++;

    int close_result = 0;
    if ((close_result = tcp_client_close(client, 0)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] client failed to close\nerrno: %d\nerrno reason: %d\n%s", close_result, netc_errno_reason, ANSI_RESET);
        return;
    };
};

static void test001_client_on_disconnect(struct netc_tcp_client* client, int is_error)
{
    printf("[TEST CASE 001] client disconnected from server. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
    test001_client_disconnect++;
};

static int test001()
{
    struct netc_tcp_server* server = malloc(sizeof(struct netc_tcp_server));
    if (server->on_connect) server->on_connect = test001_server_on_connect;
    server->on_data = test001_server_on_data;
    server->on_disconnect = test001_on_disconnect;

    struct netc_tcp_server_config server_config = {
        .port = PORT,
        .backlog = BACKLOG,
        .reuse_addr = REUSE_ADDRESS,
        .ipv6 = USE_IPV6,
        .non_blocking = SERVER_NON_BLOCKING
    };

    int init_result = 0;
    if ((init_result = tcp_server_init(server, server_config)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = tcp_server_bind(server)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] server failed to bind\nerrno: %d\nerrno reason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int listen_result = 0;
    if ((listen_result = tcp_server_listen(server)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] server failed to listen\nerrno: %d\nerrno reason: %d\n%s", listen_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[TEST CASE 001] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, test001_server_thread_nonblocking_main, server);

    struct netc_tcp_client* client = malloc(sizeof(struct netc_tcp_client));
    client->on_connect = tet001_client_on_connect;
    client->on_data = test001_client_on_data;
    client->on_disconnect = test001_client_on_disconnect;

    struct netc_tcp_client_config client_config = {
        .ip = IP,
        .port = PORT,
        .ipv6_connect_from = USE_IPV6,
        .ipv6_connect_to = USE_IPV6,
        .non_blocking = CLIENT_NON_BLOCKING
    };

    int client_init_result = 0;
    if ((client_init_result = tcp_client_init(client, client_config)) != 0)
    {
        printf(ANSI_RED "[TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", client_init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, test001_client_thread_nonblocking_main, client);

    int client_connect_result = 0;
    if ((client_connect_result = tcp_client_connect(client)) != 0 && client_connect_result != EINPROGRESS)
    {
        printf(ANSI_RED "[TEST CASE 001] client failed to connect\nerrno: %d\nerrno reason: %d\n%s", client_connect_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);

    if (test001_server_connect != 1) printf(ANSI_RED "\n\n\n[SERVER_CONNECT] server failed to accept incoming connection\n" ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[SERVER_CONNECT] server accepted incoming connection\n" ANSI_RESET);

    if (test001_server_data != 1) printf(ANSI_RED "[SERVER_DATA] server failed to receive data from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DATA] server received data from client\n" ANSI_RESET);

    if (test001_server_disconnect != 1) printf(ANSI_RED "[SERVER_DISCONNECT] server failed to disconnect from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DISCONNECT] server disconnected from client\n" ANSI_RESET);

    if (test001_client_connect != 1) printf(ANSI_RED "[CLIENT_CONNECT] client failed to connect to server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_CONNECT] client connected to server\n" ANSI_RESET);

    if (test001_client_data != 1) printf(ANSI_RED "[CLIENT_DATA] client failed to receive data from server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DATA] client received data from server\n" ANSI_RESET);

    if (test001_client_disconnect != 1) printf(ANSI_RED "[CLIENT_DISCONNECT] client failed to disconnect from server\n\n\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DISCONNECT] client disconnected from server\n\n\n" ANSI_RESET);

    return 
        (int)(!(test001_server_connect == 1 && test001_server_data == 1 && test001_server_disconnect == 1 && test001_client_connect == 1 && test001_client_data == 1 && test001_client_disconnect == 1));
};

#endif // TEST_001