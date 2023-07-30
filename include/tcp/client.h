#ifndef CLIENT_H
#define CLIENT_H

#include "tcp/server.h"

extern __thread int netc_client_listening;

/** Initializes the client. */
int client_init(struct netc_client* client, struct netc_client_config config);
/** The main loop of the client. */
int client_main_loop(struct netc_client* client);

/** Connects the client to the server. */
int client_connect(struct netc_client* client);
/** Sends data to the server. Returns -1 if message not sent in full. */
int client_send(struct netc_client* client, char* message, size_t msglen);
/** Receives data from the server. Returns -1 if message not received in full. */
int client_receive(struct netc_client* client, char* message, size_t msglen);

/** Closes the client. */
int client_close(struct netc_client* client, int is_error);

#endif // CLIENT_H