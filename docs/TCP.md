# TCP Documentation

## Table of Contents
1. [TCP Server](#tcp-server)
    1. [Creating a TCP Server](#creating-a-tcp-server)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-server)
    3. [Handling Blocking Mechanism](#handling-blocking-mechanism-server)
2. [TCP Client](#tcp-client)
    1. [Creating a TCP Client](#creating-a-tcp-client)
    2. [Handling Asynchronous Events](#handling-asynchronous-events-client)
    3. [Handling Blocking Mechanism](#handling-blocking-mechanism-client)

## TCP Server <a name="tcp-server"/>

### Creating a TCP Server <a name="creating-a-tcp-server"/>
Creating a TCP server is a straightforward process. The following code snippet shows how to create a TCP server that listens on port 8923 and handles incoming connections.

```c
#include <stdio.h>
#include "netc/include/tcp/server.h"

struct tcp_server server = {0};
/** If you plan to use asynchronous events, you need to set the event callbacks. */
server.on_connect = on_connect;
server.on_data = on_data;
server.on_disconnect = on_disconnect;

server.data = NULL; /** This data will be passed to the event callbacks if ever needed. */

struct sockaddr_in sockaddr = 
{
    .sin_family = AF_INET,
    .sin_port = htons(8923),
    .sin_addr.s_addr = INADDR_ANY
};

/** Create the TCP server. */
int init_result = tcp_server_init(&server, (struct sockaddr *)&sockaddr, 1 /** use non-blocking mode or not */);
if (init_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Bind the TCP server to the address. */
int bind_result = tcp_server_bind(&server);
if (bind_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Start listening for incoming connections. */
int listen_result = tcp_server_listen(&server, 10 /** backlog */);
if (listen_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Start the event loop. */
int r = tcp_server_main_loop(&server); /** This function will block. */
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
#include "netc/include/tcp/server.h"

/** The server does not track current connected clients. You need to do it yourself. */
/** You can use the map or vector structs provided by the library. */

void on_connect(struct tcp_server *server)
{
    struct tcp_client client = {0};
    int r = tcp_server_accept(server, &client);
    if (r != 0) 
    {
        /** Handle error. */
        netc_perror("failure");
        return 1;
    }

    /** Do something with the client. */
    printf("Client connected. Their sockfd: %d\n", client.sockfd);
};

void on_data(struct tcp_server *server, socket_t sockfd)
{
    /** An example of an efficient protocol would be 4 bytes for the size of the data and then the data itself. */
    char data_size[4] = {0};
    int r = tcp_server_receive(sockfd, data_size, 4 /** how much you want to recv */, 0 /** flags */);

    uint32_t recv_amt = 0;

    if (r == -1) 
    {
        /** Handle error. */
        netc_perror("failure");
        return 1;
    }
    else
    {
        recv_amt = *(uint32_t *)data_size;
        char buffer[recv_amt];
        r = tcp_server_receive(sockfd, buffer, recv_amt, 0 /** flags */);
        if (r == -1) 
        {
            /** Handle error. */
            netc_perror("failure");
            return 1;
        }
        else
        {
            /** Do something with the data. */
            printf("Received data from client: %s\n", buffer);
        }
    };
};

void on_disconnect(struct tcp_server *server, socket_t sockfd, bool is_error)
{
    /** Do something with the client. */
    printf("Client disconnected. Their sockfd: %d\n", sockfd);
};

// Assign these functions to the server structs.
server.on_connect = on_connect;
server.on_data = on_data;
server.on_disconnect = on_disconnect;
```

### Handling Blocking Mechanisms <a name="handling-blocking-mechanism-server"/>

Use the functions as normal. They will block.

```c
#include <stdio.h>
#include "netc/include/tcp/server.h"

// assume a blocking server is already set up as `server`.
struct tcp_client client = {0};

// this will block until an incoming connection is received.
tcp_server_accept(&server, &client);
printf("Client connected. Their sockfd: %d\n", client.sockfd);

char buffer[1024] = {0};
// this will block until data is received.
tcp_server_receive(client.sockfd, buffer, 1024 /** amount of data to receive */, 0 /** flags */);

printf("Received data from client: %s\n", buffer);
```

A common approach to handling blocking mechanisms is to use threads for each client.

## TCP Client <a name="tcp-client"/>

### Creating a TCP Client <a name="creating-a-tcp-client"/>
Creating a TCP client is a straightforward process. The following code snippet shows how to create a TCP client that connects to localhost:8923.

```c
#include <stdio.h>
#include "netc/include/tcp/client.h"

struct tcp_client client = {0};
/** If you plan to use asynchronous events, you need to set the event callbacks. */
client.on_connect = on_connect;
client.on_data = on_data;
client.on_disconnect = on_disconnect;

client.data = NULL; /** This data will be passed to the event callbacks if ever needed. */

struct sockaddr_in sockaddr = 
{
    .sin_family = AF_INET,
    .sin_port = htons(8923),
};

if (inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr) != 1) printf("failed to convert address.\n"); /** Handle error. */

/** Create the TCP client. */
int init_result = tcp_client_init(&client, *(struct sockaddr *)&sockaddr, 1 /** use non-blocking mode or not */);
if (init_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Connect to the server. */
int connect_result = tcp_client_connect(&client);
if (connect_result != 0) 
{
    /** Handle error. */
    netc_perror("failure");
    return 1;
}

/** Start the event loop. */
int r = tcp_client_main_loop(&client); /** This function will block. */
if (r != 0) 
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
#include "netc/include/tcp/client.h"

void on_connect(struct tcp_client *client)
{
    /** Do something with the client. */
    printf("Connected to server.\n");
};

void on_data(struct tcp_client *client)
{
    /** An example of an efficient protocol would be 4 bytes for the size of the data and then the data itself. */
    char data_size[4] = {0};
    int r = tcp_client_receive(client, data_size, 4 /** how much you want to recv */, 0 /** flags */);

    uint32_t recv_amt = 0;

    if (r == -1) 
    {
        /** Handle error. */
        netc_perror("failure");
        return 1;
    }
    else
    {
        recv_amt = *(uint32_t *)data_size;
        char buffer[recv_amt];
        r = tcp_client_receive(client, buffer, recv_amt, 0 /** flags */);
        if (r == -1) 
        {
            /** Handle error. */
            netc_perror("failure");
            return 1;
        }
        else
        {
            /** Do something with the data. */
            printf("Received data from server: %s\n", buffer);
        }
    };
};

void on_disconnect(struct tcp_client *client, bool is_error)
{
    /** Do something with the client. */
    printf("Disconnected from server.\n");
};

// Assign these functions to the client structs.
client.on_connect = on_connect;
client.on_data = on_data;
client.on_disconnect = on_disconnect;
```

### Handling Blocking Mechanisms <a name="handling-blocking-mechanism-client"/>

Use the functions as normal. They will block.

```c
#include <stdio.h>
#include "netc/include/tcp/client.h"

// assume a blocking client is already set up as `client`.

// this will block until a connection is established.
tcp_client_connect(&client, *(struct sockaddr *)&sockaddr, sizeof(sockaddr));

char buffer[1024] = {0};
// this will block until data is received.
tcp_client_receive(client, buffer, 1024 /** amount of data to receive */, 0 /** flags */);

printf("Received data from server: %s\n", buffer);
```

A common approach to handling blocking mechanisms is to use threads for each client.