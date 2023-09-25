#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H

#include "utils/vector.h"
#include "utils/string.h"

#include <stdint.h>

/** Parses a component of a request with state management and error handling. */
#define NETC_HTTP_REQUEST_PARSE(current_state, required_state, sockfd, string, bytes, remove_delimiter, max_bytes_received, is_fixed) \
{ \
    switch (current_state) \
    { \
        case -1: current_state = REQUEST_PARSING_STATE_METHOD; break; \
        case REQUEST_PARSING_STATE_METHOD: current_state = REQUEST_PARSING_STATE_PATH; break; \
        case REQUEST_PARSING_STATE_PATH: current_state = REQUEST_PARSING_STATE_VERSION; break; \
        case REQUEST_PARSING_STATE_VERSION: current_state = REQUEST_PARSING_STATE_HEADER_NAME; break; \
        case REQUEST_PARSING_STATE_HEADER_NAME: current_state = REQUEST_PARSING_STATE_HEADER_VALUE; break; \
        case REQUEST_PARSING_STATE_HEADER_VALUE: current_state = REQUEST_PARSING_STATE_HEADER_NAME; break; \
        case REQUEST_PARSING_STATE_CHUNK_SIZE: current_state = REQUEST_PARSING_STATE_CHUNK_DATA; break; \
        case REQUEST_PARSING_STATE_CHUNK_DATA: current_state = REQUEST_PARSING_STATE_CHUNK_SIZE; break; \
        case REQUEST_PARSING_STATE_BODY: break; \
        default: break; \
    } \
    \
    \
    if (current_state == required_state) \
    { \
        ssize_t result = 0; \
        if (is_fixed) \
            result = socket_recv_until_fixed((sockfd), (string), (max_bytes_received), (bytes), (remove_delimiter)); \
        else \
            result = socket_recv_until_dynamic((sockfd), (string), (bytes), (remove_delimiter), (max_bytes_received)); \
        if (result == -1) { printf("[CHECK RECV RESULT serv] recv failed... erno: %d\n", errno); return REQUEST_PARSE_ERROR_RECV; } \
        else if (result == 0) continue; \
    } \
};

/** Parses a component of a response with state management and error handling. */
#define NETC_HTTP_RESPONSE_PARSE(current_state, required_state, sockfd, string, bytes, remove_delimiter, max_bytes_received, is_fixed) \
{ \
    switch (current_state) \
    { \
        case -1: current_state = RESPONSE_PARSING_STATE_VERSION; break; \
        case RESPONSE_PARSING_STATE_VERSION: current_state = RESPONSE_PARSING_STATE_STATUS_CODE; break; \
        case RESPONSE_PARSING_STATE_STATUS_CODE: current_state = RESPONSE_PARSING_STATE_STATUS_MESSAGE; break; \
        case RESPONSE_PARSING_STATE_STATUS_MESSAGE: current_state = RESPONSE_PARSING_STATE_HEADER_NAME; break; \
        case RESPONSE_PARSING_STATE_HEADER_NAME: current_state = RESPONSE_PARSING_STATE_HEADER_VALUE; break; \
        case RESPONSE_PARSING_STATE_HEADER_VALUE: current_state = RESPONSE_PARSING_STATE_HEADER_NAME; break; \
        case RESPONSE_PARSING_STATE_CHUNK_SIZE: current_state = RESPONSE_PARSING_STATE_CHUNK_DATA; break; \
        case RESPONSE_PARSING_STATE_CHUNK_DATA: current_state = RESPONSE_PARSING_STATE_CHUNK_SIZE; break; \
        case RESPONSE_PARSING_STATE_BODY: break; \
        default: break; \
    } \
    \
    \
    if (current_state == required_state) \
    { \
        ssize_t result = 0; \
        if (is_fixed) \
            result = socket_recv_until_fixed((sockfd), (string), (max_bytes_received), (bytes), (remove_delimiter)); \
        else \
            result = socket_recv_until_dynamic((sockfd), (string), (bytes), (remove_delimiter), (max_bytes_received)); \
        if (result == -1) { printf("[CHECK RECV RESULT clin] recv failed... erno: %d\n", errno); return RESPONSE_PARSE_ERROR_RECV; } \
        else if (result == 0) continue; \
    } \
};

#define HTTP_STATUS_CODES \
    X(100, "Continue") \
    X(101, "Switching Protocols") \
    X(102, "Processing") \
    X(103, "Early Hints") \
    X(200, "OK") \
    X(201, "Created") \
    X(202, "Accepted") \
    X(203, "Non-Authoritative Information") \
    X(204, "No Content") \
    X(205, "Reset Content") \
    X(206, "Partial Content") \
    X(207, "Multi-Status") \
    X(208, "Already Reported") \
    X(226, "IM Used") \
    X(300, "Multiple Choices") \
    X(301, "Moved Permanently") \
    X(302, "Found") \
    X(303, "See Other") \
    X(304, "Not Modified") \
    X(305, "Use Proxy") \
    X(306, "Switch Proxy") \
    X(307, "Temporary Redirect") \
    X(308, "Permanent Redirect") \
    X(400, "Bad Request") \
    X(401, "Unauthorized") \
    X(402, "Payment Required") \
    X(403, "Forbidden") \
    X(404, "Not Found") \
    X(405, "Method Not Allowed") \
    X(406, "Not Acceptable") \
    X(407, "Proxy Authentication Required") \
    X(408, "Request Timeout") \
    X(409, "Conflict") \
    X(410, "Gone") \
    X(411, "Length Required") \
    X(412, "Precondition Failed") \
    X(413, "Payload Too Large") \
    X(414, "URI Too Long") \
    X(415, "Unsupported Media Type") \
    X(416, "Range Not Satisfiable") \
    X(417, "Expectation Failed") \
    X(418, "I'm a teapot") \
    X(421, "Misdirected Request") \
    X(422, "Unprocessable Entity") \
    X(423, "Locked") \
    X(424, "Failed Dependency") \
    X(425, "Too Early") \
    X(426, "Upgrade Required") \
    X(428, "Precondition Required") \
    X(429, "Too Many Requests") \
    X(431, "Request Header Fields Too Large") \
    X(451, "Unavailable For Legal Reasons") \
    X(500, "Internal Server Error") \
    X(501, "Not Implemented") \
    X(502, "Bad Gateway") \
    X(503, "Service Unavailable") \
    X(504, "Gateway Timeout") \
    X(505, "HTTP Version Not Supported") \
    X(506, "Variant Also Negotiates") \
    X(507, "Insufficient Storage") \
    X(508, "Loop Detected") \
    X(510, "Not Extended") \
    X(511, "Network Authentication Required")

#define X(code, message) HTTP_STATUS_CODE_##code,
/** The HTTP status codes. */
enum http_status_code
{
    HTTP_STATUS_CODES
};
#undef X

#define X(code, message) message,
/** The HTTP status messages. */
static const char *http_status_messages[] =
{
    HTTP_STATUS_CODES
};
#undef X

/** An enum representing the different states during parsing a request. */
enum http_request_parsing_states
{
    // REQUEST LINE
    /** The method is being parsed. */
    REQUEST_PARSING_STATE_METHOD,
    /** The path is being parsed. */
    REQUEST_PARSING_STATE_PATH,
    /** The version is being parsed. */
    REQUEST_PARSING_STATE_VERSION,

    // HEADERS
    /** The header name is being parsed. */
    REQUEST_PARSING_STATE_HEADER_NAME,
    /** The header value is being parsed. */
    REQUEST_PARSING_STATE_HEADER_VALUE,

    // BODY (CHUNKED)
    /** The chunk size is being parsed. */
    REQUEST_PARSING_STATE_CHUNK_SIZE,
    /** The chunk data is being parsed. */
    REQUEST_PARSING_STATE_CHUNK_DATA,

    // BODY (NOT CHUNKED)
    /** The body is being parsed. */
    REQUEST_PARSING_STATE_BODY,
};

/** An enum representing the different states during parsing a response. */
enum http_response_parsing_states
{
    // RESPONSE LINE
    /** The version is being parsed. */
    RESPONSE_PARSING_STATE_VERSION,
    /** The status code is being parsed. */
    RESPONSE_PARSING_STATE_STATUS_CODE,
    /** The status message is being parsed. */
    RESPONSE_PARSING_STATE_STATUS_MESSAGE,

    // HEADERS
    /** The header name is being parsed. */
    RESPONSE_PARSING_STATE_HEADER_NAME,
    /** The header value is being parsed. */
    RESPONSE_PARSING_STATE_HEADER_VALUE,

    // BODY (CHUNKED)
    /** The chunk size is being parsed. */
    RESPONSE_PARSING_STATE_CHUNK_SIZE,
    /** The chunk data is being parsed. */
    RESPONSE_PARSING_STATE_CHUNK_DATA,

    // BODY (NOT CHUNKED)
    /** The body is being parsed. */
    RESPONSE_PARSING_STATE_BODY,
};

/** An enum representing the failure codes for `http_server_parse_request()`. */
enum parse_request_error_types
{
    /** The `recv` syscall failed. */
    REQUEST_PARSE_ERROR_RECV = -1,
    /** The body was too big. */
    REQUEST_PARSE_ERROR_BODY_TOO_BIG = -2,
    /** The request contained too many headers. */
    REQUEST_PARSE_ERROR_TOO_MANY_HEADERS = -2,
    /** The request took too long to process. */
    REQUEST_PARSE_ERROR_TIMEOUT = -3,
};

/** An enum representing the failure codes for `http_client_parse_response()`. */
enum parse_response_error_types
{
    /** The `recv` syscall failed. */
    RESPONSE_PARSE_ERROR_RECV = -1,
};

/** A structure representing the HTTP request. */
struct http_request
{
    /** The HTTP method. */
    string_t method;
    /** The HTTP path the request was sent to. */
    string_t path;
    /** The HTTP version. */
    string_t version;

    /** The parsed query in the path. */
    struct vector query; // <http_query>
    /** The HTTP headers. */
    struct vector headers; // <http_header>

    /** The HTTP body. */
    char *body;
    /** The size of the HTTP body. */
    size_t body_size;
};

/** A structure representing the HTTP response. */
struct http_response
{
    /** The HTTP version. */
    string_t version;
    /** The HTTP status code. */
    int status_code;
    /** The HTTP status message. */
    string_t status_message;

    /** The HTTP headers. */
    struct vector headers; // <http_header>

    /** The HTTP body. */
    char *body;
    /** The size of the HTTP body. */
    size_t body_size;
};

/** A structure representing the HTTP header. */
struct http_header
{
    /** The header's name. */
    string_t name;
    /** The header's value. */
    string_t value;
};

/** A structure representing a query key-value pair. */
struct http_query
{
    /** The query's key. */
    string_t key;
    /** The query's value. */
    string_t value;
};

/** A struct representing the current state of parsing a HTTP request. */
struct http_server_parsing_state
{
    /** The current HTTP request. */
    struct http_request request;
    /** The current state of parsing. */
    enum http_request_parsing_states parsing_state;

    /** The content length of the request. < 0 means chunked. */
    int content_length;

    /** Whether or not parsing the terminating CRLF is done. */
    int parsed_crlf;
    /** The current HTTP header being populated (if any). */
    struct http_header header;
    /** The current chunk length being populated (if any). */
    char chunk_length[18];
    /** The current (parsed) chunk size. */
    size_t chunk_size;
    /** The size of the (incomplete) chunk data. */
    size_t incomplete_chunk_data_size;
};

/** A struct representing the current state of parsing a HTTP response. */
struct http_client_parsing_state
{
    /** The current HTTP response. */
    struct http_response response;
    /** The current state of parsing. */
    enum http_response_parsing_states parsing_state;

    /** The content length of the request. < 0 means chunked. */
    int content_length;

    /** Whether or not parsing the terminating CRLF is done. */
    int parsed_crlf;
    /** The current HTTP header being populated (if any). */
    struct http_header header;
    /** The current chunk length being populated (if any). */
    char chunk_length[18];
    /** The current (parsed) chunk size. */
    size_t chunk_size;
    /** The dynamically sized buffer for chunked data. */
    struct vector chunk_data;
};

/** Gets the value of a request's method. */
const char *http_request_get_method(const struct http_request *request);
/** Gets the value of a request's path. */
const char *http_request_get_path(const struct http_request *request);
/** Gets the value of a request's version. */
const char *http_request_get_version(const struct http_request *request);
/** Gets the value of a request's body. */
const char *http_request_get_body(const struct http_request *request);

/** Sets the value of a request's method. */
void http_request_set_method(struct http_request *request, const char *method);
/** Sets the value of a request's path. */
void http_request_set_path(struct http_request *request, const char *path);
/** Sets the value of a request's version. */
void http_request_set_version(struct http_request *request, const char *version);

/** Gets the value of a response's version. */
const char *http_response_get_version(const struct http_response *response);
/** Gets the value of a response's status message. */
const char *http_response_get_status_message(const struct http_response *response);
/** Gets the value of a response's body. */
const char *http_response_get_body(const struct http_response *response);

/** Sets the value of a response's version. */
void http_response_set_version(struct http_response *response, const char *version);
/** Sets the value of a response's status message. */
void http_response_set_status_message(struct http_response *response, const char *status_message);

/** Gets the value of a header's name. */
const char *http_header_get_name(const struct http_header *header);
/** Gets the value of a header's value. */
const char *http_header_get_value(const struct http_header *header);

/** Sets the value of a header's name. */
void http_header_set_name(struct http_header *header, const char *name);
/** Sets the value of a header's value. */
void http_header_set_value(struct http_header *header, const char *value);

/** Gets the value of a query's key. */
const char *http_query_get_key(const struct http_query *query);
/** Gets the value of a query's value. */
const char *http_query_get_value(const struct http_query *query);

/** Sets the value of a query's key. */
void http_query_set_key(struct http_query *query, const char *key);
/** Sets the value of a query's value. */
void http_query_set_value(struct http_query *query, const char *value);

void print_bytes(const char *bytes, size_t bytes_len);

/** Percent encodes a URL. */
void http_url_percent_encode(char *url, char *encoded);
/** Percent decodes a URL. */
void http_url_percent_decode(char *url, char *decoded);

#endif // HTTP_COMMON_H