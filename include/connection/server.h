#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include "tcp/server.h"
#include "connection/client.h"
#include "http/common.h"
#include "utils/map.h"

/** A structure representing a server connection over HTTP/WS. */
struct server_connection
{
    /** The underlying TCP server. */
    struct tcp_server *server;

    /** A vector storing all the HTTP routes to their callbacks. */
    struct vector routes; // <server_route>

    /** A map storing all the sockfds to their client structs. */
    struct map clients; // <socket_t sockfd, struct client_connection *client>

    /** User defined data to be passed to the event callbacks. */
    void *data;

    /**
     * [HTTP ONLY] 
     * A structure representing the configuration for the HTTP server.
    */
    struct http_server_config
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
        /** The amount of time for waiitng on the completion of parsing method, path, and version. Defaults to `10`. */
        size_t request_line_timeout_seconds;
        /** The amount of time for waiting on the completion of parsing HTTP headers. Defaults to `10`. */
        size_t headers_timeout_seconds;
        /** The amount of time for waiting on the completion of parsing the HTTP body. Defaults to `10`. */
        size_t body_timeout_seconds;
    } config;
    
    /** The callback for when a client connects. */
    void (*on_connect)(struct server_connection *server, struct client_connection *client);
    /** The callback for when a response is malformed. */
    void (*on_malformed_request)(struct server_connection *server, struct client_connection *client, enum parse_request_error_types error);
    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct server_connection *server, socket_t sockfd, int is_error);
};

/** A structure representing a route. */
struct server_route
{
    union
    {
        /** The HTTP callback when the path is requested. */
        void (*http_callback)(struct server_connection *server, struct client_connection *client, struct http_request request);
        /** The WebSocket callback when the server receives a message. */
        void (*ws_callback)(struct server_connection *server, struct client_connection *client, struct ws_message message);
    };

    /** The path pattern. */
    char *path;
    /** The connection the route is using. */
    enum connection_types connection_type;
};

/** Initializes the HTTP server. */
int http_server_init(struct server_connection *http_server, struct sockaddr address, int backlog);
/** Starts a nonblocking event loop for the HTTP server. */
int http_server_start(struct server_connection *server);

/** Creates a route for a path. Note that precedence works by whichever route is created first. */
void http_server_create_route(struct server_connection *server, struct server_route *route);
/** Finds a route given a path. */
struct server_route *http_server_find_route(struct server_connection *server, const char *path);
/** Removes a route for a path. */
void http_server_remove_route(struct server_connection *server, const char *path);

/** Sends chunked data to the client. */
int http_server_send_chunked_data(struct server_connection *server, struct client_connection *client, char *data, size_t data_length);
/** Sends the HTTP response. */
int http_server_send_response(struct server_connection *server, struct client_connection *client, struct http_response *response, const char *data, size_t length);
/** Parses the HTTP request. */
int http_server_parse_request(struct server_connection *server, struct client_connection *client, struct http_server_parsing_state *current_state);

/** Closes the HTTP server. */
int http_server_close(struct server_connection *server);
/** Closes a HTTP client connection. */
int http_server_close_client(struct server_connection *server, struct client_connection *client);

#endif // SERVER_CONNECTION_H