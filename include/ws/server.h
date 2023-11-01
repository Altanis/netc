#ifndef WS_SERVER_H
#define WS_SERVER_H

struct web_server;
struct web_client;

#include "../http/common.h"

/** Upgrades the connection to WebSocket. Returns `-1` if the upgrade was not able to occur. */
int ws_server_upgrade_connection(struct web_server *server, struct web_client *client, struct http_request *request);

/** Builds a WebSocket masking key. */
void ws_server_build_masking_key(uint8_t masking_key[4]);
/** Builds a WebSocket frame. */
void ws_server_build_frame(struct ws_frame *frame, uint8_t fin, uint8_t rsv1, uint8_t rsv2, uint8_t rsv3, uint8_t opcode, uint8_t mask, uint8_t masking_key[4], uint64_t payload_length);
/** Sends a WebSocket message. Returns 1, otherwise a failure. */
int ws_server_send_frame(struct web_server *server, struct web_client *client, struct ws_frame *frame, const char *payload_data, size_t num_frames);
/** Parses an incoming websocket frame. */
int ws_server_parse_frame(struct web_server *server, struct web_client *client, struct ws_frame_parsing_state *current_state);
/** Closes a WebSocket client. */
int ws_server_close_client(struct web_server *server, struct web_client *client, uint16_t code, char *reason);

#endif // WS_SERVER_H