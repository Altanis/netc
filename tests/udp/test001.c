#ifndef UDP_TEST_001
#define UDP_TEST_001

/**
 * TEST CASE 1: nonblocking server and client
*/

#include "udp/server.h"
#include "udp/client.h"
#include "utils/error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/_endian.h>
#include <time.h>
#include <unistd.h>

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
static int udp_test001_server_data = 0;
static int udp_test001_client_data = 0;

static void* udp_test001_server_thread_nonblocking_main(void* arg);
static void udp_test001_server_on_data(struct netc_udp_server* server, void* data);

static void* udp_test001_client_thread_nonblocking_main(void* arg);
static void udp_test001_client_on_data(struct netc_udp_client* client, void* data);

static int udp_test001();

static void* udp_test001_server_thread_nonblocking_main(void* arg)
{
    struct netc_udp_server* server = (struct netc_udp_server*)arg;
    int r = udp_server_main_loop(server);
    if (r != 0) printf(ANSI_RED "[UDP TEST CASE 001] server main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void udp_test001_server_on_data(struct netc_udp_server* server, void* data)
{
    char* buffer = calloc(18, sizeof(char));
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    int recv_result = 0;
    if ((recv_result = udp_server_receive(server, buffer, 18, 0, &client_addr, &client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] server recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    } else printf("[UDP TEST CASE 001] server received data from client %s\n", buffer);

    udp_test001_server_data = 1;

    int send_result = 0;
    if ((send_result = udp_server_send(server, "hello from server", 18, 0, &client_addr, client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] server send failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    };

    udp_server_close(server);
};

static void* udp_test001_client_thread_nonblocking_main(void* arg)
{
    struct netc_udp_client* client = (struct netc_udp_client*)arg;
    int r = udp_client_main_loop(client);
    if (r != 0) printf(ANSI_RED "[UDP TEST CASE 001] client main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void udp_test001_client_on_data(struct netc_udp_client* client, void* data)
{
    char* buffer = calloc(18, sizeof(char));

    struct sockaddr addr = {};
    socklen_t socklen = 0;

    int recv_result = 0;
    if ((recv_result = udp_client_receive(client, buffer, 18, 0, &addr, &socklen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] client recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    } else printf("[UDP TEST CASE 001] client received data from server %s\n", buffer);

    udp_test001_client_data = 1;
    netc_udp_client_listening = 0;

    udp_client_close(client);
};

static int udp_test001()
{
    struct netc_udp_server* server = malloc(sizeof(struct netc_udp_server));
    server->on_data = udp_test001_server_on_data;

    struct sockaddr_in udp_server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    int init_result = 0;
    if ((init_result = udp_server_init(server, USE_IPV6, SERVER_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] server init failed:\nerrno: %d\nreason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = udp_server_bind(server, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr), REUSE_ADDRESS)) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] server bind failed:\nerrno: %d\nreason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[UDP TEST CASE 001] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, udp_test001_server_thread_nonblocking_main, server);

    struct netc_udp_client* client = malloc(sizeof(struct netc_udp_client));
    client->on_data = udp_test001_client_on_data;

    struct sockaddr_in udp_client_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };

    if (inet_pton(AF_INET, IP, &(udp_client_addr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int client_init_result = 0;
    if ((client_init_result = udp_client_init(client, USE_IPV6, CLIENT_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] client init failed:\nerrno: %d\nreason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int client_connect_result = 0;
    if ((client_connect_result = udp_client_connect(client, (struct sockaddr*)&udp_client_addr, sizeof(udp_client_addr))) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] client connect failed:\nerrno: %d\nreason: %d\n%s", client_connect_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[UDP TEST CASE 001] client connected to %s:%d\n", IP, PORT);

    int send_result = 0;
    // note: you can provide NULL for the server addr and socklen if you have used
    // udp_client_connect() to connect to the server beforehand.
    // you can put the server address and socklen in the udp_client_send() call
    // regardless of whether you have connected to the server or not.
    if ((send_result = udp_client_send(client, "hello from client", 18, 0, NULL, 0)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 001] client send failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, udp_test001_client_thread_nonblocking_main, client);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (udp_test001_server_data != 1) printf(ANSI_RED "\n\n\n[UDP TEST CASE 001] server data not received\n%s", ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[UDP TEST CASE 001] server data received\n%s", ANSI_RESET); 

    if (udp_test001_client_data != 1) printf(ANSI_RED "[UDP TEST CASE 001] client data not received\n\n\n%s", ANSI_RESET);
    else printf(ANSI_GREEN "[UDP TEST CASE 001] client data received\n\n\n%s", ANSI_RESET);

    return (int)(!(udp_test001_server_data == 1 && udp_test001_client_data == 1));
};

#endif // UDP_TEST_001