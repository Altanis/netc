#ifndef TCP_TEST_001
#define TCP_TEST_001

/**
 * TEST CASE 1: non-blocking server and client
*/

#include "tcp/server.h"
#include "tcp/client.h"
#include "utils/error.h"

#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/_endian.h>

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
#define SERVER_NON_BLOCKING 1
#define CLIENT_NON_BLOCKING 1

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

/** At the end of this test, all of these values must equal 1. */
static int tcp_test001_server_connect = 0;
static int tcp_test001_server_data = 0;
static int tcp_test001_server_disconnect = 0;
static int tcp_test001_client_connect = 0;
static int tcp_test001_client_data = 0;
static int tcp_test001_client_disconnect = 0;

static void* tcp_test001_server_thread_nonblocking_main(void* arg);
static void tcp_test001_server_on_connect(struct tcp_server* server, void* data);
static void tcp_test001_server_on_data(struct tcp_server* server, socket_t sockfd, void* data);
static void tcp_test001_on_disconnect(struct tcp_server* server, socket_t sockfd, int is_error, void* data);

static void* tcp_test001_client_thread_nonblocking_main(void* arg);
static void tcp_tet001_client_on_connect(struct tcp_client* client, void* data);
static void tcp_test001_client_on_data(struct tcp_client* client, void* data);
static void tcp_test001_client_on_disconnect(struct tcp_client* client, int is_error, void* data);

static void* tcp_test001_server_thread_nonblocking_main(void* arg)
{
    struct tcp_server* server = (struct tcp_server*)arg;
    int r = tcp_server_main_loop(server);
    if (r != 0) printf(ANSI_RED "[TCP TEST CASE 001] client main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void tcp_test001_server_on_connect(struct tcp_server* server, void* data)
{
    struct tcp_client client = {};
    tcp_server_accept(server, &client);

    printf("[TCP TEST CASE 001] new socket connected. socket id: %d\n", client.sockfd);
    tcp_test001_server_connect++;
};

static void tcp_test001_server_on_data(struct tcp_server* server, socket_t sockfd, void* data)
{    
    char* buffer = calloc(18, sizeof(char));
    
    int recv_result = tcp_server_receive(sockfd, buffer, 17, 0);
    if (recv_result != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] server failed to receive\nerror: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return;
    };
    printf("[TCP TEST CASE 001] message received. socket id: %d, message: %s\n", sockfd, buffer);

    int send_result = tcp_server_send(sockfd, "hello from server", 17, 0);
    if (send_result != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] server failed to send\nerror: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return;
    };

    tcp_test001_server_data++;
};

static void tcp_test001_on_disconnect(struct tcp_server* server, socket_t sockfd, int is_error, void* data)
{
    printf("[TCP TEST CASE 001] socket disconnected. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
    tcp_test001_server_disconnect++;
    tcp_server_close_self(server);
};

static void* tcp_test001_client_thread_nonblocking_main(void* arg)
{
    struct tcp_client* client = (struct tcp_client*)arg;
    int r = tcp_client_main_loop(client);
    if (r != 0) printf(ANSI_RED "[TCP TEST CASE 001] client main loop aborted:\nerrno: %d\nreason: %d\n%s", r, netc_errno_reason, ANSI_RESET);

    return NULL;
};

static void tcp_tet001_client_on_connect(struct tcp_client* client, void* data)
{
    printf("[TCP TEST CASE 001] client connected to server!\n");
    tcp_test001_client_connect++;

    int send_result = tcp_client_send(client, "hello from client", 17, 0);
    if (send_result != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to send\nerrno: %d\nerrno reason: %d\n%s", send_result, netc_errno_reason, ANSI_RESET);
        return;
    }
};

static void tcp_test001_client_on_data(struct tcp_client* client, void* data)
{
    char* buffer = calloc(18, sizeof(char));

    int recv_result = tcp_client_receive(client, buffer, 17, 0);
    if (recv_result != 17)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to receive\nerrno: %d\nerrno reason: %d\n%s", recv_result, netc_errno_reason, ANSI_RESET);
        return;
    };

    printf("[TCP TEST CASE 001] message received from server! message: %s\n", buffer);
    tcp_test001_client_data++;

    int close_result = 0;
    if ((close_result = tcp_client_close(client, 0)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to close\nerrno: %d\nerrno reason: %d\n%s", close_result, netc_errno_reason, ANSI_RESET);
        return;
    };
};

static void tcp_test001_client_on_disconnect(struct tcp_client* client, int is_error, void* data)
{
    printf("[TCP TEST CASE 001] client disconnected from server. this was %s\n", is_error ? "closed disgracefully" : "closed gracefully");
    tcp_test001_client_disconnect++;
};

static int tcp_test001()
{
    struct tcp_server* server = malloc(sizeof(struct tcp_server));
    server->on_connect = tcp_test001_server_on_connect;
    server->on_data = tcp_test001_server_on_data;
    server->on_disconnect = tcp_test001_on_disconnect;

    struct sockaddr_in saddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    int init_result = 0;
    if ((init_result = tcp_server_init(server, USE_IPV6, SERVER_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int optval = 1;
    if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 002] server failed to setsockopt\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int bind_result = 0;
    if ((bind_result = tcp_server_bind(server, (struct sockaddr*)&saddr, sizeof(saddr))) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] server failed to bind\nerrno: %d\nerrno reason: %d\n%s", bind_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int listen_result = 0;
    if ((listen_result = tcp_server_listen(server, BACKLOG)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] server failed to listen\nerrno: %d\nerrno reason: %d\n%s", listen_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("[TCP TEST CASE 001] server listening on port %d\n", PORT);
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_test001_server_thread_nonblocking_main, server);

    struct tcp_client* client = malloc(sizeof(struct tcp_client));
    client->on_connect = tcp_tet001_client_on_connect;
    client->on_data = tcp_test001_client_on_data;
    client->on_disconnect = tcp_test001_client_on_disconnect;

    int client_init_result = 0;
    if ((client_init_result = tcp_client_init(client, USE_IPV6, CLIENT_NON_BLOCKING)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", client_init_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, tcp_test001_client_thread_nonblocking_main, client);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, IP, &(addr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    int client_connect_result = 0;
    if ((client_connect_result = tcp_client_connect(client, (struct sockaddr*)&addr, (socklen_t)sizeof(addr))) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to connect\nerrno: %d\nerrno reason: %d\n%s", client_connect_result, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);

    if (tcp_test001_server_connect != 1) printf(ANSI_RED "\n\n\n[SERVER_CONNECT] server failed to accept incoming connection\n" ANSI_RESET);
    else printf(ANSI_GREEN "\n\n\n[SERVER_CONNECT] server accepted incoming connection\n" ANSI_RESET);

    if (tcp_test001_server_data != 1) printf(ANSI_RED "[SERVER_DATA] server failed to receive data from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DATA] server received data from client\n" ANSI_RESET);

    if (tcp_test001_server_disconnect != 1) printf(ANSI_RED "[SERVER_DISCONNECT] server failed to disconnect from client\n" ANSI_RESET);
    else printf(ANSI_GREEN "[SERVER_DISCONNECT] server disconnected from client\n" ANSI_RESET);

    if (tcp_test001_client_connect != 1) printf(ANSI_RED "[CLIENT_CONNECT] client failed to connect to server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_CONNECT] client connected to server\n" ANSI_RESET);

    if (tcp_test001_client_data != 1) printf(ANSI_RED "[CLIENT_DATA] client failed to receive data from server\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DATA] client received data from server\n" ANSI_RESET);

    if (tcp_test001_client_disconnect != 1) printf(ANSI_RED "[CLIENT_DISCONNECT] client failed to disconnect from server\n\n\n" ANSI_RESET);
    else printf(ANSI_GREEN "[CLIENT_DISCONNECT] client disconnected from server\n\n\n" ANSI_RESET);

    return 
        (int)(!(tcp_test001_server_connect == 1 && tcp_test001_server_data == 1 && tcp_test001_server_disconnect == 1 && tcp_test001_client_connect == 1 && tcp_test001_client_data == 1 && tcp_test001_client_disconnect == 1));
};

#endif // TCP_TEST_001