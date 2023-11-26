# HTTP Documentation

## Table of Contents
1. [HTTP Server](#http-server)
    1. [Creating an HTTP Server](#creating-an-http-server)
    2. [Setting Up Routes](#setting-up-routes)
    3. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    4. [Sending Responses](#sending-data)
    5. [Sending Files](#sending-files-server)
    6. [Keep Alive](#keep-alive-server)
2. [HTTP Client](#http-client)
    1. [Creating an HTTP Client](#creating-an-http-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Sending Requests](#sending-requests)
    4. [Sending Files](#sending-files-client)
    5. [Keep Alive](#keep-alive-client)

## HTTP Server <a name="http-server"/>

The HTTP component of this library is purely asynchronous, and the underlying TCP mechanism will only be nonblocking. The HTTP server only supports HTTP/1.1 for now.

### Creating an HTTP Server <a name="creating-an-http-server"/>
Creating a HTTP server is a straightforward process. The following code snippet shows how to create a HTTP server that listens on port 8080 and handles incoming connections.

```c
#include <stdio.h>
#include "netc/include/http/server.h"

/** A server capable of serving HTTP/1.1 content. */
struct web_server server;
server.on_connect = on_connect;
server.on_http_malformed_request = on_http_malformed_request;
server.on_disconnect = on_disconnect;

/** You can change configurations for the HTTP server as such. Their defaults are visible in the include/web/server.h header file. */
web_server.http_server_config.max_body_len = 2048;

struct sockaddr_in addr =
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
    .sin_addr.s_addr = INADDR_ANY
};

int init_result = web_server_init(&server, (struct sockaddr *)&addr, 10 /** maximum number of connections in backlog */);
if (init_result != 0)
{
    netc_perror("web_server_init");
    return 1;
};

int start_result = web_server_start(&server); // This function will block until the server is stopped.
if (start_result != 0)
{
    netc_perror("web_server_start");
    return 1;
};
```

### Setting Up Routes <a name="setting-up-routes"/>
The HTTP server supports routing, which means that you can specify a function to be called when a certain path is requested. The following code snippet shows how to set up a route.

```c
#include <stdio.h>
#include "netc/include/http/server.h"

/** Assume the server is initialised under the `server` name. */
struct web_server_route echo_route =
{
    .path = "/echo",
    .on_http_message = callback_echo,
};

struct web_server_route default_route =
{
    .path = "/*",
    .on_http_message = callback_404,
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
The HTTP server is asynchronous, which means that it will only use one thread to poll for events, and code can be executed in "event callbacks" when an event occurs on the server. The following code snippet shows how to handle events.

```c
#include <stdio.h>
#include "netc/include/http/server.h"

void on_connect(struct web_server *server, struct web_client *http_client)
{
    printf("Client connected.\n");

    /** Refer to documentation and header files for more information about TCP. */
    struct tcp_client *tcp_client = client->tcp_client;
    
    // get the ip of the client
    char ip = calloc(INET6_ADDRSTRLEN, sizeof(char));
    if (tcp_client->sockaddr->sa_family == AF_INET)
    {
        inet_ntop(AF_INET, &(((struct sockaddr_in*)tcp_client->sockaddr)->sin_addr), ip, INET6_ADDRSTRLEN);
    }
    else
    {
        inet_ntop(AF_INET6, &(((struct sockaddr_in6*)tcp_client->sockaddr)->sin6_addr), ip, INET6_ADDRSTRLEN);
    };

    printf("Client IP: %s\n", ip);
    http_client->data = ip; // Ensure data stored in the client is freed in the on_disconnect callback. Also ensure data stored is on the heap and is persistent (malloc, calloc, etc).
};

void on_http_malformed_request(struct web_server *server, struct web_client *http_client, enum parse_request_error_types error)
{
    printf("Client sent malformed request: ");
    switch (error)
    {
        case REQUEST_PARSE_ERROR_RECV: printf("the recv syscall failed.\n"); break;
        case REQUEST_PARSE_ERROR_BODY_TOO_BIG: printf("the body was too big.\n"); break;
        case REQUEST_PARSE_ERROR_TOO_MANY_HEADERS: printf("too many headers were sent.\n"); break;
        case REQUEST_PARSE_ERROR_TIMEOUT: printf("the request timed out, likely due to a DoS attempt.\n"); break;
    };
    printf("\n");

    /** If you want to send a response to the client, you can do so here. The socket will get closed after the callback is done. */
};

void on_disconnect(struct web_server* server, struct web_client* http_client)
{
    printf("Client disconnected.\n");
    free(http_client->data);
};

/** The callback for the /echo route (in the example above). */
void callback_echo(struct web_server *server, struct web_client *client, struct http_request *request)
{
    printf("An incoming request has come!\n");

    /** The library has its own string implementation for optimizations. You need to use a getter for the data in a string_t struct. */
    printf("METHOD: %s", http_request_get_method(request));
    printf("PATH: %s", http_request_get_path(request));
    printf("VERSION: %s", http_request_get_version(request));

    /** request->query and request->headers are vectors. */

    for (size_t i = 0; i < request->query.size; ++i)
    {
        struct http_query *query = vector_get(&request->query, i);
        printf("QUERY: %s=%s", http_query_get_key(query), http_query_get_value(query));
    };

    for (size_t i = 0; i < request->headers.size; ++i)
    {
        struct http_header *header = vector_get(&request->headers, i);
        printf("HEADER: %s=%s", http_header_get_name(header), http_header_get_value(header));
    };

    char *body = http_request_get_body(request);
    // In the event this is binary data, printing may terminate early or cause UB
    // because the binary data may contain '\0' or no '\0' at all.
    printf("BODY:");
    for (size_t i = 0; i < request->body_size; ++i) printf("%c", body[i]);
    printf("\n"); 
};
```

### Sending Responses <a name="sending-data"/>
The HTTP server supports sending responses to clients. The following code snippet shows how to send a response.

```c
#include <stdio.h>
#include "netc/include/http/server.h"

void callback_echo(struct web_server *server, struct web_client *client, struct http_request *request)
{
    char *body = http_request_get_body(request);
    
    /** Send back headers. */
    const char *headers[1][2] = 
    {
        "Content-Type", "text/plain",
        /** This is optional, as the library will automatically add this header if it is not present. If the data length is greater than 15 digits, manually fill this header in. */
        // "Content-Length", "13",
    };

    /** Send back data. */
    struct http_response response;
    http_response_build(&response, "HTTP/1.1", 200 /** status code */, headers, 1 /** length of headers */);

    int send_result = http_server_send_response(server, client, &response, body, request->body_size);
    if (send_result < 1)
    {
        netc_perror("http_server_send_response");
        return 1;
    };
};
```

### Sending Files <a name="sending-files-server"/>
The HTTP server supports sending files to clients. The following code snippet shows how to send a file.

```c
#define CHUNKED 0

if (CHUNKED == 0)
{
    const char *headers[1][2] =
    {
        {"Content-Type", "image/png"}
    };

    FILE *file = fopen("image.png", "rb");
    if (file == NULL)
    {
        perror("fopen");
        return 1;
    };

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    char file_data[file_size];
    fseek(file, 0, SEEK_SET);
    fread(file_data, 1, file_size, file);
    fclose(file);

    struct http_response response;
    http_response_build(&response, "HTTP/1.1", 200, headers, 1);

    int send_result = http_server_send_response(server, client, &response, file_data, file_size);
    if (send_result < 1)
    {
        netc_perror("http_server_send_response");
        return 1;
    };
}
else
{
    const char *headers[1][2] =
    {
        {"Content-Type", "image/png"}
    };

    FILE *file = fopen("image.png", "rb");
    if (file == NULL)
    {
        perror("fopen");
        return 1;
    };
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    struct http_response response;
    http_response_build(&response, "HTTP/1.1", 200, headers, 1);

    int send_result = http_server_send_response(server, client, &response, NULL, 0);
    if (send_result < 1)
    {
        netc_perror("http_server_send_response 1");
        return 1;
    };

    char buffer[file_size];
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, file_size, file)) > 0)
    {
        send_result = http_server_send_chunked_data(server, client, buffer, bytes_read);
        if (send_result < 1)
        {
            netc_perror("http_server_send_response_chunked 2");
            return 1;
        };
    };

    send_result = http_server_send_chunked_data(server, client, NULL, 0);
    if (send_result < 1)
    {
        netc_perror("http_server_send_response_chunked_data 3");
        return 1;
    };
};
```

### Keep Alive <a name="keep-alive-server"/>
Keep alive is a feature that allows the server to keep the underlying TCP connection open after sending a response. This allows the client to send more requests without having to reconnect. Keep alive generally is more performant.

The server by default will keep the connection alive. As of current, there is no mechanism to time out the connection. The following code snippet shows how to close the connection after sending a response.

```c
#include <stdio.h>
#include "netc/include/http/server.h"

void callback_echo(struct web_server *server, struct web_client *client, struct http_request *request)
{
    /** Adds a "Connection: close" header automatically when sending a response. Alternatively, add it manually. */
    client->server_close_flag = 1;

    char *body = http_request_get_body(request);
    
    /** Send back headers. */
    const char *headers[1][2] = 
    {
        "Content-Type", "text/plain",
        /** This is optional, as the library will automatically add this header if it is not present. If the data length is greater than 15 digits, manually fill this header in. */
        // "Content-Length", "13",
    };

    /** Send back data. */
    struct http_response response;
    http_response_build(&response, "HTTP/1.1", 200 /** status code */, headers, 1 /** length of headers */);

    int send_result = http_server_send_response(server, client, &response, body, request->body_size);
    if (send_result < 1)
    {
        netc_perror("http_server_send_response");
        return 1;
    };
};
```

## HTTP Client <a name="http-client"/>

The HTTP component of this library is purely asynchronous, and the underlying TCP mechanism will only be nonblocking. The HTTP client only supports HTTP/1.1 for now.

### Creating an HTTP Client <a name="creating-an-http-client"/>
Creating a HTTP client is a straightforward process. The following code snippet shows how to create a HTTP client that connects to 127.0.0.1:8080.

```c
#include <stdio.h>
#include "netc/include/http/client.h"

/** A client capable of sending HTTP/1.1 requests. */
struct web_client client;
client.on_http_connect = on_http_connect;
client.on_http_malformed_response = on_http_malformed_response;
client.on_http_response = on_http_response;
client.on_http_disconnect = on_http_disconnect;

struct sockaddr_in addr =
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
};

if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 0)
{
    perror("inet_pton");
    return 1;
};

int init_result = web_client_init(&client, (struct sockaddr*)&addr);
if (init_result != 0)
{
    netc_perror("web_client_init");
    return 1;
};

int start_result = web_client_start(&client);
if (start_result != 0)
{
    netc_perror("web_client_start");
    return 1;
};
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-client"/>
The HTTP client is asynchronous, which means that it will only use one thread to poll for events, and code can be executed in "event callbacks" when an event occurs on the client. The following code snippet shows how to handle events.

```c
#include <stdio.h>
#include "netc/include/http/client.h"

void on_http_connect(struct web_client* client)
{
    printf("Connected to server!\n");
};

void on_http_malformed_response(struct web_client* client, enum parse_response_error_types error)
{
    printf("Server sent malformed response: ");
    switch (error)
    {
        case RESPONSE_PARSE_ERROR_RECV: printf("the recv syscall failed."); break;
    };
    printf("\n");

    /** The client doesn't automatically close the connection -- you need to do it manually. */
    web_client_close(client, /** the next parameters are for websockets: keep them as 0 and NULL */ 0, NULL);
};

void on_http_response(struct web_client* client, struct http_response *response)
{
    printf("Server sent response!\n");

    /** The library has its own string implementation for optimizations. You need to use a getter for the data in a string_t struct. */
    printf("VERSION: %s", http_response_get_version(response));
    printf("STATUS CODE: %d", http_response_get_status_code(response));
    printf("STATUS MESSAGE: %s", http_response_get_status_message(response));

    /** response.headers is a vector. */
    for (size_t i = 0; i < response->headers.size; ++i)
    {
        struct http_header *header = vector_get(&response->headers, i);
        printf("HEADER: %s=%s", http_header_get_name(header), http_header_get_value(header));
    };

    char *body = http_response_get_body(response);
    // In the event this is binary data, printing may terminate early or cause UB
    // because the binary data may contain '\0' or no '\0' at all.
    printf("BODY:");
    for (size_t i = 0; i < response->body_size; ++i) printf("%c", body[i]);
    printf("\n");
};

void on_http_disconnect(struct web_client* client, bool is_error)
{
    printf("Disconnected from server!\n");
};
```

### Sending Requests <a name="sending-requests"/>
The HTTP client supports sending requests to servers. The following code snippet shows how to send a request.

```c
#include <stdio.h>
#include "netc/include/http/client.h"

void on_http_connect(struct web_client* client)
{
    printf("Connected to server!\n");

    /** Send back headers. */
    const char *headers[1][2] = 
    {
        "Content-Type", "text/plain",
        /** This is optional, as the library will automatically add this header if it is not present. If the data length is greater than 15 digits, manually fill this header in. */
        // "Content-Length", "13",
    };

    /** Send back data. */
    struct http_request request;
    http_request_build(&request, "GET", "/", "HTTP/1.1", headers, 1 /** length of headers */);

    int send_result = http_client_send_request(client, &request, NULL /** buffer to send */, 0 /** length of data */);
    if (send_result < 1)
    {
        netc_perror("http_client_send_request");
        return 1;
    };
};
```

### Sending Files <a name="sending-files-client"/>
The HTTP client supports sending files to servers. The following code snippet shows how to send a file.

```c
#define CHUNKED 0

if (CHUNKED == 0)
{
    const char *headers[1][2] =
    {
        {"Content-Type", "image/png"}
    };

    FILE *file = fopen("image.png", "rb");
    if (file == NULL)
    {
        perror("fopen");
        return 1;
    };

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    char file_data[file_size];
    fseek(file, 0, SEEK_SET);
    fread(file_data, 1, file_size, file);
    fclose(file);

    struct http_request request;
    http_request_build(&request, "POST", "/", "HTTP/1.1", headers, 1);

    int send_result = http_client_send_request(client, &request, file_data, file_size);
    if (send_result < 1)
    {
        netc_perror("http_client_send_request");
        return 1;
    };
}
else
{
    const char *headers[1][2] =
    {
        {"Content-Type", "image/png"}
    };

    FILE *file = fopen("image.png", "rb");
    if (file == NULL)
    {
        perror("fopen");
        return 1;
    };
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    struct http_request request;
    http_request_build(&request, "POST", "/", "HTTP/1.1", headers, 1);

    int send_result = http_client_send_request(client, &request, NULL, 0);
    if (send_result < 1)
    {
        netc_perror("http_client_send_request");
        return 1;
    };

    char buffer[file_size];
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, file_size, file)) > 0)
    {
        send_result = http_client_send_chunked_data(client, buffer, bytes_read);
        if (send_result < 1)
        {
            netc_perror("http_client_send_chunked_data");
            return 1;
        };
    };

    send_result = http_client_send_chunked_data(client, NULL, 0);
    if (send_result < 1)
    {
        netc_perror("http_client_send_chunked_data");
        return 1;
    };
};
```

### Keep Alive <a name="keep-alive-client"/>
Keep alive is a feature that allows the client to keep the underlying TCP connection open after sending a request. This allows the server to send more responses without having to reconnect. Keep alive generally is more performant.

The client by default will keep the connection alive. As of current, there is no mechanism to time out the connection. The following code snippet shows how to close the connection after sending a request.

```c
#include <stdio.h>
#include "netc/include/http/client.h"

void on_http_connect(struct web_client* client)
{
    printf("Connected to server!\n");

    /** Adds a "Connection: close" header automatically when sending a request. Alternatively, add it manually. */
    client->client_close_flag = 1;

    /** Send back headers. */
    const char *headers[1][2] = 
    {
        "Content-Type", "text/plain",
        /** This is optional, as the library will automatically add this header if it is not present. If the data length is greater than 15 digits, manually fill this header in. */
        // "Content-Length", "13",
    };

    /** Send back data. */
    struct http_request request;
    http_request_build(&request, "GET", "/", "HTTP/1.1", headers, 1 /** length of headers */);

    int send_result = http_client_send_request(client, &request, NULL /** buffer to send */, 0 /** length of data */);
    if (send_result < 1)
    {
        netc_perror("http_client_send_request");
        return 1;
    };
};
```