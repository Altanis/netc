#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "tcp/server.h"

/** A structure representing the HTTP request. */
struct http_request
{
    /** The HTTP method. */
    char* method;
    /** The HTTP path the request was sent to. */
    char* path;
    /** The HTTP version. */
    char* version;

    /** The parsed query in the path. */
    struct vector* query; // <http_query>
    /** The HTTP headers. */
    struct vector* headers; // <http_header>
    /** The HTTP body. */
    char* body;
};

/** A structure representing the HTTP response. */
struct http_response
{
    /** The HTTP status code. */
    int status_code;
    /** The HTTP version. */
    char* version;

    /** The HTTP headers. */
    struct vector* headers; // <http_header>
    /** The HTTP body. */
    char* body;
};

/** A structure representing the HTTP header. */
struct http_header
{
    /** The header's name. */
    char* name;
    /** The header's value. */
    char* value;
};

/** A structure representing a query key-value pair. */
struct http_query
{
    /** The query's key. */
    char* key;
    /** The query's value. */
    char* value;
};

/** A structure representing an entry in the routes map. */
struct http_route
{
    /** The callback. */
    void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request);
    /** The path. */
    char* path;
};

/** A structure representing the HTTP server. */
struct http_server
{
    /** The TCP server. */
    struct netc_tcp_server server;
    /** A map storing all the routes to their callbacks. */
    struct map* routes; // <char* path, void (*)(struct http_server* server, socket_t sockfd, struct http_request request)>

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** A structure representing the configuration for the HTTP server. */
    struct netc_http_server_config
    {
        /** The maximum length of the HTTP method (excluding the null terminator). Defaults to `8`. */
        size_t max_method_len;
        /** The maximum length of the HTTP path (excluding the null terminator). */
        size_t max_path_len;
        /** The maximum length of the HTTP version (excluding the null terminator). */
        size_t max_version_len;
        /** The maximum length of the HTTP header name (excluding the null terminator). */
        size_t max_header_name_len;
        /** The maximum length of the HTTP header value (excluding the null terminator). */
        size_t max_header_value_len;
        /** The maximum length of the HTTP header (excluding the null terminator). */
        size_t max_header_len;
        /** The maximum number of HTTP headers. */
        size_t max_header_count;
        /** The maximum length of the body (excluding the null terminator). */
        size_t max_body_len;
        /** The amount of time for waiting on the completion of parsing HTTP headers. */
        size_t headers_timeout_seconds;
        /** The amount of time for waiting on the completion of parsing the HTTP body. */
        size_t body_timeout_seconds;
    } config;
    
    /** The callback for when a client connects. */
    void (*on_connect)(struct http_server* server, struct netc_tcp_client* client, void* data);
    /** The callback for when a client disconnects. */
    void (*on_disconnect)(struct http_server* server, socket_t sockfd, int is_error, void* data);
};

/** Whether or not the server is listening for events. */
extern __thread int netc_http_server_listening;

/** Initializes the HTTP server. */
int http_server_init(struct http_server* server, int ipv6, int reuse_addr, struct sockaddr* address, socklen_t addrlen, int backlog);
/** Starts a nonblocking event loop for the HTTP server. */
int http_server_start(struct http_server* server);

/** Parses the HTTP request. */
int http_server_parse_request(struct http_server* server, socket_t sockfd, struct http_request* request);
/** Sends the HTTP response. */
int http_server_send_response(struct http_server* server, struct http_response* response);

/** Creates a route for a path. */
void http_server_create_route(struct http_server* server, const char* path, void (*callback)(struct http_server* server, socket_t sockfd, struct http_request request));
/** Finds a route given a path. */
void (*http_server_find_route(struct http_server* server, const char* path))(struct http_server* server, socket_t sockfd, struct http_request request);
/** Removes a route for a path. */
void http_server_remove_route(struct http_server* server, const char* path);

/** Closes the HTTP server. */
int http_server_close(struct http_server* server);

#endif // HTTP_SERVER_H