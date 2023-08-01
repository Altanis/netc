#ifndef HTTP_H
#define HTTP_H

#include "tcp/server.h"

/** The different HTTP methods. */
enum http_method
{
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_PATCH
};

/** A structure representing the HTTP request. */
struct http_request
{
    /** The HTTP method. */
    enum http_method method;
    /** The complete HTTP path to request to. */
    char* path;
    /** The HTTP version. */
    char* version;

    /** The HTTP headers. */
    struct vector* headers; // <http_header>
    /** The HTTP body. */
    char* body;
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
    struct netc_tcp_server* server;

    /** The callback for when a request is received. */
    void (*on_request)(struct http_server* server, struct netc_tcp_client* client, struct http_request* request);
};

/** Whether or not the server is running. */
extern __thread int netc_http_server_listening;

/** A basic loop for a non-blocking HTTP server to respond to requests. */
int http_server_main_loop(struct http_server* server);

/** Initializes the HTTP server. */
int http_server_init(struct http_server* server, struct netc_tcp_server_config config);
/** Starts the HTTP server. */
int http_server_start(struct http_server* server);
/** Parses the HTTP request. */
int http_server_parse_request(struct http_server* server, char* buffer, struct http_request* request);
/** Sends the HTTP response. */
int http_server_send_response(struct http_server* server, struct http_response* response);
/** Closes the HTTP server. */
int http_server_close(struct http_server* server);

#endif // HTTP_H