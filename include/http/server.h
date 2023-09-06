#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "tcp/server.h"
#include "http/common.h"

/** A structure representing the HTTP server. */
struct http_server
{
    /** The TCP server. */
    struct tcp_server server;
    /** A map storing all the routes to their callbacks. */
    struct vector routes; // <http_route>

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** A structure representing the configuration for the HTTP server. */
    struct netc_http_server_config
    {
        /** The maximum length of the HTTP method. Defaults to `7`. */
        size_t max_method_len;
        /** The maximum length of the HTTP path. Defaults to `2000`. */
        size_t max_path_len;
        /** The maximum length of the HTTP version. Defaults to `8`. */
        size_t max_version_len;
        /** The maximum length of the HTTP header name. Defaults to `256`. */
        size_t max_header_name_len;
        /** The maximum length of the HTTP header value. Defaults to `4096`. */
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
    void (*on_connect)(struct http_server* server, struct tcp_client* client, void* data);
    /** The callback for when a response is malformed. */
    void (*on_malformed_request)(struct http_server* server, socket_t sockfd, enum parse_request_error_types error, void* data);
    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct http_server* server, socket_t sockfd, int is_error, void* data);
};

/** A structure representing an entry in the routes map. */
struct http_route
{
    /** The callback. */
    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request);
    /** The path. */
    char* path;
};

/** Whether or not the server is listening for events. */
extern __thread int netc_http_server_listening;

/** Initializes the HTTP server. */
int http_server_init(struct http_server* http_server, struct sockaddr address, int backlog);
/** Starts a nonblocking event loop for the HTTP server. */
int http_server_start(struct http_server* server);

/** Creates a route for a path. Note that precedence works by whichever route is created first. */
void http_server_create_route(struct http_server* server, struct http_route* route);
/** Finds a route given a path. */
void (*http_server_find_route(struct http_server* server, const char* path))(struct http_server* server, socket_t sockfd, struct http_request request);
/** Removes a route for a path. */
void http_server_remove_route(struct http_server* server, const char* path);

/** Sends chunked data to the client. */
int http_server_send_chunked_data(struct http_server* server, socket_t sockfd, char* data);
/** Sends the HTTP response. */
int http_server_send_response(struct http_server* server, socket_t sockfd, struct http_response* response);
/** Parses the HTTP request. */
int http_server_parse_request(struct http_server* server, socket_t sockfd, struct http_request* request);

/** Closes the HTTP server. */
int http_server_close(struct http_server* server);
/** Closes a HTTP client connection. */
int http_server_close_client(struct http_server* server, socket_t sockfd);

#endif // HTTP_SERVER_H