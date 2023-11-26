# UDP Documentation

## Table of Contents
1. [UDP Server](#udp-server)
    1. [Creating a UDP Server](#creating-a-udp-server)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    3. [Handling Blocking Mechanism](#handling-blocking-mechanism-server)
2. [UDP Client](#udp-client)
    1. [Creating a UDP Client](#creating-a-udp-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Handling Blocking Mechanism](#handling-blocking-mechanism-client)

## UDP Server <a name="udp-server"/>

### Creating a UDP Server <a name="creating-a-udp-server"/>
Creating a UDP server is a straightforward process. The following code snippet shows how to create a UDP server that listens on port 8080 and handles incoming connections.

```c
#include <stdio.h>
#include "netc/include/udp/server.h"

struct udp_server server = {0};
/** If you plan to use asynchronous events, you need to set the event callbacks. */
server.on_data = on_data;

server.data = NULL; /** This data will be passed to the event callbacks if ever needed. */

struct sockaddr_in sockaddr = 
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
    .sin_addr.s_addr = INADDR_ANY
};

/** Create the UDP server. */
int init_result = udp_server_init(&server, *(struct sockaddr *)&sockaddr, 1 /** use non-blocking mode or not */);
if (init_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Bind the UDP server to the address. */
int bind_result = udp_server_bind(&server);
if (bind_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Start the event loop. */
int r = udp_server_main_loop(&server); /** This function will block. */
if (r != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-server"/>
If you plan to use asynchronous events, you need to set the event callbacks. The following code snippet shows how the event callbacks work.

```c
#include <stdio.h>
#include <stdint.h>
#include "netc/include/udp/server.h"

void on_data(struct udp_server *server)
{
    /** The packet structure is very lackluster and doesn't include sequence numbers, payload lengths, checksums, etc. */
    char buffer[1024];
    struct sockaddr addr = {0};
    socklen_t addr_len = sizeof(addr);

    /** Receive the data. */
    int r = udp_server_receive(server, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, &addr_len);
    if (r == -1) 
    {
        /** Handle error. */
        netc_perror("failure");
        return 1;
    }
    else
    {
        /** Send the data back. */
        r = udp_server_send(server, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, addr_len);
        if (r == -1) 
        {
            /** Handle error. */
            netc_perror("failure");
            return 1;
        }
        else
        {
            /** Print the data. */
            printf("Sent data: %s\n", buffer);
        }
    };
};
```

### Handling Blocking Mechanism <a name="handling-blocking-mechanism-server"/>

Use the functions as normal. They will block.

```c
#include <stdio.h>
#include "netc/include/udp/server.h"

// assume a blocking server is already set up as `server`
char buffer[1024];
struct sockaddr addr = {0};
socklen_t addrlen = sizeof(addr);

/** Receive the data. */
int r = udp_server_receive(server, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, &addrlen); // This will block until data is received.
if (r == -1) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Send the data. */
r = udp_server_send(server, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, addrlen); // This will block until the data is sent.
```

## UDP Client <a name="udp-client"/>

### Creating a UDP Client <a name="creating-a-udp-client"/>
Creating a UDP client is a straightforward process. The following code snippet shows how to create a UDP client that connects to localhost:8080.

```c
#include <stdio.h>
#include <string.h>
#include "netc/include/udp/client.h"

struct udp_client client = {0};
/** If you plan to use asynchronous events, you need to set the event callbacks. */
client.on_data = on_data;

struct sockaddr_in sockaddr = 
{
    .sin_family = AF_INET,
    .sin_port = htons(8080),
};

if (inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr) != 1) printf("failed to convert address.\n"); /** Handle error. */

/** Create the UDP client. */
int init_result = udp_client_init(&client, 0 *(struct sockaddr *)&sockaddr, 1 /** use non-blocking mode or not */);
if (init_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Connect to the server. */
int connect_result = udp_client_connect(&client);
if (connect_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}
```

### Handling Asynchronous Events <a name="handling-asynchronous-events-client"/>

If you plan to use asynchronous events, you need to set the event callbacks. The following code snippet shows how the event callbacks work.

```c
#include <stdio.h>
#include <stdint.h>
#include "netc/include/udp/client.h"

void on_data(struct udp_client *client)
{
    /** The packet structure is very lackluster and doesn't include sequence numbers, payload lengths, checksums, etc. */
    char buffer[1024];
    struct sockaddr addr = {0};
    socklen_t addr_len = sizeof(addr);

    /** Receive the data. */
    int r = udp_client_receive(client, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, &addr_len);
    if (r == -1) 
    {
        /** Handle error. */
        netc_perror("failure");
        return 1;
    }
    else
    {
        /** Send the data back. */
        r = udp_client_send(client, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, addr_len);
        if (r == -1) 
        {
            /** Handle error. */
            netc_perror("failure");
            return 1;
        }
        else
        {
            /** Print the data. */
            printf("Sent data: %s\n", buffer);
        }
    };
};
```

### Handling Blocking Mechanism <a name="handling-blocking-mechanism-client"/>

Use the functions as normal. They will block.

```c
#include <stdio.h>
#include "netc/include/udp/client.h"

// assume a blocking client is already set up as `client`
char buffer[1024];
struct sockaddr addr = {0};
socklen_t addrlen = sizeof(addr);

/** Receive the data. */
int r = udp_client_receive(client, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, &addrlen); // This will block until data is received.

/** Send the data. */
r = udp_client_send(client, buffer, 1024 /** sizeof buffer */, 0 /** flags */, &addr, addrlen); // This will block until the data is sent.
```