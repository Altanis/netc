#ifndef WS_CLIENT_H
#define WS_CLIENT_H

struct web_server;
struct web_client;

#include "../http/common.h"

/** 
 * Upgrades an existing HTTP connection. Ensure `url` includes the host. Returns `1` if the request was successful.
 * NOTE: This does not guarantee that the server will accept the upgrade request. The callback will be called with the response.
 */
int ws_client_connect(struct web_client *client, const char *hostname, const char *path, const char *protocols[]);

/** Closes a WebSocket client connection. Returns `1` if the connection closed */
int ws_client_close(struct web_client *client, uint16_t code, const char *reason);

#endif // WS_CLIENT_H