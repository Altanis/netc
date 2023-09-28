#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include "tcp/client.h"
#include "http/client.h"

/** A structure representing a client connection over HTTP/WS. */
struct client_connection
{
    /** The underlying TCP client. */
    struct tcp_client *client;
    /** The type of client. */
    enum connection_types connection_type;

    /**
     * [WS ONLY]
     * The path the client is connected to.
    */
    char *path;

    /** User defined data to be passed to the event callbacks. */
    void *data;

    union {
        /** 
         * [HTTP ONLY]
         * Specific to a server using this struct as a client representation.
         * Signals if the server should close the connection.
         * Checked when sending a response.
        */
        int server_close_flag;

        /** 
         * [HTTP ONLY]
         * Specific to a client using this struct as a client representation.
         * Signals if the client should close the connection.
         * Checked when receiving a response.
        */
        int client_close_flag;
    };

    union
    {
        /** 
         * [HTTP ONLY]
         * Specific to a server using this struct as a client representation.
         * The parsing state of the server's incoming request.
        */
        struct http_server_parsing_state server_parsing_state;

        /** 
         * [HTTP ONLY]
         * Specific to a client using this struct as a client representation.
         * The parsing state of the client's incoming response.
        */
        struct http_client_parsing_state client_parsing_state;
    };

    /** The callback for when a client connects. */
    void (*on_connect)(struct client_connection *client);
    /** The callback for when a response is received. */
    void (*on_data)(struct client_connection *client, struct http_response response);
    /** The callback for when a response is malformed. */
    void (*on_malformed_response)(struct client_connection *client, enum parse_response_error_types error);
    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct client_connection *client, int is_error);
};

/** Initializes the client. */
int client_init(struct client_connection *client, struct sockaddr address, enum connection_types connection_type);
/** Starts a nonblocking event loop for the client. */
int client_start(struct client_connection *client);
/** Closes the client. */
int client_close(struct client_connection *client);

#endif // CLIENT_CONNECTION_H