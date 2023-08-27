#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H

#include <stdint.h>

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
static const char* http_status_messages[] =
{
    HTTP_STATUS_CODES
};
#undef X

/** An enum representing the failure codes for `http_server_parse_request()`. */
enum parse_request_error_types
{
    /** The `recv` syscall failed. */
    REQUEST_PARSE_ERROR_RECV = -1,
    /** The request contained too many headers. */
    REQUEST_PARSE_ERROR_TOO_MANY_HEADERS = -2,
    /** The request took too long to process. */
    REQUEST_PARSE_ERROR_TIMEOUT = -3,
    /** The request contained a malformed header. */
    REQUEST_PARSE_ERROR_MALFORMED_HEADER = -4,
    /** The body is too big. */
    REQUEST_PARSE_ERROR_BODY_TOO_BIG = -5,
};

/** An enum representing the failure codes for `http_client_parse_response()`. */
enum parse_response_error_types
{
    /** The `recv` syscall failed. */
    RESPONSE_PARSE_ERROR_RECV = -1,
    /** The response contained a malformed header. */
    RESPONSE_PARSE_ERROR_MALFORMED_HEADER = -2,
};

/** A structure representing the HTTP request. */
struct http_request
{
    /** The HTTP method. */
    char* method;
    /** The HTTP path the request was sent to. */
    char* path;
    /** The HTTP version. */
    char* version;

    /** The parsed query in the path. */
    struct vector* query; // <http_query>
    /** The HTTP headers. */
    struct vector* headers; // <http_header>
    /** The HTTP body. */
    char* body;
};

/** A structure representing the HTTP response. */
struct http_response
{
    /** The HTTP version. */
    char* version;
    /** The HTTP status code. */
    int status_code;
    /** The HTTP status message. */
    char* status_message;

    /** The HTTP headers. */
    struct vector* headers; // <http_header>
    /** The HTTP body. */
    char* body;
};

/** A structure representing the HTTP header. */
struct http_header
{
    /** The header's name. */
    char* name;
    /** The header's value. */
    char* value;
};

/** A structure representing a query key-value pair. */
struct http_query
{
    /** The query's key. */
    char* key;
    /** The query's value. */
    char* value;
};

/** Percent encodes a URL. */
char* http_url_percent_encode(char* url);
/** Percent decodes a URL. */
char* http_url_percent_decode(char* url);

#endif // HTTP_COMMON_H