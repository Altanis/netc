#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <openssl/sha.h>

#include "../../include/web/server.h"
#include "../../include/ws/server.h"
#include "../../include/tcp/server.h"

int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request)
{
    socket_t sockfd = client->tcp_client->sockfd;

    struct http_header *sec_websocket_key = http_request_get_header(request, "Sec-WebSocket-Key");
    struct http_header *sec_websocket_version = http_request_get_header(request, "Sec-WebSocket-Version");
    struct http_header *sec_websocket_protocol = http_request_get_header(request, "Sec-WebSocket-Protocol");

    if (sec_websocket_key == NULL)
    {
        // That comedian...
        const char *badrequest_message = "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 33\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Invalid Sec-WebSocket-Key header.";

        tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
        tcp_server_close_client(server->tcp_server, sockfd, 0);

        return -1;
    };

    if (sec_websocket_version == NULL || strcmp(sso_string_get(&sec_websocket_version->value), WEBSOCKET_VERSION) != 0)
    {
        // That comedian...
        const char *badrequest_message = "HTTP/1.1 426 Upgrade Required\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 41\r\n"
            "Sec-WebSocket-Version: 13\r\n" // reminder to change this
            "Connection: close\r\n"
            "\r\n"
            "Unsupported Sec-WebSocket-Version header.";

        tcp_server_send(sockfd, (char *)badrequest_message, strlen(badrequest_message), 0);
        tcp_server_close_client(server->tcp_server, sockfd, 0);

        return -1;
    };

    size_t guid_len = strlen(WEBSOCKET_HANDSHAKE_GUID);
    char websocket_key_id[sec_websocket_key->value.length + guid_len];
    sso_string_copy_buffer(websocket_key_id, &sec_websocket_key->value);
    for (size_t i = 0; i < guid_len; ++i)
    {
        websocket_key_id[sec_websocket_key->value.length + i] = WEBSOCKET_HANDSHAKE_GUID[i];
    };

    char websocket_accept_id[SHA_DIGEST_LENGTH];
    SHA1(websocket_key_id, sizeof(websocket_key_id), websocket_accept_id);


    char websocket_accept_id_base64[((4 * sizeof(websocket_accept_id) / 3) + 3) & ~3];
    http_base64_encode(websocket_accept_id, sizeof(websocket_accept_id), websocket_accept_id_base64);

    char* headers[][2] =
    {
        { "Connection", "upgrade" },
        { "Upgrade", "websocket" },
        { "Sec-WebSocket-Accept", websocket_accept_id_base64 }
    };

    struct http_response response;
    http_response_build(
        &response,
        "HTTP/1.1", 
        101, 
        headers,
        3
    );

    if (sec_websocket_protocol != NULL)
    {
        struct http_header protocol;
        http_header_init(&protocol, "Sec-WebSocket-Protocol", sso_string_get(&sec_websocket_protocol->value));
        vector_push(&response.headers, &protocol);
    };

    int result = http_server_send_response(server, client, &response, NULL, 0);
    if (result > 0)
    {
        client->path = strdup(sso_string_get(&request->path));
        client->connection_type = CONNECTION_WS;
        return 0;
    } else return -1;
};

int ws_server_close_client(struct web_server *server, struct web_client *client, uint16_t code, char *reason)
{
    size_t reason_len = reason != NULL ? strlen(reason) : 0;

    struct ws_frame frame;
    ws_build_frame(&frame, 0, 0, 0, WS_OPCODE_CLOSE, 0, NULL, 2 + reason_len);

    char payload_data[2 + reason_len];
    payload_data[0] = (code >> 8) & 0xFF;
    payload_data[1] = code & 0xFF;

    if (reason != NULL)
    {
        memcpy(payload_data + 2, reason, reason_len);
    };

    ws_send_frame(client, &frame, payload_data, 1);
    return tcp_server_close_client(server->tcp_server, client->tcp_client->sockfd, false);
};