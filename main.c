#include "http/server.h"
#include "http/client.h"

#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"

#include "tests/udp/test001.c"
#include "tests/udp/test002.c"

#include "tests/http/test001.c"

#include <time.h>

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
    http_request_set_method(&request, "GET");
    http_request_set_path(&request, "/");
    http_request_set_version(&request, "HTTP/1.1");

    struct http_header content_type = {0};
    http_header_set_name(&content_type, "Content-Type");
    http_header_set_value(&content_type, "text/plain");

    struct http_header content_length = {0};
    http_header_set_name(&content_length, "Content-Length");
    http_header_set_value(&content_length, "5");

    vector_init(&request.headers, 2, sizeof(struct http_header));
    vector_push(&request.headers, &content_type);
    vector_push(&request.headers, &content_length);

    http_request_set_body(&request, "hello");

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
    http_response_set_version(&response, "HTTP/1.1");
    http_response_set_status_message(&response, http_status_messages[status_code == 400 ? HTTP_STATUS_CODE_400 : HTTP_STATUS_CODE_500]);

    struct http_header content_type = {0};
    http_header_set_name(&content_type, "Content-Type");
    http_header_set_value(&content_type, "text/plain");

    struct http_header content_length = {0};
    http_header_set_name(&content_length, "Content-Length");
    http_header_set_value(&content_length, "100");

    vector_init(&response.headers, 2, sizeof(struct http_header));
    vector_push(&response.headers, &content_type);
    vector_push(&response.headers, &content_length);

    http_response_set_body(&response, response_body);

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
    printf("method: %s\n", http_request_get_method(&request));
    printf("path: %s\n", http_request_get_path(&request));
    printf("version: %s\n", http_request_get_version(&request));

    for (size_t i = 0; i < request.query.size; ++i)
    {
        struct http_query* query = vector_get(&request.query, i);
        printf("query: %s=%s\n", http_query_get_key(query), http_query_get_value(query));
    };

    for (size_t i = 0; i < request.headers.size; ++i)
    {
        struct http_header* header = vector_get(&request.headers, i);
        printf("header: %s=%s\n", http_header_get_name(header), http_header_get_value(header));
    };

    printf("body: %s\n", http_request_get_body(&request));

    struct http_response response = {0};
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 200;
    http_response_set_status_message(&response, "OK");

    struct http_header content_type = {0};
    http_header_set_name(&content_type, "Content-Type");
    http_header_set_value(&content_type, "text/plain");

    struct http_header content_length = {0};
    http_header_set_name(&content_length, "Content-Length");
    http_header_set_value(&content_length, "5");

    vector_init(&response.headers, 2, sizeof(struct http_header));
    vector_push(&response.headers, &content_type);
    vector_push(&response.headers, &content_length);

    http_response_set_body(&response, "hello");

    http_server_send_response(server, sockfd, &response);
};

void client_incomingm(struct http_client* client, struct http_response response, void* data)
{
    printf("incoming data at response /\nDETAILS:\n\n");
    printf("version: %s\n", http_response_get_version(&response));
    printf("status code: %d\n", response.status_code);
    printf("status message: %s\n", http_response_get_status_message(&response));

    for (size_t i = 0; i < response.headers.size; ++i)
    {
        struct http_header* header = vector_get(&response.headers, i);
        printf("header: %s=%s\n", http_header_get_name(header), http_header_get_value(header));
    };

    printf("body: %s\n", http_response_get_body(&response));
};

void clistart(struct http_client* client)
{
    int x = http_client_start(client);
    printf("[client]\n netc error:%d\n error:%s\n", netc_errno_reason, netc_strerror());
};

void serstart(struct http_server* server)
{
    int x = http_server_start(server);
    printf("[server]\n netc error:%d\n error:%s\n", netc_errno_reason, netc_strerror());
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
    for (int i = 0; i < 5; ++i)
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

    return 0;

    // struct sockaddr_in addr = {
    //     .sin_family = AF_INET,
    //     .sin_addr.s_addr = INADDR_ANY,
    //     .sin_port = htons(3000)
    // };

    // struct http_server server = {0};

    // server.on_connect = server_incomings;
    // server.on_malformed_request = server_incominge;

    // if (http_server_init(&server, 0, (struct sockaddr*)&addr, sizeof(addr), 128) != 0)
    // {
    //     printf("server failed to initialize %d\n", errno);
    //     return 1;
    // };


    // struct http_route route = {0};
    // route.path = "/*";
    // route.callback = server_incomingm;

    // http_server_create_route(&server, &route);

    // printf("server initialized %d\n", server.server.pfd);
    // // http_server_start(&server);

    // pthread_t thread;
    // pthread_create(&thread, NULL, (void*)serstart, &server);

    // printf("server started\n");

    // struct http_client client = {0};
    // struct sockaddr_in cliaddr = {
    //     .sin_family = AF_INET,
    //     .sin_port = htons(3000)
    // };
    
    // client.on_connect = client_outgoings;
    // client.on_malformed_response = client_incominge;
    // client.on_data = client_incomingm;

    // if (inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr)) <= 0)
    // {
    //     printf(ANSI_RED "[TCP TEST CASE 001] client failed to convert ip address\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
    //     return 1;
    // };

    // if (http_client_init(&client, 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr)) != 0)
    // {
    //     printf(ANSI_RED "[TCP TEST CASE 001] client failed to initialize\nerrno: %d\nerrno reason: %d\n%s", errno, netc_errno_reason, ANSI_RESET);
    //     return 1;
    // };

    // printf("client initialized\n");
    // pthread_create(&thread, NULL, (void*)clistart, &client);

    // printf("client started\n");

    // pthread_join(thread, NULL);

    return 0;
};