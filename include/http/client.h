#ifndef HTTP_HELPER_CLIENT_H
#define HTTP_HELPER_CLIENT_H

struct client_connection;

#include "http/common.h"

/** Sends chunked data to the server. */
int http_client_send_chunked_data(struct client_connection *client, char *data, size_t data_length);
/** Sends an HTTP request to the server. */
int http_client_send_request(struct client_connection *client, struct http_request *request, const char *data, size_t data_length);
/** Parses an HTTP response from the server. */
int http_client_parse_response(struct client_connection *client, struct http_client_parsing_state *current_state);

#endif // HTTP_HELPER_CLIENT_H