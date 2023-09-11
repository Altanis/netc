#ifndef TCP_TEST_002
#define TCP_TEST_002

/**
 * TEST CASE 2: blocking server and client
*/

#include "tcp/server.h"
#include "tcp/client.h"
#include "utils/error.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/errno.h>
#include <sys/_endian.h>
#endif

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

/** At the end of this test, all of these values must equal 1 unless otherwise specified. */
static int tcp_test002_server_connect = 0;
static int tcp_test002_server_data = 0;
static int tcp_test002_server_disconnect = 0;
static int tcp_test002_client_connect = 0;
static int tcp_test002_client_data = 0;
static int tcp_test002_client_disconnect = 0;

static void* tcp_test002_server_thread_blocking_main(void* arg);
static void* tcp_test002_client_thread_blocking_main(void* arg);

static void* tcp_test002_server_thread_blocking_main(void* arg)
{
    struct tcp_server* server = (struct tcp_server*)arg;
    struct tcp_client* client = malloc(sizeof(struct tcp_client));

    int accept_result;
    if ((accept_result = tcp_server_accept(server, client)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to accept incoming connection\nerrno: %d\nerrno reason: %d\n%s", accept_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        tcp_test002_server_connect++;
        printf("[TCP TEST CASE 002] new socket connected. socket id: %d\n", client->sockfd);
    }

    int recv_result;
    char* buffer = calloc(18, sizeof(char));

    if ((recv_result = tcp_server_receive(client->sockfd, buffer, 17, 0)) != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to receive data from client\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        tcp_test002_server_data++;
        printf("[TCP TEST CASE 002] server received data from client! %s\n", buffer);
    }

    int send_result;
    if ((send_result = tcp_server_send(client->sockfd, "hello from server", 17, 0)) != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to send data to client\nerrno: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        printf("[TCP TEST CASE 002] server sent data to client!\n");
    }

    // check if recv == 0
    int disconnect_result;
    if ((disconnect_result = tcp_server_receive(client->sockfd, buffer, 17, 0)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to disconnect from client\nerrno: %d\nerrno reason: %d\n%s", disconnect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        tcp_test002_server_disconnect++;
        printf("[TCP TEST CASE 002] server disconnected from client!\n");
    }

    return NULL;
};

static void* tcp_test002_client_thread_blocking_main(void* arg)
{
    struct tcp_client* client = (struct tcp_client*)arg;

    int client_connect_result = 0;
    if ((client_connect_result = tcp_client_connect(client)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to connect\nerrno: %d\nerrno reason: %d\n%s", client_connect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    }
    else
    {
        tcp_test002_client_connect++;
        printf("[TCP TEST CASE 002] client connected!\n");
    }

    int send_result = 0;
    if (tcp_client_send(client, "hello from client", 17, 0) != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to send data to server\nerrno: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } 
    else 
    {
        printf("[TCP TEST CASE 002] client sent data to server!\n");
    }

    int recv_result = 0;
    char* buffer = calloc(18, sizeof(char));

    if ((recv_result = tcp_client_receive(client, buffer, 17, 0)) != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to receive data from server\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        tcp_test002_client_data++;
        printf("[TCP TEST CASE 002] client received data from server! %s\n", buffer);
    }

    int disconnect_result = 0;
    if ((disconnect_result = tcp_client_close(client, 0)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to disconnect from server\nerrno: %d\nerrno reason: %d\n%s", disconnect_result, netc_errno_reason, ANSI_RESET);
        return NULL;
    } else {
        tcp_test002_client_disconnect++;
        printf("[TCP TEST CASE 002] client disconnected from server!\n");
    }

    return NULL;
};

static int tcp_test002()
{
    struct tcp_server* server = malloc(sizeof(struct tcp_server));
    struct sockaddr_in saddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, IP, &(addr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int init_result = 0;
    if ((init_result = tcp_server_init(server, *(struct sockaddr*)&saddr, SERVER_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int optval = 1;
    if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to setsockopt\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = tcp_server_bind(server)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to bind\nerrno: %d\nerrno reason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int listen_result = 0;
    if ((listen_result = tcp_server_listen(server, BACKLOG)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to listen\nerrno: %d\nerrno reason: %d\n%s", listen_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[TCP TEST CASE 002] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_test002_server_thread_blocking_main, server);

    struct tcp_client* client = malloc(sizeof(struct tcp_client));

    int client_init_result = 0;
    if ((client_init_result = tcp_client_init(client, *(struct sockaddr*)&addr, CLIENT_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", client_init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    tcp_test002_client_thread_blocking_main(client);
    pthread_join(server_thread, NULL);

    if (tcp_test002_server_connect != 1) printf(ANSI_RED "\n\n\n[SERVER_CONNECT] server failed to accept incoming connection\n" ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[SERVER_CONNECT] server accepted incoming connection\n" ANSI_RESET);

    if (tcp_test002_server_data != 1) printf(ANSI_RED "[SERVER_DATA] server failed to receive data from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DATA] server received data from client\n" ANSI_RESET);

    if (tcp_test002_server_disconnect != 1) printf(ANSI_RED "[SERVER_DISCONNECT] server failed to disconnect from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DISCONNECT] server disconnected from client\n" ANSI_RESET);

    if (tcp_test002_client_connect != 1) printf(ANSI_RED "[CLIENT_CONNECT] client failed to connect to server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_CONNECT] client connected to server\n" ANSI_RESET);

    if (tcp_test002_client_data != 1) printf(ANSI_RED "[CLIENT_DATA] client failed to receive data from server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DATA] client received data from server\n" ANSI_RESET);

    if (tcp_test002_client_disconnect != 1) printf(ANSI_RED "[CLIENT_DISCONNECT] client failed to disconnect from server\n\n\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DISCONNECT] client disconnected from server\n\n\n" ANSI_RESET);

    return 
        (int)(!(tcp_test002_server_connect == 1 && tcp_test002_server_data == 1 && tcp_test002_server_disconnect == 1 && tcp_test002_client_connect == 1 && tcp_test002_client_data == 1 && tcp_test002_client_disconnect == 1));
};

#endif // TCP_TEST_002