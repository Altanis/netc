#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "tcp/client.h"
#include "http/common.h"

/** A structure representing the HTTP client. */
struct http_client
{
    /** The TCP client. */
    struct tcp_client *client;

    /** User defined data to be passed to the event callbacks. */
    void *data;

    union {
        /** 
         * Specific to a server using this struct as a client representation.
         * Signals if the server should close the connection.
         * Checked when sending a response.
        */
        int server_close_flag;

        /** 
         * Specific to a client using this struct as a client representation.
         * Signals if the client should close the connection.
         * Checked when receiving a response.
        */
        int client_close_flag;
    };

    union
    {
        /** 
         * Specific to a server using this struct as a client representation.
         * The parsing state of the server's incoming request.
        */
        struct http_server_parsing_state server_parsing_state;

        /** 
         * Specific to a client using this struct as a client representation.
         * The parsing state of the client's incoming response.
        */
        struct http_client_parsing_state client_parsing_state;
    };

    /** The callback for when a client connects. */
    void (*on_connect)(struct http_client *client, void *data);
    /** The callback for when a response is received. */
    void (*on_data)(struct http_client *client, struct http_response response, void *data);
    /** The callback for when a response is malformed. */
    void (*on_malformed_response)(struct http_client *client, enum parse_response_error_types error, void *data);
    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct http_client *client, int is_error, void *data);
};

/** Initializes the HTTP client. */
int http_client_init(struct http_client *client, struct sockaddr address);
/** Starts a nonblocking event loop for the HTTP client. */
int http_client_start(struct http_client *client);

/** Sends chunked data to the server. */
int http_client_send_chunked_data(struct http_client *client, char *data, size_t data_length);
/** Sends an HTTP request to the server. */
int http_client_send_request(struct http_client *client, struct http_request *request, const char *data, size_t data_length);
/** Parses an HTTP response from the server. */
int http_client_parse_response(struct http_client *client, struct http_client_parsing_state *current_state);

/** Closes the HTTP client. */
int http_client_close(struct http_client *client);

#endif // HTTP_CLIENT_H