#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <stdbool.h>

#include "../tcp/client.h"
#include "../http/client.h"
#include "../ws/client.h"
#include "../ws/common.h"

/** A structure representing a client connection over HTTP/WS. */
struct web_client
{
    /** The underlying TCP client. */
    struct tcp_client *tcp_client;
    
    /** The type of client. */
    enum connection_types connection_type;

    /** [WS ONLY] The path the client is connected to. */
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
        struct http_server_parsing_state http_server_parsing_state;

        /** 
         * [HTTP ONLY]
         * Specific to a client using this struct as a client representation.
         * The parsing state of the client's incoming response.
        */
        struct http_client_parsing_state http_client_parsing_state;

        /**
         * [WS ONLY]
         * The parsing state of a connection's message.
        */
       struct ws_frame_parsing_state ws_parsing_state;
    };

    /** The callback for when the client connects via HTTP. */
    void (*on_http_connect)(struct web_client *client);
    /** The callback for when the client connects via WS. */
    void (*on_ws_connect)(struct web_client *client);

    /** The callback for when a HTTP response is received. */
    void (*on_http_response)(struct web_client *client, struct http_response response);
    /** The callback for when a WS message is received. */
    void (*on_ws_message)(struct web_client *client, struct ws_message message);

    /** The callback for when a response is malformed. */
    void (*on_http_malformed_response)(struct web_client *client, enum parse_response_error_types error);
    /** The callback for when a frame is malformed. */
    void (*on_ws_malformed_frame)(struct web_client *client, enum parse_frame_error_types error);

    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct web_client *client, bool is_error);
};

/** Initializes the client. */
int web_client_init(struct web_client *client, struct sockaddr address);
/** Starts a nonblocking event loop for the client. */
int web_client_start(struct web_client *client);
/** Closes the client. */
int web_client_close(struct web_client *client);

#endif // CLIENT_CONNECTION_H