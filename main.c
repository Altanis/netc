#include "web/server.h"

#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"

#include "tests/udp/test001.c"
#include "tests/udp/test002.c"

#include "tests/http/test001.c"

#include <time.h>

const char *MAPPINGS[] = {
    "[TCP TEST CASE 001]",
    "[TCP TEST CASE 002]",
    "[UDP TEST CASE 001]",
    "[UDP TEST CASE 002]",
    "[HTTP TEST CASE 001]",
};

const char *BANNER = "\
███╗   ██╗███████╗████████╗ ██████╗\n\
████╗  ██║██╔════╝╚══██╔══╝██╔════╝\n\
██╔██╗ ██║█████╗     ██║   ██║     \n\
██║╚██╗██║██╔══╝     ██║   ██║     \n\
██║ ╚████║███████╗   ██║   ╚██████╗\n\
╚═╝  ╚═══╝╚══════╝   ╚═╝    ╚═════╝\n";

int http_handling_moment(struct web_server *server, struct web_client *client, struct http_request request)
{
    struct http_response response;
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 200;
    http_response_set_status_message(&response, http_status_messages[HTTP_STATUS_CODE_200]);

    char *content_type = http_request_get_header(&request, "Content-Type");

    struct http_header header;
    http_header_init(&header);

    http_header_set_name(&header, "Content-Type");
    http_header_set_value(&header, content_type == NULL ? "text/plain" : content_type);

    vector_init(&response.headers, 1, sizeof(struct http_header));
    vector_push(&response.headers, &header);

    http_server_send_response(server, client, &response, http_request_get_body(&request), http_request_get_body_size(&request));
};

void ws_start_handling_moment(struct web_server *server, struct web_client *client, struct http_request request)
{
    printf("lol...\n");
};

int main()
{
    int testsuite_result[5] = {0};
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

    struct web_server server = {0};
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(5001),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (web_server_init(&server, *(struct sockaddr *)&server_address, BACKLOG) != 0) 
    {
        netc_perror("web_server_init", stderr);
        return 1;
    };

    struct web_server_route main = { 
        .path = "/", 
        .on_http_message = http_handling_moment,
        .on_ws_handshake_request = ws_start_handling_moment,
    };

    web_server_create_route(&server, &main);
    web_server_start(&server);

    return 0;
};