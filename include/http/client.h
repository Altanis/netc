#ifndef HTTP_HELPER_CLIENT_H
#define HTTP_HELPER_CLIENT_H

struct web_client;

#include "./common.h"

/** Sends chunked data to the server. Returns 1 for success, otherwise a failure. */
int http_client_send_chunked_data(struct web_client *client, char *data, size_t data_length);
/** Sends an HTTP request to the server. Returns 1 for success, otherwise a failure. */
int http_client_send_request(struct web_client *client, struct http_request *request, char *data, size_t data_length);
/** Parses an HTTP response from the server. */
int http_client_parse_response(struct web_client *client, struct http_client_parsing_state *current_state);

#endif // HTTP_HELPER_CLIENT_H