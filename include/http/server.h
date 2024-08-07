#ifndef HTTP_HELPER_SERVER_H
#define HTTP_HELPER_SERVER_H

struct web_server;
struct web_client;

#include "./common.h"

/** Sends chunked data to the client. Returns 1, otherwise a failure. */
int http_server_send_chunked_data(struct web_server *server, struct web_client *client, const char *data, size_t data_length);
/** Sends the HTTP response. Returns 1, otherwise a failure. */
int http_server_send_response(struct web_server *server, struct web_client *client, struct http_response *response, const char *data, size_t length);
/** Parses the HTTP request. */
int http_server_parse_request(struct web_server *server, struct web_client *client, struct http_server_parsing_state *current_state);

#endif // HTTP_HELPER_SERVER_H