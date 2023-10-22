#ifndef WS_SERVER_H
#define WS_SERVER_H

struct web_server;
struct web_client;

#include "../http/common.h"

/** Upgrades the connection to WebSocket. Returns `-1` if the upgrade was not able to occur. */
int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request);

/** Parses an incoming websocket frame. */
int ws_server_parse_frame(struct web_server *server, struct web_client *client, struct ws_frame_parsing_state *current_state);

#endif // WS_SERVER_H