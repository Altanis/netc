#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "tcp/server.h"

#define MAX_HTTP_METHOD_LEN 7
#define MAX_HTTP_PATH_LEN 2000
#define MAX_HTTP_VERSION_LEN 8
#define MAX_HTTP_HEADER_NAME_LEN 256
#define MAX_HTTP_HEADER_VALUE_LEN 4096
#define MAX_HTTP_HEADER_LEN (MAX_HTTP_HEADER_NAME_LEN + MAX_HTTP_HEADER_VALUE_LEN + 1)
#define MAX_HTTP_HEADER_COUNT 24

/** A structure representing the HTTP request. */
struct http_request
{
    /** The HTTP method. */
    char* method;
    /** The complete HTTP path to request to. */
    char* path;
    /** The HTTP version. */
    char* version;

    /** The HTTP headers. */
    struct vector* headers; // <http_header>
};

/** A structure representing the HTTP response. */
struct http_response
{
    /** The HTTP version. */
    char* version;
    /** The HTTP status code. */
    int status_code;
    /** The HTTP status message. */
    char* status_message;

    /** The HTTP headers. */
    struct vector* headers;
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

/** A structure representing the HTTP server. */
struct http_server
{
    /** The TCP server. */
    struct netc_tcp_server server;

    /** User defined data to be passed to the event callbacks. */
    void* data;
    
    /** The callback for when a client connects. */
    void (*on_connect)(struct http_server* server, struct netc_tcp_client* client, void* data);
    /** The callback for when a request is received. */
    void (*on_request)(struct http_server* server, socket_t sockfd, void* data);
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
/** Closes the HTTP server. */
int http_server_close(struct http_server* server);

#endif // HTTP_SERVER_H