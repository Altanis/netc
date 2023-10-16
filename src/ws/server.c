#include "../../include/ws/server.h"
#include "../../include/tcp/server.h"

int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request)
{
    socket_t sockfd = client->client->sockfd;

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
        tcp_server_close_client(server->server, sockfd, 0);

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
        tcp_server_close_client(server->server, sockfd, 0);

        return -1;
    };

    struct http_response response = {0};
    sso_string_set(&response.version, "HTTP/1.1");
    response.status_code = 101;
    sso_string_set(&response.status_message, http_status_messages[HTTP_STATUS_CODE_101]);

    vector_init(&response.headers, 4, sizeof(struct http_header));

    struct http_header connection = {0};
    sso_string_set(&connection.name, "Connection");
    sso_string_set(&connection.value, "upgrade");
    vector_push(&response.headers, &connection);

    struct http_header upgrade = {0};
    sso_string_set(&upgrade.name, "Upgrade");
    sso_string_set(&upgrade.value, "websocket");
    vector_push(&response.headers, &upgrade);

    if (sec_websocket_protocol != NULL)
    {
        struct http_header protocol = {0};
        sso_string_set(&protocol.name, "Sec-WebSocket-Protocol");
        sso_string_set(&protocol.value, sso_string_get(&sec_websocket_protocol->value));
        vector_push(&response.headers, &protocol);
    };

    struct http_header accept = {0};
    sso_string_set(&accept.name, "Sec-WebSocket-Accept");
};