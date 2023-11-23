#ifndef HTTP_TEST_001
#define HTTP_TEST_001

#include "../../include/web/server.h"
#include "../../include/web/client.h"
#include "../../include/utils/error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>

#ifdef __APPLE__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#endif

#undef IP
#undef PORT
#undef BACKLOG
#undef USE_IPV6
#undef SERVER_NON_BLOCKING
#undef CLIENT_NON_BLOCKING
#undef ANSI_RED
#undef ANSI_GREEN
#undef ANSI_RESET

#define IP "127.0.0.1"
#define PORT 8081
#define BACKLOG 3
#define USE_IPV6 0
#define SERVER_NON_BLOCKING 1
#define CLIENT_NON_BLOCKING 1

#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

static struct web_server server = {0};

/** At the end of this test, all of these values must equal 1 unless otherwise specified. */
static int http_test001_server_connect = 0;
static int http_test001_server_data = 0; // 0 = passed /, 1 = passed /wow, 2 = passed /wow?x=1, 3 = passed /?x=1, 4 = passed /test (priority over /*)
static int http_test001_server_disconnect = 0;
static int http_test001_client_connect = 0;
static int http_test001_client_data = 0; // 0 = passed /, 1 = passed /wow, 2 = passed /wow?x=1, 3 = passed /?x=1, 4 = passed /test (priority over /*)
static int http_test001_client_disconnect = 0;

static void http_test001_server_on_connect(struct web_server *server, struct web_client *client);
static void http_test001_server_on_data(struct web_server *server, struct web_client *client, struct http_request *request);
static void http_test001_server_on_data_wrong_route(struct web_server *server, struct web_client *client, struct http_request *request);
static void http_test001_server_on_http_malformed_request(struct web_server *server, struct web_client *client, enum parse_request_error_types error);
static void http_test001_server_on_disconnect(struct web_server *server, socket_t sockfd, bool is_error);

static void http_test001_client_on_connect(struct web_client *client);
static void http_test001_client_on_data(struct web_client *client, struct http_response *response);
static void http_test001_client_on_malformed_response(struct web_client *client, enum parse_response_error_types error);
static void http_test001_client_on_disconnect(struct web_client *client, bool is_error);

static void print_request(struct http_request request);
static void print_response(struct http_response response);

static int http_test001();

static void print_request(struct http_request request)
{
    printf("\n\nmethod: %s\n", http_request_get_method(&request));
    printf("path: %s\n", http_request_get_path(&request));
    printf("version: %s\n", http_request_get_version(&request));

    for (size_t i = 0; i < request.query.size; ++i)
    {
        struct http_query *query = vector_get(&request.query, i);
        printf("query: %s=%s\n", http_query_get_key(query), http_query_get_value(query));
    };

    for (size_t i = 0; i < request.headers.size; ++i)
    {
        struct http_header *header = vector_get(&request.headers, i);
        printf("header: %s=%s\n", http_header_get_name(header), http_header_get_value(header));
    };

    printf("request body? %s\n", request.body);

    printf("body: %s\n\n", http_request_get_body(&request));
};

static void print_response(struct http_response response)
{
    printf("\n\nversion: %s\n", http_response_get_version(&response));
    printf("status code: %d\n", response.status_code);
    printf("status message: %s\n", http_response_get_status_message(&response));

    for (size_t i = 0; i < response.headers.size; ++i)
    {
        struct http_header *header = vector_get(&response.headers, i);
        printf("header: %s=%s\n", http_header_get_name(header), http_header_get_value(header));
    };

    printf("body: %s\n\n", http_response_get_body(&response));
};

static void http_test001_server_on_connect(struct web_server *server, struct web_client *client)
{
    http_test001_server_connect = 1;

    char *ip = calloc(INET6_ADDRSTRLEN, sizeof(char));
    if (client->tcp_client->sockaddr.sa_family == AF_INET)
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)&client->tcp_client->sockaddr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, client->tcp_client->sockfd);
    }
    else
    {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&client->tcp_client->sockaddr;
        inet_ntop(AF_INET6, &addr->sin6_addr, ip, client->tcp_client->sockfd);
    };

    client->data = ip;
    printf("[HTTP TEST CASE 001] server accepting client. ip: %s\n", (char *)client->data);
};

static void http_test001_server_on_data(struct web_server *server, struct web_client *client, struct http_request *h_request)
{
    struct http_request request = *h_request;

    ++http_test001_server_data;
    
    printf("[HTTP TEST CASE 001] server received data from %s at endpoint \"/\"\n", (char *)client->data);
    print_request(request);

    int chunked = 0;
    for (size_t i = 0; i < request.headers.size; ++i)
    {
        if (strcasecmp(http_header_get_name(vector_get(&request.headers, i)), "Transfer-Encoding") == 0)
        {
            chunked = 1;
            break;
        };
    };

    struct http_response response = {0};
    http_response_build(&response, "HTTP/1.1", 200, (const char *[][2]){ {"Content-Type", "text/plain"} }, 1);    

    if (http_test001_server_data == 8)
    {
        // write image to ./tests/http/tests/server_recv.png
        FILE *file_w = fopen("./tests/http/tests/server_recv.png", "wb");
        for (size_t i = 0; i < request.body_size; ++i)
            fwrite(request.body + i, 1, 1, file_w);
        fclose(file_w);
        printf("Wrote image to ./tests/http/tests/server_recv.png\n");


        printf("sending image\n");
        struct http_header transfer_encoding = {0};
        http_header_set_name(&transfer_encoding, "Transfer-Encoding");
        http_header_set_value(&transfer_encoding, "chunked");

        vector_push(&response.headers, &transfer_encoding);

        printf("found?\n");
        FILE *file = fopen("./tests/http/image.png", "rb");
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        http_server_send_response(server, client, &response, NULL, 0);

        char buffer[8192];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            http_server_send_chunked_data(server, client, buffer, bytes_read);
        };

        // write image to ./tests/http/tests/server_send.png
        FILE *file_w2 = fopen("./tests/http/tests/server_send.png", "wb");
        for (size_t i = 0; i < file_size; ++i)
            fwrite(buffer + i, 1, 1, file_w2);
        fclose(file_w2);

        http_server_send_chunked_data(server, client, NULL, 0);

        fclose(file);
    }
    else if (chunked)
    {
        printf("sending \"helloworld\"\n");
     
        struct http_header transfer_encoding = {0};
        http_header_set_name(&transfer_encoding, "Transfer-Encoding");
        http_header_set_value(&transfer_encoding, "chunked");

        vector_push(&response.headers, &transfer_encoding);

        http_server_send_response(server, client, &response, NULL, 0);
        http_server_send_chunked_data(server, client, "he", 2);
        http_server_send_chunked_data(server, client, "llo", 3);
        http_server_send_chunked_data(server, client, "world", 5);
        http_server_send_chunked_data(server, client, NULL, 0);
    }
    else 
    {
        printf("sending \"hello\"\n");
        http_server_send_response(server, client, &response, "hello", 5);
    }
};

static void http_test001_server_on_data_wrong_route(struct web_server *server, struct web_client *client, struct http_request *h_request)
{
    struct http_request request = *h_request;

    ++http_test001_server_data;
    printf("Sending \"later\"\n");
    
    printf("[HTTP TEST CASE 001] server received data from %s at endpoint \"/test\"\n", (char *)client->data);
    print_request(request);

    struct http_response response = {0};
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 200;
    http_response_set_status_message(&response, "OK");

    struct http_header content_type = {0};
    http_header_set_name(&content_type, "Content-Type");
    http_header_set_value(&content_type, "text/plain");

    vector_init(&response.headers, 1, sizeof(struct http_header));
    vector_push(&response.headers, &content_type);

    http_server_send_response(server, client, &response, "later", 5);
};

static void http_test001_server_on_http_malformed_request(struct web_server *server, struct web_client *client, enum parse_request_error_types error)
{
    printf("[HTTP TEST CASE 001] server could not process request from %s\n", (char *)client->data);
};

static void http_test001_server_on_disconnect(struct web_server *server, socket_t sockfd, bool is_error)
{
    if (sockfd == server->tcp_server->sockfd)
    {
        http_test001_server_disconnect = 1;
        printf("[HTTP TEST CASE 001] server disconnected from from %d\n", sockfd);
    }
    else
    {
        struct web_client *client = map_get(&server->clients, &sockfd, sizeof(sockfd));
        if (client->tcp_client->sockfd != sockfd)
        {
            printf("error with map.\n");
            exit(1);
        };

        printf("[HTTP TEST CASE 001] client disconnected from server %d\n", client->tcp_client->sockfd);
    };
};

static void http_test001_client_on_connect(struct web_client *client)
{
    http_test001_client_connect = 1;

    printf("[HTTP TEST CASE 001] client connected\n");
    printf("sending \"hello\"\n");

    struct http_request request = {0};
    http_request_set_method(&request, "GET");
    http_request_set_path(&request, "/");
    http_request_set_version(&request, "HTTP/1.1");

    struct http_header content_type = {0};
    http_header_set_name(&content_type, "Content-Type");
    http_header_set_value(&content_type, "text/plain");

    vector_init(&request.headers, 2, sizeof(struct http_header));
    vector_push(&request.headers, &content_type);

    printf("wow? %s\n", http_request_get_method(&request));
    
    http_client_send_request(client, &request, "hello", 5);
};

static void http_test001_client_on_data(struct web_client *client, struct http_response *h_response)
{
    struct http_response response = *h_response;

    printf("[HTTP TEST CASE 001] client received data\n");
    print_response(response);

    if (++http_test001_client_data <= 7)
    {
        const char *path = (http_test001_client_data == 1 ? "/" : (http_test001_client_data == 2 ? "/wow" : (http_test001_client_data == 3 ? "/wow?x=1" : (http_test001_client_data == 4 ? "/?x=1" : 
        (http_test001_client_data == 5 ? "/test" : "/")))));

        struct http_request request = {0};
        http_request_set_method(&request, "POST");
        http_request_set_path(&request, path);
        http_request_set_version(&request, "HTTP/1.1");

        struct http_header content_type = {0};
        http_header_set_name(&content_type, "Content-Type");
        http_header_set_value(&content_type, "text/plain");

        vector_init(&request.headers, 2, sizeof(struct http_header));
        vector_push(&request.headers, &content_type);

        if (http_test001_client_data < 6)
        {
            http_client_send_request(client, &request, "hello", 5);
        }
        else if (http_test001_client_data == 6)
        {
            // transfer encoding
            struct http_header transfer_encoding = {0};
            http_header_set_name(&transfer_encoding, "Transfer-Encoding");
            http_header_set_value(&transfer_encoding, "chunked");

            vector_push(&request.headers, &transfer_encoding);

            http_client_send_request(client, &request, NULL, 0);
            http_client_send_chunked_data(client, "he", 2);
            http_client_send_chunked_data(client, "llo", 3);
            http_client_send_chunked_data(client, "world", 5);
            http_client_send_chunked_data(client, NULL, 0);
        }
        else if (http_test001_client_data == 7)
        {
            // transfer encoding
            struct http_header transfer_encoding = {0};
            http_header_set_name(&transfer_encoding, "Transfer-Encoding");
            http_header_set_value(&transfer_encoding, "chunked");

            struct http_header connection = {0};
            http_header_set_name(&connection, "Connection");
            http_header_set_value(&connection, "close");

            vector_push(&request.headers, &transfer_encoding);
            vector_push(&request.headers, &connection);

            FILE *file = fopen("./tests/http/image.png", "rb");
            if (file == NULL) perror("file is null\n");
            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            fseek(file, 0, SEEK_SET);

            http_client_send_request(client, &request, NULL, 0);

            char buffer[8192];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
            {
                http_client_send_chunked_data(client, buffer, bytes_read);
            };

            http_client_send_chunked_data(client, NULL, 0);

            // write image to ./tests/http/tests/client_send.png
            FILE *file_w = fopen("./tests/http/tests/client_send.png", "wb");
            for (size_t i = 0; i < file_size; ++i)
                fwrite(buffer + i, 1, 1, file_w);
            fclose(file_w);

            fclose(file);
        };
    } 
    else 
    {
        // write `request.body` to ./tests/http/tests/client_recv.png
        FILE *file = fopen("./tests/http/tests/client_recv.png", "wb");
        for (size_t i = 0; i < response.body_size; ++i)
            fwrite(response.body + i, 1, 1, file);
        fclose(file);

        printf("Wrote image to ./tests/http/tests/client_recv.png\n");

        web_server_close(&server);
    };
};

static void http_test001_client_on_malformed_response(struct web_client *client, enum parse_response_error_types error)
{
    printf(ANSI_RED "[HTTP TEST CASE 001] client received malformed response\n%s", ANSI_RESET);
};

static void http_test001_client_on_disconnect(struct web_client *client, bool is_error)
{
    http_test001_client_disconnect = 1;

    printf("[HTTP TEST CASE 001] client disconnected\n");
};

static int http_test001()
{
    server.on_connect = http_test001_server_on_connect;
    server.on_http_malformed_request = http_test001_server_on_http_malformed_request;
    server.on_disconnect = http_test001_server_on_disconnect;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (web_server_init(&server, *(struct sockaddr *)&addr, BACKLOG) != 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] server failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    struct web_server_route test_route = { .path = "/test", .on_http_message = http_test001_server_on_data_wrong_route };
    struct web_server_route wildcard_route = { .path = "/*", .on_http_message = http_test001_server_on_data };

    /** 
     * Please note hierarchy matters. If a path matches two routes,
     * it will use the callback initialized first. 
     */
    web_server_create_route(&server, &test_route);
    web_server_create_route(&server, &wildcard_route);

    pthread_t servt;
    pthread_create(&servt, NULL, (void *)web_server_start, &server);

    struct web_client client = {0};
    client.on_http_connect = http_test001_client_on_connect;
    client.on_http_malformed_response = http_test001_client_on_malformed_response;
    client.on_http_response = http_test001_client_on_data;
    client.on_http_disconnect = http_test001_client_on_disconnect;

    struct sockaddr_in cliaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, IP, &(cliaddr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    if (web_client_init(&client, *(struct sockaddr *)&cliaddr) != 0)
    {
        printf(ANSI_RED "[HTTP TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    pthread_t clit;
    pthread_create(&clit, NULL, (void *)web_client_start, &client);

    pthread_join(clit, NULL);
    pthread_join(servt, NULL);

    if (http_test001_server_connect == 1) {
        printf(ANSI_GREEN "[HTTP TEST 001] server received connection correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] server received connection incorrectly\n" ANSI_RESET);
    }

    if (http_test001_server_data == 8) {
        printf(ANSI_GREEN "[HTTP TEST 001] server handled data correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] server handled data incorrectly\n" ANSI_RESET);
    }

    if (http_test001_server_disconnect == 1) {
        printf(ANSI_GREEN "[HTTP TEST 001] server disconnected correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] server disconnected incorrectly\n" ANSI_RESET);
    }

    if (http_test001_client_connect == 1) {
        printf(ANSI_GREEN "[HTTP TEST 001] client connected correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] client connected incorrectly\n" ANSI_RESET);
    }

    if (http_test001_client_data == 8) {
        printf(ANSI_GREEN "[HTTP TEST 001] client handled data correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] client handled data incorrectly\n" ANSI_RESET);
    }

    if (http_test001_client_disconnect == 1) {
        printf(ANSI_GREEN "[HTTP TEST 001] client disconnected correctly\n" ANSI_RESET);
    } else {
        printf(ANSI_RED "[HTTP TEST CASE 001] client disconnected incorrectly\n" ANSI_RESET);
    }

    return (int)(http_test001_server_connect == 1 && http_test001_server_data == 8 && http_test001_server_disconnect == 1 && http_test001_client_connect == 1 && http_test001_client_data == 8 && http_test001_client_disconnect == 1 ? 0 : 1);
};

#endif // HTTP_TEST_001