#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <stdbool.h>

#include "../tcp/server.h"
#include "../web/client.h"

#include "../http/common.h"
#include "../http/server.h"

#include "../ws/server.h"
#include "../ws/common.h"

#include "../utils/map.h"

/** A structure representing a server connection over HTTP/WS. */
struct web_server
{
    /** The underlying TCP server. */
    struct tcp_server *tcp_server;

    /** A vector storing all the HTTP routes to their callbacks. */
    struct vector routes; // <server_route>

    /** A map storing all the sockfds to their client structs. */
    struct map clients; // <socket_t sockfd, struct web_client *client>

    /** User defined data to be passed to the event callbacks. */
    void *data;

    /** [HTTP ONLY] A structure representing the configuration for a HTTP server. */
    struct
    {
        /** The maximum length of the HTTP method. Defaults to `7`. */
        size_t max_method_len;
        /** The maximum length of the HTTP path. Defaults to `2000`. */
        size_t max_path_len;
        /** The maximum length of the HTTP version. Defaults to `8`. */
        size_t max_version_len;
        /** The maximum length of the HTTP header name. Defaults to `256`. */
        size_t max_header_name_len;
        /** The maximum length of the HTTP header value. Defaults to `8192`. */
        size_t max_header_value_len;
        /** The maximum number of HTTP headers. Defaults to `24`. */
        size_t max_header_count;
        /** The maximum length of the body. Defaults to `65536`. */
        size_t max_body_len;
    } http_server_config;

    /** [WS ONLY] A structure representing the configuration for a WebSocket server. */
    struct
    {
        /** The maximum number of bytes of one payload/message. Defaults to `65536`. */
        size_t max_payload_len;
        /** 
         * Whether or not the client should record latency (sends a ping on connection and replies to pongs with pings). 
         * Do not set to `true` if the server replies to pong frames.
        */
        bool record_latency;
    } ws_server_config;

    /** Whether or not the server is closing. */
    bool is_closing;
    
    /** The callback for when a client connects, for both HTTP and WS. */
    void (*on_connect)(struct web_server *server, struct web_client *client);

    /** The callback for when a request is malformed for HTTP. */
    void (*on_http_malformed_request)(struct web_server *server, struct web_client *client, enum parse_request_error_types error);

    /** The callback for when a HTTP client disconnects. */
    void (*on_disconnect)(struct web_server *server, socket_t sockfd, bool is_error);
};

/** A structure representing a route. */
struct web_server_route
{
    /** The HTTP callback when the path is requested. */
    void (*on_http_message)(struct web_server *server, struct web_client *client, struct http_request *request);

    /** The callback for when a client requests an upgrade to websocket. Set to NULL if you want to reject upgrades. */
    void (*on_ws_handshake_request)(struct web_server *server, struct web_client *client, struct http_request *request);
    /** The callback for when a WebSocket client sends a heartbeat (ping/pong) frame. */
    void (*on_heartbeat)(struct web_server *server, struct web_client *client, struct ws_message *message);
    /** The callback for when a WebSocket client sends a message to the server. */
    void (*on_ws_message)(struct web_server *server, struct web_client *client, struct ws_message *message);
    /** The callback for when a request is malformed for WS. */
    void (*on_ws_malformed_frame)(struct web_server *server, struct web_client *client, enum ws_frame_parsing_errors error);
    /** The callback for when a WebSocket connection closes. */
    void (*on_ws_close)(struct web_server *server, struct web_client *client, uint16_t code, const char *reason);

    /** The path pattern. */
    const char *path;
};

/** Initializes the web server. */
int web_server_init(struct web_server *http_server, struct sockaddr *address, int backlog);
/** Starts a nonblocking event loop for the web server. */
int web_server_start(struct web_server *server);

/** Creates a route for a path. Note that precedence works by whichever route is created first. */
void web_server_create_route(struct web_server *server, struct web_server_route *route);
/** Finds a route given a path. */
struct web_server_route *web_server_find_route(struct web_server *server, const char *path);
/** Removes a route for a path. */
void web_server_remove_route(struct web_server *server, const char *path);

/** Closes the web server. */
int web_server_close(struct web_server *server);

#endif // SERVER_CONNECTION_H