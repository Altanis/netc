#ifndef SERVER_H
#define SERVER_H

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

/** The maximum amount of connections permitted by the server. */
#define MAX_CONNECTIONS 10
/** The maximum bytes able to be sent through the socket (1 KB). */
#define MAX_BUFFER_SIZE 1024 
/** The port the server is connected to. */
#define PORT 8080
/** The maximum amount of sockets in the backlog. */
#define BACKLOG 3

/** A structure representing the server. */
struct netc_server_t {
    /** The socket file descriptor. */
    int socket_fd;
    /** The server's address. */
    struct sockaddr_in address;
    /** The size of the server's address. */
    int addrlen;
    /** An array of each client's file descriptor. */
    int client_fds[MAX_CONNECTIONS];
    /** The number of clients. */
    int client_count;
};

/** A structure representing the client. */
struct netc_client_t {
    /** The socket file descriptor. */
    int socket_fd;
    /** The address of the server to connect to. */
    struct sockaddr_in address;
    /** The size of the client's address. */
    int addrlen;
};

/** Initializes the server. */
struct netc_server_t* server_init();

/** Binds the server to the socket. */
int server_bind(struct netc_server_t* server);
/** Listens for connections on the server. */
int server_listen(struct netc_server_t* server);
/** Accepts a connection on the server. */
int server_accept(struct netc_server_t* server, struct netc_client_t* client);

/** Sends a message to the client. */
int server_send_message(struct netc_client_t* client, char* message, size_t msglen);
/** Receives a message from the client. */
int server_receive_message(struct netc_client_t* client, char* message);

/** Closes the server. */
int server_close_self(struct netc_server_t* server);
/** Closes a client connection. */
int server_close_client(struct netc_server_t* server, struct netc_client_t* client);

/** Sets a client to nonblocking mode. */
int client_set_non_blocking(struct netc_client_t* client);

#endif // SERVER_H