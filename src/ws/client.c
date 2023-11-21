#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>

#include "../../include/web/client.h"
#include "../../include/ws/client.h"
#include "../../include/tcp/client.h"

int ws_client_connect(struct web_client *client, const char *hostname, const char *path, const char *protocols[])
{
    srand(time(NULL));
    
    char rand_bytes[16];
    for (size_t i = 0; i < sizeof(rand_bytes); ++i)
    {
        rand_bytes[i] = rand();
    };

    char websocket_key[((4 * sizeof(rand_bytes) / 3) + 3) & ~3];
    http_base64_encode(rand_bytes, sizeof(rand_bytes), websocket_key);

    const char *headers[5][2] =
    {
        {"Host", hostname},
        {"Connection", "Upgrade"},
        {"Upgrade", "websocket"},
        {"Sec-WebSocket-Version", "13"},
        {"Sec-WebSocket-Key", websocket_key}
    };

    struct http_request request;
    http_request_build(&request, "GET", path, "HTTP/1.1", headers, 5);

    int result = 0;
    if ((result = http_client_send_request(client, &request, NULL, 0)) < 1) return result;

    return 1;
};

int ws_client_close(struct web_client *client, uint16_t code, const char *reason)
{
    struct ws_message message;
    ws_build_message(&message, WS_OPCODE_CLOSE, strlen(reason), (uint8_t *)reason);

    (void) ws_send_message(client, &message, NULL, 1); // Doesn't matter too much if this fails.

    int result = tcp_client_close(client->tcp_client, false);
    if (result < 1) return result;

    return 1;
};