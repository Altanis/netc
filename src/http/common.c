#include "../../include/http/common.h"
#include "../../include/utils/vector.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *http_status_code_to_message(int status_code)
{
    switch (status_code)
    {
#define X(code, message) case code: return message;
        HTTP_STATUS_CODES
#undef X
    };

    return NULL;
};

void http_request_build(struct http_request *request, const char *method, const char *path, const char *version, const char *headers[][2], size_t headers_length)
{
    sso_string_init(&request->method, method);
    sso_string_init(&request->path, path);
    sso_string_init(&request->version, version);

    vector_init(&request->headers, headers_length, sizeof(struct http_header));

    for (size_t i = 0; i < headers_length; ++i)
    {
        const char *name = headers[i][0];
        const char *value = headers[i][1];

        struct http_header header = {0};
        sso_string_init(&header.name, name);
        sso_string_init(&header.value, value);

        vector_push(&request->headers, &header);
    };
};

void http_request_free(struct http_request *request)
{
    free(request->body);

    vector_free(&request->headers);
    vector_free(&request->query);

    sso_string_free(&request->method);
    sso_string_free(&request->path);
    sso_string_free(&request->version);
};

const char *http_request_get_method(const struct http_request *request) { return sso_string_get(&request->method); };
const char *http_request_get_path(const struct http_request *request) { return sso_string_get(&request->path); };
const char *http_request_get_version(const struct http_request *request) { return sso_string_get(&request->version); };
struct http_header *http_request_get_header(const struct http_request *request, const char *name)
{
    for (size_t i = 0; i < request->headers.size; ++i)
    {
        struct http_header *header = vector_get(&request->headers, i);
        if (strcasecmp(sso_string_get(&header->name), name) == 0)
            return header;
    };

    return NULL;
};
const char *http_request_get_body(const struct http_request *request) { return request->body; };
size_t http_request_get_body_size(const struct http_request *request) { return request->body_size; };

void http_request_set_method(struct http_request *request, const char *method) { sso_string_set(&request->method, method); };
void http_request_set_path(struct http_request *request, const char *path) { sso_string_set(&request->path, path); };
void http_request_set_version(struct http_request *request, const char *version) { sso_string_set(&request->version, version); };

void http_response_build(struct http_response *response, char *version, int status_code, const char *headers[][2], size_t headers_length)
{
    sso_string_init(&response->version, version);
    response->status_code = status_code;

    const char *status_message = http_status_code_to_message(status_code);
    sso_string_init(&response->status_message, status_message);

    vector_init(&response->headers, headers_length, sizeof(struct http_header));

    for (size_t i = 0; i < headers_length; ++i)
    {
        const char *name = headers[i][0];
        const char *value = headers[i][1];

        struct http_header header = {0};
        sso_string_init(&header.name, name);
        sso_string_init(&header.value, value);

        vector_push(&response->headers, &header);
    };  
};

void http_response_free(struct http_response *response)
{
    free(response->body);

    vector_free(&response->headers);

    sso_string_free(&response->status_message);
    sso_string_free(&response->version);
};

const char *http_response_get_version(const struct http_response *response) { return sso_string_get(&response->version); };
const char *http_response_get_status_message(const struct http_response *response) { return sso_string_get(&response->status_message); };
const char *http_response_get_header(const struct http_response *response, const char *name)
{
    for (size_t i = 0; i < response->headers.size; ++i)
    {
        struct http_header *header = vector_get(&response->headers, i);
        if (strcasecmp(sso_string_get(&header->name), name) == 0)
            return sso_string_get(&header->value);
    };

    return NULL;
};
const char *http_response_get_body(const struct http_response *response) { return response->body; };

void http_response_set_version(struct http_response *response, const char *version) { sso_string_set(&response->version, version); };
void http_response_set_status_message(struct http_response *response, const char *status_message) { sso_string_set(&response->status_message, status_message); };

void http_header_init(struct http_header *header, const char *name, const char *value) { sso_string_init(&header->name, name); sso_string_init(&header->value, value); };
void http_header_free(struct http_header *header)
{
    sso_string_free(&header->name);
    sso_string_free(&header->value);
};

const char *http_header_get_name(const struct http_header *header) { return sso_string_get(&header->name); };
const char *http_header_get_value(const struct http_header *header) { return sso_string_get(&header->value); };

void http_header_set_name(struct http_header *header, const char *name) { sso_string_set(&header->name, name); };
void http_header_set_value(struct http_header *header, const char *value) { sso_string_set(&header->value, value); };

const char *http_query_get_key(const struct http_query *query) { return sso_string_get(&query->key); };
const char *http_query_get_value(const struct http_query *query) { return sso_string_get(&query->value); };

void http_url_percent_encode(char *url, char *encoded)
{
    char *encoded_ptr = encoded;

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

void http_url_percent_decode(char *url, char *decoded)
{
    char *decoded_ptr = decoded;

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

void http_base64_encode(const char *bytes, size_t bytes_len, char *encoded)
{
    static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *encoded_ptr = encoded;

    for (size_t i = 0; i < bytes_len; i += 3)
    {
        char b1 = bytes[i];
        char b2 = i + 1 < bytes_len ? bytes[i + 1] : 0;
        char b3 = i + 2 < bytes_len ? bytes[i + 2] : 0;

        unsigned char b1_1 = (b1 & 0xFC) >> 2;
        unsigned char b1_2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;
        unsigned char b2_1 = (b2 & 0x0F) << 2 | (b3 & 0xC0) >> 6;
        unsigned char b2_2 = (b3 & 0x3F);

        *encoded_ptr++ = base64_chars[b1_1];
        *encoded_ptr++ = base64_chars[b1_2];
        *encoded_ptr++ = i + 1 < bytes_len ? base64_chars[b2_1] : '=';
        *encoded_ptr++ = i + 2 < bytes_len ? base64_chars[b2_2] : '=';
    };

    *encoded_ptr = '\0';
};

void http_base64_decode(const char *encoded, char *bytes, size_t *bytes_len)
{
    static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *bytes_ptr = bytes;

    for (size_t i = 0; i < strlen(encoded); i += 4)
    {
        char b1 = strchr(base64_chars, encoded[i]) - base64_chars;
        char b2 = strchr(base64_chars, encoded[i + 1]) - base64_chars;
        char b3 = strchr(base64_chars, encoded[i + 2]) - base64_chars;
        char b4 = strchr(base64_chars, encoded[i + 3]) - base64_chars;

        char b1_1 = (b1 & 0x3F) << 2 | (b2 & 0x30) >> 4;
        char b1_2 = (b2 & 0x0F) << 4 | (b3 & 0x3C) >> 2;
        char b2_1 = (b3 & 0x03) << 6 | (b4 & 0x3F);

        *bytes_ptr++ = b1_1;
        *bytes_ptr++ = b1_2;
        *bytes_ptr++ = i + 2 < strlen(encoded) ? b2_1 : 0;
    };

    *bytes_len = bytes_ptr - bytes;
};