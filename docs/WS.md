# WebSocket Documentation

## Table of Contents
1. [WebSocket Server](#ws-server)
    1. [Creating a WebSocket Server](#creating-a-ws-server)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    3. [Sending Messages](#sending-data-server)
    4. [Pings/Pongs](#pings-pongs-server)
2. [WebSocket Client](#ws-client)
    1. [Creating a WebSocket Client](#creating-a-ws-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Sending Messages](#sending-data-client)
    4. [Pings/Pongs](#pings-pongs-client)

## WebSocket Server <a name="ws-server"/>

### Creating a WebSocket Server <a name="creating-a-ws-server"/>
Creating a WebSocket server is a straightforward process. The following code snippet shows how to create a WebSocket server that listens on port 8080 and handles incoming connections.

You need to create an [HTTP web server at first](https://github.com/Altanis/netc/blob/main/docs/HTTP.md#creating-an-http-server), then create a route which has WebSocket handlers as such:

```c
#include <stdio.h>
#include "netc/include/ws/server.h"

/** Assume a server named `ws_server` was initialised. */
struct web_server_route ws_route =
{
    .path = "/",
    .on_ws_handshake_request: ws_on_handshake_request,
    .on_ws_message: ws_on_message,
    .on_ws_malformed_frame: ws_on_malformed_frame,
    .on_ws_close: ws_on_close,
    .on_ws_heartbeat: ws_on_heartbeat,
};

web_server_add_route(&ws_server, &ws_route);
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-server"/>
The WebSocket server is asynchronous, which means that it will only use one thread to poll for events, and code can be executed in "event callbacks" when an event occurs on the server. The following code snippet shows how to handle events.

```c
#include <stdio.h>
#include "netc/include/ws/server.h"

/** Incoming HTTP request to upgrade. */
void ws_on_handshake_request(struct web_server *server, struct web_server_client *client, struct http_request *request)
{
    printf("Handshake request received.\n");

    int r = ws_server_upgrade_connection(server, client, request);
    if (r < 0) netc_perror("ws_server_upgrade_connection %d", r);
    else printf("Connection upgraded.\n");
};

/** Incoming WebSocket message (called when a frame with a FIN bit is sent). */
void ws_on_message(struct web_server *server, struct web_client *client, struct ws_message *message)
{
    printf("Message received. DETAILS:\n");
    switch (message->opcode)
    {
        /** The initial opcode from the message will be preserved.
          * Unless the initial opcode was a CONTINUE (should not happen unless client is poorly made), this won't be invoked.
        */
        // case WS_OPCODE_CONTINUE: printf("Opcode: CONTINUE\n"); break;

        case WS_OPCODE_TEXT: printf("Opcode: TEXT\n"); break;
        case WS_OPCODE_BINARY: printf("Opcode: BINARY\n"); break;

        /** This won't be run, it will be sent to the on_ws_disconnect event. */
        // case WS_OPCODE_CLOSE: printf("Opcode: CLOSE\n"); break;

        /** These won't be run, they will be sent to the on_ws_heartbeat event.
        // case WS_OPCODE_PING: printf("Opcode: PING\n"); break;
        // case WS_OPCODE_PONG: printf("Opcode: PONG\n"); break;
    };

    printf("Payload length: %lu\n", message->payload_length);
    printf("Payload:\n");
    for (size_t i = 0; i < message->payload_length; i++)
    {
        printf("%c", message->buffer[i]);
    };
};

/** Incoming malformed WebSocket frame. */
void ws_on_malformed_frame(struct web_server *server, struct web_client *client, enum ws_frame_parsing_errors error)
{
    printf("Malformed frame received.\n");
    switch (error)
    {
        /** The recv syscall failed. */
        case WS_FRAME_PARSE_ERROR_RECV: printf("Error: RECV\n"); break;
        /** The payload length for the frame is invalid. */
        case WS_FRAME_PARSE_ERROR_INVALID_FRAME_LENGTH: printf("Error: PAYLOAD_LENGTH\n"); break;
        /** The payload length for the frame is too large. */
        case WS_FRAME_PARSE_ERROR_PAYLOAD_TOO_BIG: printf("Error: PAYLOAD_LENGTH_TOO_LARGE\n"); break;
    };

    /** Send any messages now, but the connection will be closed after this callback with error "1002 Malformed Frame". */
};

/** Incoming WebSocket close frame. */
void ws_on_close(struct web_server *server, struct web_client *client, uint16_t code, char *reason)
{
    printf("Close frame received. Code: %d, Reason: %s\n", code, reason);
};
```

### Sending Messages <a name="sending-data-server"/>
Sending messages is a straightforward process. The following code snippet shows how to send a message to a client.

```c
#include <stdio.h>
#include "netc/include/ws/server.h"

/** Assume a server named `ws_server` was initialised. */
struct ws_message message;
ws_build_message(
    &message,
    WS_OPCODE_TEXT, /** opcode */
    2, /** payload length */
    "hi" /** payload */
);

int r = ws_send_message(
    client, /** client to send to */
    &message /** message to send */
    NULL, /** masking keys cannot be sent from the server */
    1, /** the number of frames to split the message into (set to 1 ideally) */
);

if (r < 0) netc_perror("ws_send_message %d", r);
else printf("Message sent.\n");
```

### Pings/Pongs <a name="pings-pongs-server"/>
The server automatically replies to a ping with a pong. The server is also capable of sending pings to clients and replying to pongs with pings, creating a steady heartbeat system capable of recording latency. To enable this, you can do this as such:

```c
#include <stdio.h>
#include "netc/include/ws/server.h"

static struct timespec start_time;
static double latency = 0;

struct web_server server;
server.ws_server_config.record_latency = true; // Replies to pongs with pings, and sends an initial PING to a client.

/** Assume a server route was initialised which listens to on_ws_heartbeat with this function. */
void ws_on_heartbeat(struct web_server *server, struct web_client *client, struct ws_message *message)
{
    /** This message may have payload data, you can handle it. */
    if (message->opcode == WS_OPCODE_PONG)
    {
        /** This runs every time a client sends back a pong. */
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        latency = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        latency += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

        printf("Latency: %fms\n", latency);

        start_time = end_time;
    };

    /** After this callback runs, a ping will be sent to the client (if record_latency is true). */
};
```

## WebSocket Client <a name="ws-client"/>

### Creating a WebSocket Client <a name="creating-a-ws-client"/>
Creating a WebSocket client is a straightforward process. The following code snippet shows how to create a WebSocket client that connects to a server.

```c
struct web_client client;
client.on_http_connect = ws_on_http_connect;
client.on_ws_connect = ws_on_ws_connect;
client.on_ws_message = ws_on_ws_message;
client.on_ws_malformed_frame = ws_on_ws_malformed_frame;
client.on_ws_disconnect = ws_on_ws_close;
client.on_ws_heartbeat = ws_on_ws_heartbeat;

struct sockaddr_in addr =
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
};

if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0)
{
    netc_perror("inet_pton");
    return -1;
};

int r = web_client_init(&client, (struct sockaddr *) &addr);
if (r < 0) netc_perror("web_client_init %d", r);

r = web_client_start(&client);
if (r < 0) netc_perror("web_client_start %d", r);
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-client"/>
The WebSocket client is asynchronous, which means that it will only use one thread to poll for events, and code can be executed in "event callbacks" when an event occurs on the client. The following code snippet shows how to handle events.

```c
#include <stdio.h>
#include "netc/include/ws/client.h"

/** Incoming HTTP response to upgrade. */
void ws_on_http_connect(struct web_client *client, struct http_response *response)
{
    printf("HTTP connection established.\n");

    int r = ws_client_connect(client, "localhost:8080", "/ws");
    if (r != 1) netc_perror("ws_client_connect %d", r);
    else printf("WebSocket connection established.\n");
};

/** Incoming WebSocket connection. */
void ws_on_ws_connect(struct web_client *client)
{
    printf("WebSocket connection established.\n");
};

/** Incoming WebSocket message (called when a frame with a FIN bit is sent). */
void ws_on_ws_message(struct web_client *client, struct ws_message *message)
{
    printf("Message received. DETAILS:\n");
    switch (message->opcode)
    {
        /** The initial opcode from the message will be preserved.
          * Unless the initial opcode was a CONTINUE (should not happen unless client is poorly made), this won't be invoked.
        */
        // case WS_OPCODE_CONTINUE: printf("Opcode: CONTINUE\n"); break;

        case WS_OPCODE_TEXT: printf("Opcode: TEXT\n"); break;
        case WS_OPCODE_BINARY: printf("Opcode: BINARY\n"); break;

        /** This won't be run, it will be sent to the on_ws_disconnect event. */
        // case WS_OPCODE_CLOSE: printf("Opcode: CLOSE\n"); break;

        /** These won't be run, they will be sent to the on_ws_heartbeat event.
        // case WS_OPCODE_PING: printf("Opcode: PING\n"); break;
        // case WS_OPCODE_PONG: printf("Opcode: PONG\n"); break;
    };

    printf("Payload length: %lu\n", message->payload_length);
    printf("Payload:\n");
    for (size_t i = 0; i < message->payload_length; i++)
    {
        printf("%c", message->buffer[i]);
    };
};

/** Incoming malformed WebSocket frame. */
void ws_on_ws_malformed_frame(struct web_client *client, enum ws_frame_parsing_errors error)
{
    printf("Malformed frame received.\n");
    switch (error)
    {
        /** The recv syscall failed. */
        case WS_FRAME_PARSE_ERROR_RECV: printf("Error: RECV\n"); break;
    };
};

/** Incoming WebSocket close frame. */
void ws_on_ws_close(struct web_client *client, uint16_t code, char *reason)
{
    printf("Close frame received. Code: %d, Reason: %s\n", code, reason);
};
```

### Sending Messages <a name="sending-data-client"/>
Sending messages is a straightforward process. The following code snippet shows how to send a message to a server.

```c
#include <stdio.h>
#include "netc/include/ws/client.h"

uint masking_key[4];
ws_build_masking_key(masking_key); /** Generate a masking key. */

/** Assume a client named `client` was initialised. */
struct ws_message message;
ws_build_message(
    &message,
    WS_OPCODE_TEXT, /** opcode */
    2, /** payload length */
    "hi" /** payload */
);

int r = ws_send_message(
    client, /** client to send from */
    &message /** message to send */
    masking_key, /** masking key (optional, set to NULL if none) */
    1, /** the number of frames to split the message into (set to 1 ideally) */
);

if (r < 0) netc_perror("ws_send_message %d", r);
else printf("Message sent.\n");
```

### Pings/Pongs <a name="pings-pongs-client"/>
The client automatically replies to a ping with a pong. The client is also capable of sending pings to servers and replying to pongs with pings, creating a steady heartbeat system capable of recording latency. To enable this, you can do this as such:

```c
#include <stdio.h>
#include "netc/include/ws/client.h"

static struct timespec start_time;
static double latency = 0;

struct web_client client;
client.ws_client_config.record_latency = true; // Replies to pongs with pings, and sends an initial PING to a server.

/** Assume a client was initialised which listens to on_ws_heartbeat with this function. */
void ws_on_ws_heartbeat(struct web_client *client, struct ws_message *message)
{
    /** This message may have payload data, you can handle it. */
    if (message->opcode == WS_OPCODE_PONG)
    {
        /** This runs every time a server sends back a pong. */
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        latency = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        latency += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

        printf("Latency: %fms\n", latency);

        start_time = end_time;
    };

    /** After this callback runs, a ping will be sent to the server (if record_latency is true). */
};
```
