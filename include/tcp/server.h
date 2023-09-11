#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "utils/vector.h"
#include "socket.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

/** A structure representing a TCP client. */
struct tcp_client
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The address of the server to connect to. */
    struct sockaddr sockaddr;

#ifdef _WIN32
    /** The polling file descriptor. */
    HANDLE pfd;
#else
    /** The polling file descriptor. */
    int pfd;
#endif 

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** The callback for when the client has connected to the server. */
    void (*on_connect)(struct tcp_client* client, void* data);
    /** The callback for when the client has received a message from the server. */
    void (*on_data)(struct tcp_client* client, void* data);
    /** The callback for when the client has disconnected from the server. */
    void (*on_disconnect)(struct tcp_client* client, int is_error, void* data);
};

/** A structure representing a TCP server. */
struct tcp_server
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The server's address. */
    struct sockaddr address;
    
    /** Whether or not the socket is nonblocking. */
    int non_blocking;

    /** The number of clients connected to the server. */
    size_t client_count;

#ifdef _WIN32
    /** The polling file descriptor. */
    HANDLE pfd;
#else
    /** The polling file descriptor. */
    int pfd;
#endif 

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** The callback for when an incoming connection occurs. */
    void (*on_connect)(struct tcp_server* server, void* data);
    /** The callback for when a message is received from a client. */
    void (*on_data)(struct tcp_server* server, socket_t sockfd, void* data);
    /** The callback for when a client socket disconnects. */
    void (*on_disconnect)(struct tcp_server* server, socket_t sockfd, int is_error, void* data);
};

/** Whether or not the server is listening for events. */
extern __thread int netc_tcp_server_listening;

/** The main loop of a nonblocking TCP server. */
int tcp_server_main_loop(struct tcp_server* server);

/** Initializes a TCP server. */
int tcp_server_init(struct tcp_server* server, struct sockaddr address, int non_blocking);
/** Binds a TCP server to an address. */
int tcp_server_bind(struct tcp_server* server);
/** Starts listening for connections on a TCP server. */
int tcp_server_listen(struct tcp_server* server, int backlog);
/** Accepts a connection on the TCP server. */
int tcp_server_accept(struct tcp_server* server, struct tcp_client* client);

/** Sends a message to the client. Returns the result of the `send` syscall. */
int tcp_server_send(socket_t sockfd, char* message, size_t msglen, int flags);
/** Receives a message from the client. Returns the result of the `recv` syscall. */
int tcp_server_receive(socket_t sockfd, char* message, size_t msglen, int flags);

/** Closes the TCP server. */
int tcp_server_close_self(struct tcp_server* server);
/** Closes a client connection. */
int tcp_server_close_client(struct tcp_server* server, socket_t sockfd, int is_error);

#endif // TCP_SERVER_H