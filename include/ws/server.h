#ifndef WS_SERVER_H
#define WS_SERVER_H

#include "../web/server.h"

/** The WebSocket GUID. */
const char* WEBSOCKET_HANDSHAKE_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
/** The version the server supports. */
const char* WEBSOCKET_VERSION = "13";

/** Upgrades the connection to WebSocket. Returns `-1` if the upgrade was not able to occur. */
int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request);

#endif // WS_SERVER_H