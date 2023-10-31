#include "./include/web/server.h"

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
    struct http_header *content_type = http_request_get_header(&request, "Content-Type");
    char *text_plain_maybe = content_type == NULL ? "text/plain" : sso_string_get(&content_type->value);

    struct http_response response;
    http_response_build(&response, "HTTP/1.1", 200, (char *[][2]) { { "Content-Type", text_plain_maybe } }, 1);

    http_server_send_response(server, client, &response, "ok", 2);
};

void ws_start_handling_moment(struct web_server *server, struct web_client *client, struct http_request request)
{
    int r = 0;
    if ((r = ws_server_upgrade_connection(server, client, &request)) < 0) netc_perror("Dang nabbit. %d", r);
    else printf("ws connected sexd...\n");
};

void ws_start_failing_moment(struct web_server *server, struct web_client *client, enum ws_frame_parsing_errors error)
{
    // todo: ws_server_close();
};

void ws_start_handling_messages_moment(struct web_server *server, struct web_client *client, struct ws_message message)
{
    printf("%s\n", message.buffer);

    struct ws_header header =
    {
        .fin = 0, // does NOT matter
        .rsv1 = 0,
        .rsv2 = 0,
        .rsv3 = 0,
        .opcode = WS_OPCODE_TEXT
    };

    struct ws_frame frame = 
    {
        .header = header,
        .mask = 0,
        .payload_length = 10
    };

    if (ws_server_send_frame(server, client, &frame, "aaabbbcccd", 2) < 0) netc_perror("WTF");
    else printf("couldnt.\n");
};

void ws_start_handling_closes_moment(struct web_server *server, struct web_client *client, uint16_t code, char *reason)
{
    printf("ws closed sexd... [%d] %s\n", code, reason);
    printf("done.\n");
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

    // return 0;

    struct web_server server = {0};
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(5001),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (web_server_init(&server, *(struct sockaddr *)&server_address, BACKLOG) != 0) 
    {
        netc_perror("web_server_init");
        return 1;
    };

    struct web_server_route main = { 
        .path = "/", 
        .on_http_message = http_handling_moment,
        .on_ws_handshake_request = ws_start_handling_moment,
        .on_ws_malformed_frame = ws_start_failing_moment,
        .on_ws_message = ws_start_handling_messages_moment,
        .on_ws_close = ws_start_handling_closes_moment,
    };

    web_server_create_route(&server, &main);
    web_server_start(&server);

    return 0;
};