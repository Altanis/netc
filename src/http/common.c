#include "http/common.h"
#include "utils/vector.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* http_request_get_method(const struct http_request* request) { return sso_string_get(&request->method); };
const char* http_request_get_path(const struct http_request* request) { return sso_string_get(&request->path); };
const char* http_request_get_version(const struct http_request* request) { return sso_string_get(&request->version); };
const char* http_request_get_body(const struct http_request* request) { return request->body; };

void http_request_set_method(struct http_request* request, const char* method) { sso_string_set(&request->method, method); };
void http_request_set_path(struct http_request* request, const char* path) { sso_string_set(&request->path, path); };
void http_request_set_version(struct http_request* request, const char* version) { sso_string_set(&request->version, version); };

const char* http_response_get_version(const struct http_response* response) { return sso_string_get(&response->version); };
const char* http_response_get_status_message(const struct http_response* response) { return sso_string_get(&response->status_message); };
const char* http_response_get_body(const struct http_response* response) { return response->body; };

void http_response_set_version(struct http_response* response, const char* version) { sso_string_set(&response->version, version); };
void http_response_set_status_message(struct http_response* response, const char* status_message) { sso_string_set(&response->status_message, status_message); };

const char* http_header_get_name(const struct http_header* header) { return sso_string_get(&header->name); };
const char* http_header_get_value(const struct http_header* header) { return sso_string_get(&header->value); };

void http_header_set_name(struct http_header* header, const char* name) { sso_string_set(&header->name, name); };
void http_header_set_value(struct http_header* header, const char* value) { sso_string_set(&header->value, value); };

const char* http_query_get_key(const struct http_query* query) { return sso_string_get(&query->key); };
const char* http_query_get_value(const struct http_query* query) { return sso_string_get(&query->value); };

void http_query_set_key(struct http_query* query, const char* key) { sso_string_set(&query->key, key); };
void http_query_set_value(struct http_query* query, const char* value) { sso_string_set(&query->value, value); };

void http_url_percent_encode(char* url, char* encoded)
{
    char* encoded_ptr = encoded;

    for (size_t i = 0; i < strlen(url); ++i)
    {
        if (
            (url[i] >= '0' && url[i] <= '9') ||
            (url[i] >= 'a' && url[i] <= 'z') ||
            (url[i] >= 'A' && url[i] <= 'Z') ||
            url[i] == '-' || url[i] == '_' || url[i] == '.' || url[i] == '~' || url[i] == '/'
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
};

void http_url_percent_decode(char* url, char* decoded)
{
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
};