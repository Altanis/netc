// send/receive same data
// 1. send/recv 1 frame of data
// 2. send/recv multiple frames of data
// 3. masked data
// 4. close frames

#include "../../include/web/server.h"
#include "../../include/web/client.h"
#include "../../include/utils/error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
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
#define PORT 8081
#define BACKLOG 3
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 1
#define CLIENT_NON_BLOCKING 1

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

static struct web_server ws_server = {0};

/** At the end of this test, all of these values must equal 1 unless otherwise specified. */
static int on_connect_server = 0;
static int send_basic_server = 0;
static int send_multiple_frames_server = 0;
static int send_masked_server = 0;
static int send_close_server = 0;

static int on_open_client = 0;
static int send_basic_client = 0;
static int send_multiple_frames_client = 0;
static int send_masked_client = 0;
static int send_close_client = 0;

static void ws_server_on_connect(struct web_server *server, struct web_client *client)
{
    on_connect_server++;
};

static void ws_server_on_message(struct web_server *server, struct web_client *client, struct ws_message message)
{

};

static void ws_server_on_malformed_request(struct web_server *server, struct web_client *client, enum ws_frame_parsing_errors error)
{

};

static void ws_server_on_disconnect(struct web_server *server, socket_t sockfd, bool is_error)
{

};

static void ws_client_on_connect(struct web_client *client)
{
    on_open_client++;
};

static void ws_client_on_message(struct web_client *client, struct ws_message message)
{

};

static void ws_server_on_malformed_request(struct web_client *client, enum ws_frame_parsing_errors error)
{

};

static void ws_client_on_disconnect(struct web_client *client, uint16_t code, char *message)
{

};

static int ws_test001()
{
    ws_server.on_connect = ws_server_on_connect;
    ws_server.on_disconnect = ws_server_on_disconnect;
};