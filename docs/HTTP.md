# HTTP Documentation

## Table of Contents
1. [HTTP Server](#http-server)
    1. [Creating a HTTP Server](#creating-a-http-server)
    2. [Setting Up Routes](#setting-up-routes)
    3. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    4. [Sending Responses](#sending-data)
    5. [Sending Files](#sending-files-server)
2. [HTTP Client](#http-client)
    1. [Creating a HTTP Client](#creating-a-http-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Sending Requests](#sending-requests)
    4. [Sending Files](#sending-files-client)

## HTTP Server <a name="http-server"/>
The HTTP component of this library is purely asynchronous, and the underlying TCP mechanism will only be nonblocking.

### Creating a HTTP Server <a name="creating-a-http-server"/>
Creating a HTTP server is a straightforward process. The following code snippet shows how to create a HTTP server that listens on port 8080 and handles incoming connections.

```c
#include <stdio.h>
#include <netc/http/server.h>

struct http_server server = {0};
server.on_connect = on_connect;
server.on_malformed_request = on_malformed_request;
server.on_disconnect = on_disconnect;

/** You can change configuration measures for security when parsing requests. Refer to the HTTP Server header file. */
// server.config = /* struct config { max_method_len, max_path_len, ... } */;

struct sockaddr_in addr =
{
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = htons(8080)
};

int r = http_server_init(&server, *(struct sockaddr*)&addr, 10 /** the max number of connections in backlog */);
if (r != 0) 
{
    /** Handle error. */
    netc_perror("failure", NULL);
    return 1;
}

/** Start the main loop. */
r = http_server_start(&server); // This will block until the server is closed.
if (r != 0) 
{
    /** Handle error. */
    netc_perror("failure", NULL);
    return 1;
}
```

### Setting Up Routes <a name="setting-up-routes"/>
Routes are a way to divide the incoming requests into different handlers based off the path request. The following code snippet shows how to set up routes.

```c
// assume that the server is already initialized as `server`
struct http_route echo_route =
{
    .path = "/echo",
    .callback = callback_echo,
};

struct http_route default_route =
{
    .path = "/*",
    .callback = callback_404,
};

/** 
 * Please note hierarchy matters. If a path matches two routes,
 * it will use the callback initialized first. 
 * Ensure wildcard routes are initialized last.
 */
http_server_add_route(&server, &echo_route);
http_server_add_route(&server, &default_route);
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-server"/>
You must use asynchronous events to be notified of when incoming connections, requests, and disconnections occur. The following code snippet shows how the event callbacks work.

```c
#include <stdio.h>
#include <netc/http/server.h>

void on_connect(struct http_server *server, struct tcp_client *client)
{
    /** Refer to documentation and header files for more information about TCP. */
    printf("A connection has been established.\n");
};

void on_malformed_request(struct http_server *server, socket_t sockfd, enum parse_request_error_types error, void *data)
{
    /** An incoming request was unable to be parsed. */

    printf("A malformed request has been received.\n");
    switch (error)
    {
        case REQUEST_PARSE_ERROR_RECV: printf("the recv syscall failed.\n"); break;
        case REQUEST_PARSE_ERROR_BODY_TOO_BIG: printf("the body was too big.\n"); break;
        case REQUEST_PARSE_ERROR_TOO_MANY_HEADERS: printf("too many headers were sent.\n"); break;
        case REQUEST_PARSE_ERROR_TIMEOUT:
        {
            printf("the request timed out, likely due to a DoS attempt.\n"); 
            http_server_close_client(server, sockfd);
            break;
        };
    }
};

static void on_disconnect(struct http_server *server, socket_t sockfd, int is_error, void *data)
{
    if (sockfd == server->sockfd)
        printf("The server has been closed.\n");
    else
        printf("A client has disconnected.\n");
};

/** The callback for the /echo route (in the example above). */
void callback_echo(struct http_server *server, socket_t sockfd, struct http_request request)
{
    printf("An incoming request has come!\n");

    /** The library has its own string implementation for optimizations. You need to use a getter for the data in a string_t struct. */
    printf("METHOD: %s", http_request_get_method(&request));
    printf("PATH: %s", http_request_get_path(&request));
    printf("VERSION: %s", http_request_get_version(&request));

    size_t first_operand = 0;
    size_t second_operand = 0;

    /** request.query and request.headers are vectors. */

    for (size_t i = 0; i < request.query.size; ++i)
    {
        struct http_query *query = vector_get(&request.query, i);
        printf("QUERY: %s=%s", http_query_get_key(query), http_query_get_value(query));
    };

    for (size_t i = 0; i < request.headers.size; ++i)
    {
        struct http_header *header = vector_get(&request.headers, i);
        printf("HEADER: %s=%s", http_header_get_name(header), http_header_get_value(header));
    };

    char *body = http_request_get_body(&request);
    // Printing this will cause undefined behavior if the body is binary data.
    // Instrad, do this:
    printf("BODY:");
    for (size_t i = 0; i < request.body_size; ++i) printf("%c", body[i]);
    printf("\n"); 
};
```

### Sending Responses <a name="sending-data"/>
Sending a response to a request is a straightforward process. The following code snippet shows how to send a response (based off our previous examples).

```c
void callback_404(struct http_server *server, socket_t sockfd, struct http_request request)
{
    struct http_response response = {0};
    /** Similar to before, you can't use raw strings. You need to use a setter. */
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 404;
    http_response_set_status_message(&response, "Not Found");

    /** Create headers to send to the client. */
    struct http_header content_type_header = {0};
    http_header_set_name(&content_type_header, "Content-Type");
    http_header_set_value(&content_type_header, "text/plain");

    // struct http_header content_length_header = {0};
    // http_header_set_name(&content_length_header, "Content-Length");
    // http_header_set_value(&content_length_header, "9");

    // The Content-Length header is filled in automatically by the library.
    // Note if there are more than 15 digits in the Content-Length header, 
    // it will cause undefined behavior. Put your own Content-Length header
    // in that case.

    /** Initialize a vector to hold each header. */
    /** response.headers is already an initialized vector. */
    vector_init(&response.headers, 1 /** capacity, gets resized if need be */, sizeof(struct http_header));
    vector_push(&response.headers, &content_type_header);
    // vector_push(&response.headers, &content_length_header);

    // Keep in mind there is no setter for the response body.
    // You should directly provide the data to the send function.

    /** Send the response. */
    http_server_send_response(server, sockfd, &response, "Not Found" /** the buffer to send */, 9 /** the length of the buffer */);

    // NOTE: You are able to send binary data.
    // Ensure you set the correct Content-Type header.

    /** Free the response.headers vector. */
    vector_free(&response.headers);
};

void callback_echo(struct http_server *server, socket_t sockfd, struct http_request request)
{
    printf("An incoming request has come!\n");

    /** The library has its own string implementation for optimizations. You need to use a getter for the data in a string_t struct. */
    printf("METHOD: %s", http_request_get_method(&request));
    printf("PATH: %s", http_request_get_path(&request));
    printf("VERSION: %s", http_request_get_version(&request));

    /** request.query and request.headers are vectors. */

    for (size_t i = 0; i < request.query.size; ++i)
    {
        struct http_query *query = vector_get(&request.query, i);
        printf("QUERY: %s=%s", http_query_get_key(query), http_query_get_value(query));
    };

    for (size_t i = 0; i < request.headers.size; ++i)
    {
        struct http_header *header = vector_get(&request.headers, i);
        printf("HEADER: %s=%s", http_header_get_name(header), http_header_get_value(header));
    };

    char *body = http_request_get_body(&request);
    // Printing this will cause undefined behavior if the body is binary data.
    // Instrad, do this:
    printf("BODY:");
    for (size_t i = 0; i < request.body_size; ++i) printf("%c", body[i]);
    printf("\n"); 

    struct http_response response = {0};
    /** Similar to before, you can't use raw strings. You need to use a setter. */
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 200;
    http_response_set_status_message(&response, "OK");

    /** Create headers to send to the client. */
    struct http_header content_type_header = {0};
    http_header_set_name(&content_type_header, "Content-Type");
    http_header_set_value(&content_type_header, "text/plain");

    // struct http_header content_length_header = {0};
    // http_header_set_name(&content_length_header, "Content-Length");
    // http_header_set_value(&content_length_header, strlen(body));

    // The Content-Length header is filled in automatically by the library.
    // Note if there are more than 15 digits in the Content-Length header, 
    // it will cause undefined behavior. Put your own Content-Length header
    // in that case.

    /** Initialize a vector to hold each header. */
    /** response.headers is already an initialized vector. */
    vector_init(&response.headers, 1 /** capacity, gets resized if need be */, sizeof(struct http_header));
    vector_push(&response.headers, &content_type_header);
    // vector_push(&response.headers, &content_length_header);

    // Keep in mind there is no setter for the response body.
    // You should directly provide the data to the send function.

    /** Send the response. */
    http_server_send_response(server, sockfd, &response, body, request.body_size);

    // NOTE: You are able to send binary data.
    // Replace NULL with your binary data, and 0
    // with the length. Ensure you set the correct
    // Content-Type header.

    /** Free the response.headers vector. */
    vector_free(&response.headers);    
};
```

### Sending Files <a name="sending-files-server"/>

TBD.

## HTTP Client <a name="http-server"/>
The HTTP component of this library is purely asynchronous, and the underlying TCP mechanism will only be nonblocking.

### Creating a HTTP Client <a name="creating-a-http-client"/>
Creating a HTTP client is a straightforward process. The following code snippet shows how to create a HTTP client that connects to localhost:8080.

```c
#include <stdio.h>
#include <netc/http/client.h>

struct http_client client = {0};
client.on_connect = on_connect;
client.on_malformed_response = on_malformed_response;
client.on_data = on_data;
client.on_disconnect = on_disconnect;

struct sockaddr_in sockaddr = 
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
};

if (inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr) != 1) printf("failed to convert address.\n"); /** Handle error. */

/** Create the HTTP client. */
int init_result = http_client_init(&client, *(struct sockaddr*)&sockaddr);
if (init_result != 0) 
{
    /** Handle error. */
    netc_perror("failure", NULL);
    return 1;
}

/** Start the main loop. */
int r = http_client_start(&client); // This will block until the client is closed.
if (r != 0) 
{
    /** Handle error. */
    netc_perror("failure", NULL);
    return 1;
}
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-client"/>
You must use asynchronous events to be notified of when incoming connections, responses, and disconnections occur. The following code snippet shows how the event callbacks work.

```c
#include <stdio.h>
#include <netc/http/client.h>

void on_connect(struct http_client *client, void *data)
{
    printf("A connection has been established.\n");
};

void on_malformed_response(struct http_client *client, enum parse_response_error_types error, void *data)
{
    /** An incoming response was unable to be parsed. */
    /** This (usually) should NOT happen. If it ever happens errnoeously, please report an issue! */
    printf("A malformed response has been received.\n");

    switch (error)
    {
        case RESPONSE_PARSE_ERROR_RECV: printf("the recv syscall failed.\n"); break;
    };
};

void on_data(struct http_client *client, struct http_response response, void *data)
{
    /** Returns a response to a request. */

    printf("An incoming response has come!\n");
    /** The library has its own string implementation for optimizations. You need to use a getter for the data in a string_t struct. */
    printf("VERSION: %s", http_response_get_version(&response));
    printf("STATUS CODE: %d", response.status_code);
    printf("STATUS MESSAGE: %s", http_response_get_status_message(&response));

    /** response.headers is a vector. */

    for (size_t i = 0; i < response.headers.size; ++i)
    {
        struct http_header *header = vector_get(&response.headers, i);
        printf("HEADER: %s=%s", http_header_get_name(header), http_header_get_value(header));
    };

    char *body = http_response_get_body(&response);
    // Printing this will cause undefined behavior if the body is binary data.
    // Instrad, do this:
    printf("BODY:");
    for (size_t i = 0; i < response.body_size; ++i) printf("%c", body[i]);
    printf("\n");
};

void on_disconnect(struct http_client *client, int is_error, void *data)
{
    printf("Disconnected from the server.\n");
};
```

### Sending Requests <a name="sending-requests"/>
Sending a request to a server is a straightforward process. The following code snippet shows how to send a request (based off our previous examples).

```c
// assume that the client is already initialized as `client`

struct http_request request = {0};
/** Similar to before, you can't use raw strings. You need to use a setter. */
http_request_set_method(&request, "GET");
http_request_set_path(&request, "/echo");
http_request_set_version(&request, "HTTP/1.1");

/** Create headers to send to the server. */
struct http_header content_type_header = {0};
http_header_set_name(&content_type_header, "Content-Type");
http_header_set_value(&content_type_header, "text/plain");

// struct http_header content_length_header = {0};
// http_header_set_name(&content_length_header, "Content-Length");
// http_header_set_value(&content_length_header, "13");

// The Content-Length header is filled in automatically by the library.
// Note if there are more than 15 digits in the Content-Length header, 
// it will cause undefined behavior. Put your own Content-Length header
// in that case.

/** Initialize a vector to hold each header. */
/** request.headers is already an initialized vector. */
vector_init(&request.headers, 1 /** capacity, gets resized if need be */, sizeof(struct http_header));
vector_push(&request.headers, &content_type_header);
// vector_push(&request.headers, &content_length_header);

// Keep in mind there is no setter for the request body.
// You should directly provide the data to the send function.

/** Send the request. */
http_client_send_request(client, &request, "hello" /** the buffer to send */, 5 /** the length of the buffer */);

// NOTE: You are able to send binary data.
// Replace NULL with your binary data, and 0
// with the length. Ensure you set the correct
// Content-Type header.

/** Free the request.headers vector. */
vector_free(&request.headers);
```

### Sending Files <a name="sending-files-client"/>

TBD.