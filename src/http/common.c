#include "http/common.h"
#include "utils/vector.h"

#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* http_url_percent_encode(char* url)
{
    char* encoded = malloc(strlen(url) * 3 + 1);
    char* encoded_ptr = encoded;

    for (size_t i = 0; i < strlen(url); ++i)
    {
        if (
            (url[i] >= '0' && url[i] <= '9') ||
            (url[i] >= 'a' && url[i] <= 'z') ||
            (url[i] >= 'A' && url[i] <= 'Z') ||
            url[i] == '-' || url[i] == '_' || url[i] == '.' || url[i] == '~'
        )
        {
            *encoded_ptr++ = url[i];
        }
        else
        {
            sprintf(encoded_ptr, "%%%02X", url[i]);
            encoded_ptr += 3;
        };
    };

    *encoded_ptr = '\0';

    return encoded;
};

char* http_url_percent_decode(char* url)
{
    char* decoded = malloc(strlen(url) + 1);
    char* decoded_ptr = decoded;

    for (size_t i = 0; i < strlen(url); ++i)
    {
        if (url[i] == '%')
        {
            char hex[3] = {url[i + 1], url[i + 2], '\0'};
            *decoded_ptr++ = strtol(hex, NULL, 16);
            i += 2;
        }
        else
        {
            *decoded_ptr++ = url[i];
        };
    };

    *decoded_ptr = '\0';

    return decoded;
};