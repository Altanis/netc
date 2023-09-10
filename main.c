#include "http/server.h"

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

static void http_server_handler(struct http_server* server, socket_t sockfd, struct http_request request)
{
    printf("request.\n");

    struct http_response response = {0};
    http_response_set_version(&response, "HTTP/1.1");
    response.status_code = 200;
    http_response_set_status_message(&response, "OK");

    struct http_header header = {0};
    http_header_set_name(&header, "Content-Type");
    http_header_set_value(&header, "image/png");

    struct http_header header2 = {0};
    http_header_set_name(&header2, "Transfer-Encoding");
    http_header_set_value(&header2, "chunked");

    vector_init(&response.headers, 2, sizeof(struct http_header));
    vector_push(&response.headers, &header);
    vector_push(&response.headers, &header2);

    // read ./tests/http/image.png (an image)
    FILE* file = fopen("./tests/http/image.png", "rb");
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    http_server_send_response(server, sockfd, &response, NULL, 0);

    // Send the file data in chunks
    char buffer[8192];  // You can adjust the chunk size as needed
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        http_server_send_chunked_data(server, sockfd, buffer, bytes_read);
    };

    http_server_send_chunked_data(server, sockfd, NULL, 0);

    // Close the file
    fclose(file);

    printf("response.\n");
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

    struct http_server server = {0};
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(5001),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (http_server_init(&server, *(struct sockaddr*)&server_address, BACKLOG) != 0) 
    {
        netc_perror("http_server_init", stderr);
        return 1;
    };

    struct http_route main = { .path = "/", .callback = http_server_handler };

    http_server_create_route(&server, &main);
    http_server_start(&server);

    return 0;
};