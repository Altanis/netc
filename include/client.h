#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

/** Initializes the client. */
int client_init(struct netc_client_t* client, struct netc_client_config config);

/** Connects the client to the server. */
int client_connect(struct netc_client_t* client);
/** Sends data to the server. Returns -1 if message not sent in full. */
int client_send(struct netc_client_t* client, char* message, size_t msglen);
/** Receives data from the server. Returns -1 if message not received in full. */
int client_receive(struct netc_client_t* client, char* message, size_t msglen);

/** Closes the client. */
int client_close(struct netc_client_t* client);

#endif // CLIENT_H