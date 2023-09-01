#ifndef UDP_TEST_002
#define UDP_TEST_002

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

#undef IP
#undef PORT
#undef BACKLOG
#undef USE_IPV6
#undef SERVER_NON_BLOCKING
#undef CLIENT_NON_BLOCKING
#undef ANSI_RED
#undef ANSI_GREEN
#undef ANSI_RESET

#define IP "127.0.0.1"
#define PORT 8080
#define BACKLOG 3
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 0
#define CLIENT_NON_BLOCKING 0

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

/** At the end of this test, all of these values must equal 1. */
static int udp_test002_server_data = 0;
static int udp_test002_client_data = 0;

static void* udp_test002_server_thread_blocking_main(void* arg);
static void* udp_test002_client_thread_blocking_main(void* arg);

static int udp_test002();

static void* udp_test002_server_thread_blocking_main(void* arg)
{
    struct udp_server* server = (struct udp_server*)arg;
    char* buffer = calloc(18, sizeof(char));
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    int recv_result = 0;
    if ((recv_result = udp_server_receive(server, buffer, 18, 0, &client_addr, &client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else printf("[UDP TEST CASE 002] server received data from client %s\n", buffer);

    udp_test002_server_data = 1;

    int send_result = 0;
    if ((send_result = udp_server_send(server, "hello from server", 18, 0, &client_addr, client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server send failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return NULL;
    };

    udp_server_close(server);

    return NULL;
};

static void udp_test002_server_on_data(struct udp_server* server, void* data)
{
    char* buffer = calloc(18, sizeof(char));
    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    int recv_result = 0;
    if ((recv_result = udp_server_receive(server, buffer, 18, 0, &client_addr, &client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    } else printf("[UDP TEST CASE 002] server received data from client %s\n", buffer);

    udp_test002_server_data = 1;

    int send_result = 0;
    if ((send_result = udp_server_send(server, "hello from server", 18, 0, &client_addr, client_addrlen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server send failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    };

    udp_server_close(server);
};

static void* udp_test002_client_thread_blocking_main(void* arg)
{
    struct udp_client* client = (struct udp_client*)arg;

    int send_result = 0;
    if ((send_result = udp_client_send(client, "hello from client", 18, 0, NULL, 0)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] client send failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return NULL;
    };

    char* buffer = calloc(18, sizeof(char));
    int recv_result = 0;
    // note: you can provide NULL for the server addr and socklen if you have used
    // udp_client_connect() to connect to the server beforehand.
    // you can put the server address and socklen in the udp_client_send() call
    // regardless of whether you have connected to the server or not.
    if ((recv_result = udp_client_receive(client, buffer, 18, 0, NULL, NULL)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] client recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else printf("[UDP TEST CASE 002] client received data from server %s\n", buffer);

    udp_test002_client_data = 1;

    return NULL;
};

static void udp_test002_client_on_data(struct udp_client* client, void* data)
{
    char* buffer = calloc(18, sizeof(char));

    struct sockaddr addr = {};
    socklen_t socklen = 0;

    int recv_result = 0;
    if ((recv_result = udp_client_receive(client, buffer, 18, 0, &addr, &socklen)) != 18)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] client recv failed:\nerrno: %d\nreason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return;
    } else printf("[UDP TEST CASE 002] client received data from server %s\n", buffer);

    udp_test002_client_data = 1;
    netc_udp_client_listening = 0;

    udp_client_close(client);
};

static int udp_test002()
{
    struct udp_server* server = malloc(sizeof(struct udp_server));
    server->on_data = udp_test002_server_on_data;

    struct sockaddr_in udp_server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    int init_result = 0;
    if ((init_result = udp_server_init(server, USE_IPV6, SERVER_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server init failed:\nerrno: %d\nreason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int optval = 1;
    if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to setsockopt\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = udp_server_bind(server, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr))) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] server bind failed:\nerrno: %d\nreason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[UDP TEST CASE 002] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, udp_test002_server_thread_blocking_main, server);

    struct udp_client* client = malloc(sizeof(struct udp_client));
    client->on_data = udp_test002_client_on_data;

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
        printf(ANSI_RED "[UDP TEST CASE 002] client init failed:\nerrno: %d\nreason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int client_connect_result = 0;
    if ((client_connect_result = udp_client_connect(client, (struct sockaddr*)&udp_client_addr, sizeof(udp_client_addr))) != 0)
    {
        printf(ANSI_RED "[UDP TEST CASE 002] client connect failed:\nerrno: %d\nreason: %d\n%s", client_connect_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[UDP TEST CASE 002] client connected to %s:%d\n", IP, PORT);

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, udp_test002_client_thread_blocking_main, client);

    pthread_join(server_thread, NULL);
    pthread_join(client_thread, NULL);

    if (udp_test002_server_data != 1) printf(ANSI_RED "\n\n\n[UDP TEST CASE 002] server data not received\n%s", ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[UDP TEST CASE 002] server data received\n%s", ANSI_RESET);

    if (udp_test002_client_data != 1) printf(ANSI_RED "[UDP TEST CASE 002] client data not received\n\n\n%s", ANSI_RESET);
    else printf(ANSI_GREEN "[UDP TEST CASE 002] client data received\n\n\n%s", ANSI_RESET);

    return (int)(!(udp_test002_server_data == 1 && udp_test002_client_data == 1));
};

#endif // UDP_TEST_002