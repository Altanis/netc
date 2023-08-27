#ifndef HTTP_TEST_001
#define HTTP_TEST_001

#include "http/server.h"
#include "http/client.h"
#include "utils/error.h"
#include "utils/map.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/_endian.h>

#undef IP
#undef PORT
#undef BACKLOG
#undef REUSE_ADDRESS
#undef USE_IPV6
#undef SERVER_NON_BLOCKING
#undef CLIENT_NON_BLOCKING
#undef ANSI_RED
#undef ANSI_GREEN
#undef ANSI_RESET

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
static int http_test001_server_connect = 0;
static int http_test001_server_data = 0;
static int http_test001_data_chunked = 0;
static int http_test001_server_disconnect = 0;
static int http_test001_client_connect = 0;
static int http_test001_client_data = 0;
static int http_test001_client_data_chunked = 0;
static int http_test001_client_disconnect = 0;

static void http_test001_server_on_connect(struct http_server* server, struct tcp_client* client, void* data);
static void http_test001_server_on_data(struct http_server* server, socket_t sockfd, struct http_request request);
static void http_test001_server_on_malformed_request(struct http_server* server, socket_t sockfd, enum parse_request_error_types error, void* data);
static void http_test001_server_on_disconnect(struct http_server* server, socket_t sockfd, int is_error, void* data);

static void http_test001_client_on_connect(struct http_client* client, void* data);
static void http_test001_client_on_data(struct http_client* client, struct http_response response, void* data);
static void http_test001_client_on_malformed_response(struct http_client* client, enum parse_response_error_types error, void* data);
static void http_test001_client_on_disconnect(struct http_client* client, int is_error, void* data);

/** This maps sockfds to IPs. */
static struct map connections = {0};

static void http_test001_server_on_connect(struct http_server* server, struct tcp_client* client, void* data)
{
    http_test001_server_connect = 1;

    char ip[INET6_ADDRSTRLEN] = {0};
    if (client->sockaddr->sa_family == AF_INET)
    {
        struct sockaddr_in* addr = (struct sockaddr_in*)client->sockaddr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    }
    else
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)client->sockaddr;
        inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));
    };

    map_set(&connections, &client->sockfd, ip, strlen(ip) + 1);
};

static void http_test001_server_on_data(struct http_server* server, socket_t sockfd, struct http_request request)
{
    http_test001_server_data = 1;

    char* ip = map_get(&connections, &sockfd, sizeof(sockfd));
    printf(ANSI_GREEN "[HTTP TEST CASE 001] server received data from %s\n%s", ip, ANSI_RESET);
};

static void http_test001_server_on_malformed_request(struct http_server* server, socket_t sockfd, enum parse_request_error_types error, void* data)
{
    char* ip = map_get(&connections, &sockfd, sizeof(sockfd));
    printf(ANSI_RED "[HTTP TEST CASE 001] server received malformed request from %s\n%s", ip, ANSI_RESET);
};

static void http_test001_server_on_disconnect(struct http_server* server, socket_t sockfd, int is_error, void* data)
{
    http_test001_server_disconnect = 1;

    char* ip = map_get(&connections, &sockfd, sizeof(sockfd));
    printf(ANSI_GREEN "[HTTP TEST CASE 001] server disconnected from %s\n%s", ip, ANSI_RESET);
};

static void http_test001_client_on_connect(struct http_client* client, void* data)
{
    http_test001_client_connect = 1;

    printf(ANSI_GREEN "[HTTP TEST CASE 001] client connected\n%s", ANSI_RESET);
};

static void http_test001_client_on_data(struct http_client* client, struct http_response response, void* data)
{
    http_test001_client_data = 1;

    printf(ANSI_GREEN "[HTTP TEST CASE 001] client received data\n%s", ANSI_RESET);
};

static void http_test001_client_on_malformed_response(struct http_client* client, enum parse_response_error_types error, void* data)
{
    printf(ANSI_RED "[HTTP TEST CASE 001] client received malformed response\n%s", ANSI_RESET);
};

static void http_test001_client_on_disconnect(struct http_client* client, int is_error, void* data)
{
    http_test001_client_disconnect = 1;

    printf(ANSI_GREEN "[HTTP TEST CASE 001] client disconnected\n%s", ANSI_RESET);
};

static int http_test001()
{
    map_init(&connections, 2);

    struct http_server server = {0};
    server.on_connect = http_test001_server_on_connect;
    server.on_malformed_request = http_test001_server_on_malformed_request;
    server.on_disconnect = http_test001_server_on_disconnect;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (http_server_init(&server, USE_IPV6, REUSE_ADDRESS, (struct sockaddr*)&addr, sizeof(addr), BACKLOG) != 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    struct http_route route = {0};
    route.path = "/*";
    route.callback = http_test001_server_on_data;

    http_server_create_route(&server, &route);

    pthread_t servt;
    pthread_create(&servt, NULL, (void*)http_server_start, &server);

    struct http_client client = {0};
    client.on_connect = http_test001_client_on_connect;
    client.on_malformed_response = http_test001_client_on_malformed_response;
    client.on_data = http_test001_client_on_data;
    client.on_disconnect = http_test001_client_on_disconnect;

    struct sockaddr_in cliaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, IP, &(cliaddr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    if (http_client_init(&client, USE_IPV6, (struct sockaddr*)&cliaddr, sizeof(cliaddr)) != 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_t clit;
    pthread_create(&clit, NULL, (void*)http_client_start, &client);

    pthread_join(clit, NULL);
    pthread_join(servt, NULL);
};

#endif // HTTP_TEST_001