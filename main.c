#include "http/server.h"
#include "http/client.h"

#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"

#include "tests/udp/test001.c"
#include "tests/udp/test002.c"

#include "tests/http/test001.c"

const char* MAPPINGS[] = {
    "[TCP TEST CASE 001]",
    "[TCP TEST CASE 002]",
    "[UDP TEST CASE 001]",
    "[UDP TEST CASE 002]",
    "[HTTP TEST CASE 001]",
};

const char* BANNER = "\
███╗   ██╗███████╗████████╗ ██████╗\n\
████╗  ██║██╔════╝╚══██╔══╝██╔════╝\n\
██╔██╗ ██║█████╗     ██║   ██║     \n\
██║╚██╗██║██╔══╝     ██║   ██║     \n\
██║ ╚████║███████╗   ██║   ╚██████╗\n\
╚═╝  ╚═══╝╚══════╝   ╚═╝    ╚═════╝\n";

void server_incomings(struct http_server* server, struct tcp_client* client, void* data)
{
    printf("incoming connection\n");
};

void client_outgoings(struct http_client* client, void* data)
{
    printf("client connected\n");

    struct http_request request = {0};
    request.method = "GET";
    request.path = "/";
    request.version = "HTTP/1.1";

    struct vector headers = {0};
    vector_init(&headers, 2, sizeof(struct http_header));
    vector_push(&headers, &(struct http_header){"Content-Type", "text/plain"});
    vector_push(&headers, &(struct http_header){"Content-Length", "5"});

    request.headers = &headers;
    request.body = "hello";

    http_client_send_request(client, &request);
};

void server_incominge(struct http_server* server, socket_t sockfd, enum parse_request_error_types error, void* data)
{
    int status_code = 400;
    char response_body[256] = {0};
    switch (error)
    {
        case REQUEST_PARSE_ERROR_RECV: 
        {
            status_code = 500; 
            sprintf(response_body, "Internal Server Error"); 
            break;
        };
        case REQUEST_PARSE_ERROR_TOO_MANY_HEADERS: sprintf(response_body, "Too Many Headers"); break;
        case REQUEST_PARSE_ERROR_TIMEOUT: sprintf(response_body, "Timeout"); break;
        case REQUEST_PARSE_ERROR_MALFORMED_HEADER: sprintf(response_body, "Malformed Header"); break;
        case REQUEST_PARSE_ERROR_BODY_TOO_BIG: sprintf(response_body, "Body Too Big"); break;
    }

    struct http_response response = {0};
    response.status_code = status_code;
    response.status_message = (char*)(http_status_messages[status_code == 400 ? HTTP_STATUS_CODE_400 : HTTP_STATUS_CODE_500]);
    response.version = "1.1";

    struct vector headers = {0};
    vector_init(&headers, 2, sizeof(struct http_header));
    vector_push(&headers, &(struct http_header){"Content-Type", "text/plain"});
    vector_push(&headers, &(struct http_header){"Content-Length", "100"});

    response.headers = &headers;
    response.body = response_body;

    http_server_send_response(server, sockfd, &response);
};

void client_incominge(struct http_client* client, enum parse_response_error_types error, void* data)
{
    printf("client failed to parse response\n");
    switch (error)
    {
        case RESPONSE_PARSE_ERROR_RECV: printf("recv failed\n"); break;
        case RESPONSE_PARSE_ERROR_MALFORMED_HEADER: printf("malformed header\n"); break;
    };
};

void server_incomingm(struct http_server* server, socket_t sockfd, struct http_request request)
{
    printf("incoming data at request /\nDETAILS:\n\n");
    printf("method: %s\n", request.method);
    printf("path: %s\n", request.path);
    printf("version: %s\n", request.version);

    if (request.query != NULL)
    {
        for (size_t i = 0; i < request.query->size; ++i)
        {
            struct http_query* query = vector_get(request.query, i);
            printf("query: %s=%s\n", query->key, query->value);
        };
    }

    if (request.headers != NULL)
    {
        for (size_t i = 0; i < request.headers->size; ++i)
        {
            struct http_header* header = vector_get(request.headers, i);
            printf("header: %s=%s\n", header->name, header->value);
        };
    };

    printf("body: %s\n", request.body);

    struct http_response response = {0};
    response.version = "HTTP/1.1";
    response.status_code = 200;
    response.status_message = "OK";

    struct vector headers = {0};
    vector_init(&headers, 1, sizeof(struct http_header));
    vector_push(&headers, &(struct http_header){.name = "Content-Type", .value = "text/plain"});
    vector_push(&headers, &(struct http_header){.name = "Content-Length", .value = "5"});
    response.headers = &headers;

    response.body = "hello";

    http_server_send_response(server, sockfd, &response);
};

void client_incomingm(struct http_client* client, struct http_response response, void* data)
{
    printf("incoming data at response /\nDETAILS:\n\n");
    printf("version: %s\n", response.version);
    printf("status code: %d\n", response.status_code);
    printf("status message: %s\n", response.status_message);

    if (response.headers != NULL)
    {
        for (size_t i = 0; i < response.headers->size; ++i)
        {
            struct http_header* header = vector_get(response.headers, i);
            printf("header: %s=%s\n", header->name, header->value);
        };
    };

    printf("body: %s\n", response.body);
};

int main()
{
    int testsuite_result[5];
    testsuite_result[0] = tcp_test001();
    testsuite_result[1] = tcp_test002();
    testsuite_result[2] = udp_test001();
    testsuite_result[3] = udp_test002();
    testsuite_result[4] = http_test001();

    printf("\n\n\n%s", BANNER);

    printf("\n\n\n---RESULTS---\n");

    int testsuite_passed = 1;
    for (int i = 0; i < 4; ++i)
    {
        if (testsuite_result[i] == 1)
        {
            testsuite_passed = 0;
            printf(ANSI_RED "%s failed\n%s", MAPPINGS[i], ANSI_RESET);
        }
        else
        {
            printf(ANSI_GREEN "%s passed\n%s", MAPPINGS[i], ANSI_RESET);
        }
    };

    if (testsuite_passed == 1)
    {
        printf(ANSI_GREEN "\n\nTEST SUITE PASSED! good job.\n%s", ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "\n\nTEST SUITE FAILED! fix me.\n%s", ANSI_RESET);
    };

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(3000)
    };

    struct http_server server = {0};

    server.on_connect = server_incomings;
    server.on_malformed_request = server_incominge;

    if (http_server_init(&server, 0, 1, (struct sockaddr*)&addr, sizeof(addr), 128) != 0)
        printf("server failed to initialize\n");

    struct http_route route = {0};
    route.path = "/*";
    route.callback = server_incomingm;

    http_server_create_route(&server, &route);

    printf("server initialized %d\n", server.server.pfd);
    // http_server_start(&server);

    pthread_t thread;
    pthread_create(&thread, NULL, (void*)http_server_start, &server);

    printf("server started\n");

    struct http_client client = {0};
    struct sockaddr_in cliaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(3000)
    };
    
    client.on_connect = client_outgoings;
    client.on_malformed_response = client_incominge;
    client.on_data = client_incomingm;

    if (inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr)) <= 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    if (http_client_init(&client, 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr)) != 0)
    {
        printf(ANSI_RED "[TCP TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
        return 1;
    };

    printf("client initialized\n");
    pthread_create(&thread, NULL, (void*)http_client_start, &client);

    printf("client started\n");

    pthread_join(thread, NULL);

    return 0;
};