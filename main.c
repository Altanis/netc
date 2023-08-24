#include "http/server.h"

#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"

#include "tests/udp/test001.c"
#include "tests/udp/test002.c"

const char* MAPPINGS[] = {
    "[TCP TEST CASE 001]",
    "[TCP TEST CASE 002]",
    "[UDP TEST CASE 001]",
    "[UDP TEST CASE 002]",
};

const char* BANNER = "\
███╗   ██╗███████╗████████╗ ██████╗\n\
████╗  ██║██╔════╝╚══██╔══╝██╔════╝\n\
██╔██╗ ██║█████╗     ██║   ██║     \n\
██║╚██╗██║██╔══╝     ██║   ██║     \n\
██║ ╚████║███████╗   ██║   ╚██████╗\n\
╚═╝  ╚═══╝╚══════╝   ╚═╝    ╚═════╝\n";

void incomings(struct http_server* server, struct netc_tcp_client* client, void* data)
{
    printf("incoming connection\n");
};

void incomingm(struct http_server* server, socket_t sockfd, struct http_request request)
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
};

int main()
{
    int testsuite_result[5];
    testsuite_result[0] = tcp_test001();
    testsuite_result[1] = tcp_test002();
    testsuite_result[2] = udp_test001();
    testsuite_result[3] = udp_test002();

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
    // server.config = {...};

    server.on_connect = incomings;

    if (http_server_init(&server, 0, 1, (struct sockaddr*)&addr, sizeof(addr), 128) != 0)
        printf("server failed to initialize\n");

    http_server_create_route(&server, "/", incomingm);

    printf("server initialized %d\n", server.server.pfd);
    http_server_start(&server);

    return 0;
};