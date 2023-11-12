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
#define PORT 8923
#define BACKLOG 3
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 1
#define CLIENT_NON_BLOCKING 1

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

static struct web_server ws_server = {0};
static int ws_test001();

static const int FRAME_SPLIT = 3;

/** At the end of this test, all of these values must equal 1 unless otherwise specified. */
static int on_connect_server = 0;
static int send_basic_server = 0;
static int send_multiple_frames_server = 0;
static int send_masked_server = 0;
static int send_multiple_frames_masked_server = 0;
static int send_close_server = 0;

static int on_open_client = 0;
static int send_basic_client = 0;
static int send_multiple_frames_client = 0;
static int send_masked_client = 0;
static int send_multiple_frames_masked_client = 0;
static int send_close_client = 0;

static void ws_server_on_connect(struct web_server *server, struct web_client *client, struct http_request *request)
{
    on_connect_server++;

    int r = 0;
    if ((r = ws_server_upgrade_connection(server, client, request)) < 0) netc_perror("Error when upgrading connection");
    else printf("[WS TEST CASE 001] incoming client\n");
};

static void ws_server_on_message(struct web_server *server, struct web_client *client, struct ws_message *h_message)
{
    printf("[WS TEST CASE 001] server received unspecified message %s\n", h_message->buffer);
    struct ws_message message = *h_message;

    if (strcmp(message.buffer, "hello server basic") == 0)
    {
        send_basic_client++;
        printf("[WS TEST CASE 001] server received basic message\n");

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, false, NULL, message.payload_length);
        
        int r = 0;
        if ((r = ws_send_frame(client, &frame, "hello client basic", 1)) < 1)
        {
            netc_perror("Error occured when sending basic frame server");
        };
    }
    else if (strcmp(message.buffer, "hello server multiple frames") == 0)
    {
        send_multiple_frames_client++;
        printf("[WS TEST CASE 001] server received multiple frames message\n");

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, false, NULL, message.payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, "hello client multiple frames", FRAME_SPLIT)) < 1)
        {
            netc_perror("Error occured when sending multiple frames frame server");
        };
    }
    else if (strcmp(message.buffer, "hello server masked") == 0)
    {
        send_masked_client++;
        printf("[WS TEST CASE 001] server received masked message\n");

        uint8_t masking_key[4];
        ws_build_masking_key(masking_key);

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, true, masking_key, message.payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, "hello client masked", 1)) < 1)
        {
            netc_perror("Error occured when sending masked frame server");
        };
    }
    else if (strcmp(message.buffer, "hello server multiple frames masked") == 0)
    {
        send_multiple_frames_masked_client++;
        printf("[WS TEST CASE 001] server received multiple frames masked message\n");

        uint8_t masking_key[4];
        ws_build_masking_key(masking_key);

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, true, masking_key, message.payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, "hello client multiple frames masked", FRAME_SPLIT)) < 1)
        {
            netc_perror("Error occured when sending multiple frames masked frame server");
        };
    };
};

static void ws_server_on_malformed_request(struct web_server *server, struct web_client *client, enum ws_frame_parsing_errors error)
{
    printf(ANSI_RED "[WS TEST CASE 001] server received malformed request\n" ANSI_RESET);
};

static void ws_server_on_disconnect(struct web_server *server, socket_t sockfd, bool is_error)
{
    send_close_client++;
    printf("[WS TEST CASE 001] server noticed client disconnected\n");
};

static void ws_client_on_http_connect(struct web_client *client)
{
    printf("[WS TEST CASE 001] client connected\n");

    int r = 0;
    if ((r = ws_client_connect(client, "localhost:8923", "/", NULL) != 1))
        printf("Failed to connect %d\n", r);
};

static void ws_client_on_connect(struct web_client *client)
{
    on_open_client++;
    printf("[WS TEST CASE 001] client connected\n");

    char *message = "hello server basic";
    size_t payload_length = strlen(message);

    struct ws_frame frame;
    ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, false, NULL, payload_length);

    int r = 0;
    if ((r = ws_send_frame(client, &frame, message, 1)) < 1)
    {
        netc_perror("Error occured when sending basic frame client");
    };
};

static void ws_client_on_message(struct web_client *client, struct ws_message *h_message)
{
    printf("[WS TEST CASE 001] client received unspecified message\n");

    struct ws_message message = *h_message;
    
    if (strcmp(message.buffer, "hello client basic") == 0)
    {
        send_basic_server++;
        printf("[WS TEST CASE 001] client received basic message\n");

        const char *message = "hello server multiple frames";
        size_t payload_length = strlen(message);

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, false, NULL, payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, message, FRAME_SPLIT)) < 1)
        {
            netc_perror("Error occured when sending multiple frames frame client");
        };
    }
    else if (strcmp(message.buffer, "hello client multiple frames") == 0)
    {
        send_multiple_frames_server++;
        printf("[WS TEST CASE 001] client received multiple frames message\n");

        const char *message = "hello server masked";
        size_t payload_length = strlen(message);

        uint8_t masking_key[4];
        ws_build_masking_key(masking_key);

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, true, masking_key, payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, message, 1)) < 1)
        {
            netc_perror("Error occured when sending masked frame client");
        };
    }
    else if (strcmp(message.buffer, "hello client masked") == 0)
    {
        send_masked_server++;
        printf("[WS TEST CASE 001] client received masked message\n");

        const char *message = "hello server multiple frames masked";
        size_t payload_length = strlen(message);

        uint8_t masking_key[4];
        ws_build_masking_key(masking_key);

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_TEXT, true, masking_key, payload_length);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, message, FRAME_SPLIT)) < 1)
        {
            netc_perror("Error occured when sending multiple frames masked frame client");
        };
    }
    else if (strcmp(message.buffer, "hello client multiple frames masked") == 0)
    {
        send_multiple_frames_masked_server++;
        printf("[WS TEST CASE 001] client received multiple frames masked message\n");

        struct ws_frame frame;
        ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_CLOSE, false, NULL, 0);

        int r = 0;
        if ((r = ws_send_frame(client, &frame, NULL, 1)) < 1)
        {
            netc_perror("Error occured when sending close frame client");
        };
    };
};

static void ws_client_on_malformed_request(struct web_client *client, enum ws_frame_parsing_errors error)
{
    printf(ANSI_RED "[WS TEST CASE 001] client received malformed request\n" ANSI_RESET);
};

static void ws_client_on_disconnect(struct web_client *client, uint16_t code, char *message)
{
    send_close_server++;
    printf("[WS TEST CASE 001] client disconnected\n");
};

static int ws_test001()
{
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = inet_addr(IP)
    };

    if (web_server_init(&ws_server, *(struct sockaddr *)&addr, BACKLOG) != 0)
    {
        netc_perror("web_server_init");
        return 1;
    };

    struct web_server_route main = {
        .path = "/",
        .on_ws_handshake_request = ws_server_on_connect,
        .on_ws_message = ws_server_on_message,
        .on_ws_malformed_frame = ws_server_on_malformed_request,
        .on_ws_close = ws_server_on_disconnect,
    };

    web_server_create_route(&ws_server, &main);

    pthread_t thread;
    pthread_create(&thread, NULL, (void *)web_server_start, &ws_server);

    struct web_client client = {0};
    client.on_http_connect = ws_client_on_http_connect;
    client.on_ws_connect = ws_client_on_connect;
    client.on_ws_message = ws_client_on_message;
    client.on_ws_malformed_frame = ws_client_on_malformed_request;
    client.on_ws_disconnect = ws_client_on_disconnect;

    struct sockaddr_in cliaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, IP, &cliaddr.sin_addr) < 0) perror("inet_pton");
    if (web_client_init(&client, *(struct sockaddr *)&cliaddr) < 0) netc_perror("web_client_init");

    web_client_start(&client);

    if (on_connect_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] on_connect_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] on_connect_server failed\n" ANSI_RESET);
    };

    if (send_basic_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_basic_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_basic_server failed\n" ANSI_RESET);
    };

    if (send_multiple_frames_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_multiple_frames_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_multiple_frames_server failed\n" ANSI_RESET);
    };

    if (send_masked_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_masked_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_masked_server failed\n" ANSI_RESET);
    };

    if (send_multiple_frames_masked_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_multiple_frames_masked_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_multiple_frames_masked_server failed\n" ANSI_RESET);
    };

    if (send_close_server == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_close_server passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_close_server failed\n" ANSI_RESET);
    };

    if (on_open_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] on_open_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] on_open_client failed\n" ANSI_RESET);
    };

    if (send_basic_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_basic_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_basic_client failed\n" ANSI_RESET);
    };

    if (send_multiple_frames_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_multiple_frames_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_multiple_frames_client failed\n" ANSI_RESET);
    };

    if (send_masked_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_masked_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_masked_client failed\n" ANSI_RESET);
    };

    if (send_multiple_frames_masked_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_multiple_frames_masked_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_multiple_frames_masked_client failed\n" ANSI_RESET);
    };

    if (send_close_client == 1)
    {
        printf(ANSI_GREEN "[WS TEST CASE 001] send_close_client passed\n" ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "[WS TEST CASE 001] send_close_client failed\n" ANSI_RESET);
    };

    return (int)!(on_connect_server == 1 && send_basic_server == 1 && send_multiple_frames_server == 1 && send_masked_server == 1 && send_multiple_frames_masked_server == 1 && send_close_server == 1 && on_open_client == 1 && send_basic_client == 1 && send_multiple_frames_client == 1 && send_masked_client == 1 && send_multiple_frames_masked_client == 1 && send_close_client == 1);
};