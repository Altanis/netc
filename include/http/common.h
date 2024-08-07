#ifndef HTTP_HELPER_COMMON_H
#define HTTP_HELPER_COMMON_H

#include "../utils/vector.h"
#include "../utils/string.h"

#include <stdint.h>
#include <stdbool.h>

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

/** Converts an integer HTTP status code to it's status message (i.e. `200 -> OK`). */
const char *http_status_code_to_message(int status_code);

/** An enum representing the different states during parsing a request. */
enum http_request_parsing_states
{
    /** No parsing state has been assigned yet. */
    REQUEST_PARSING_STATE_NIL = -1,

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
    /** Initial state. */
    RESPONSE_PARSING_STATE_INITIAL = -1,
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

/** An enum representing the different connection types. */
enum connection_types
{
    /** The connection uses HTTP. */
    CONNECTION_HTTP,
    /** The connection uses WebSocket. */
    CONNECTION_WS
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

    /** Whether or not the request wants to upgrade to WS protocol. */
    bool upgrade_websocket;
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

    /** Whether or not the server is willing to accept a WS connection. */
    bool accept_websocket;
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
    /** The dynamically sized buffer for chunked data. */
    struct vector chunk_data;
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

/** Builds an HTTP request. */
void http_request_build(struct http_request *request, const char *method, const char *path, const char *version, const char *headers[][2], size_t headers_length);
/** Frees an HTTP request. */
void http_request_free(struct http_request *request);

/** Gets the value of a request's method. */
const char *http_request_get_method(struct http_request *request);
/** Gets the value of a request's path. */
const char *http_request_get_path(struct http_request *request);
/** Gets the value of a request's version. */
const char *http_request_get_version(struct http_request *request);
/** Gets the value of a request's header, given the name. */
struct http_header *http_request_get_header(struct http_request *request, const char *name);
/** Gets the value of a request's body. */
char *http_request_get_body(struct http_request *request);
/** Gets the size of a request's body. */
size_t http_request_get_body_size(struct http_request *request);

/** Sets the value of a request's method. */
void http_request_set_method(struct http_request *request, const char *method);
/** Sets the value of a request's path. */
void http_request_set_path(struct http_request *request, const char *path);
/** Sets the value of a request's version. */
void http_request_set_version(struct http_request *request, const char *version);

/** Builds an HTTP request. */
void http_response_build(struct http_response *response, const char *version, int status_code, const char *headers[][2], size_t headers_length);
/** Frees an HTTP response. */
void http_response_free(struct http_response *response);

/** Gets the value of a response's version. */
const char *http_response_get_version(struct http_response *response);
/** Gets the value of a response's status message. */
const char *http_response_get_status_message(struct http_response *response);
/** Gets the value of a response's header, given the name. */
const char *http_response_get_header(struct http_response *response, const char *name);
/** Gets the value of a response's body. */
char *http_response_get_body(struct http_response *response);

/** Sets the value of a response's version. */
void http_response_set_version(struct http_response *response, const char *version);
/** Sets the value of a response's status message. */
void http_response_set_status_message(struct http_response *response, const char *status_message);

/** Initializes an HTTP header. */
void http_header_init(struct http_header *header, const char *name, const char *value);
/** Frees an HTTP header. */
void http_header_free(struct http_header *header);

/** Gets the value of a header's name. */
const char *http_header_get_name(struct http_header *header);
/** Gets the value of a header's value. */
char *http_header_get_value(struct http_header *header);

/** Sets the value of a header's name. */
void http_header_set_name(struct http_header *header, const char *name);
/** Sets the value of a header's value. */
void http_header_set_value(struct http_header *header, const char *value);

/** Gets the value of a query's key. */
const char *http_query_get_key(struct http_query *query);
/** Gets the value of a query's value. */
const char *http_query_get_value(struct http_query *query);

/** Percent encodes a URL. */
void http_url_percent_encode(char *url, char *encoded);
/** Percent decodes a URL. */
void http_url_percent_decode(char *url, char *decoded);

/** Base64 encodes a string. */
void http_base64_encode(char *bytes, size_t bytes_len, char *encoded);
/** Base64 decodes a string. */
void http_base64_decode(char *encoded, char *bytes, size_t *bytes_len);

#endif // HTTP_HELPER_COMMON_H