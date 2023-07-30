#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "tcp/client.h"

#include <stdlib.h>
#include <string.h>

/** A structure representing the HTTP client. */
struct http_client
{
    /** The TCP client. */
    struct netc_client client;

    /** The HTTP request. */
    struct http_request request;
    /** The HTTP response. */
    struct http_response response;
};

#endif // HTTP_CLIENT_H