#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

/** Initializes the client. */
struct netc_client_t* client_init();

/** Connects the client to the server. */
int client_connect(struct netc_client_t* client);
/** Sends a message to the server. */
int client_send_message(struct netc_client_t* client, char* message, size_t msglen);
/** Receives a message from the server. */
int client_receive_message(struct netc_client_t* client, char* message, size_t msglen);

/** Closes the client. */
int client_close(struct netc_client_t* client);

#endif // CLIENT_H