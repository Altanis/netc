#ifndef WS_SERVER_H
#define WS_SERVER_H

struct web_server;
struct web_client;

#include "../http/common.h"

/** Upgrades the connection to WebSocket. Returns `-1` if the upgrade was not able to occur. */
int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request);
/** Closes a WebSocket client. */
int ws_server_close_client(struct web_server *server, struct web_client *client, uint16_t code, char *reason);

#endif // WS_SERVER_H