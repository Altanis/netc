#include "./include/web/server.h"
#include "./include/web/client.h"

#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"

#include "tests/udp/test001.c"
#include "tests/udp/test002.c"

#include "tests/http/test001.c"
#include "tests/ws/test001.c"

#include <time.h>

const char *MAPPINGS[] = {
    "[TCP TEST CASE 001]",
    "[TCP TEST CASE 002]",
    "[UDP TEST CASE 001]",
    "[UDP TEST CASE 002]",
    "[HTTP TEST CASE 001]",
    "[WS TEST CASE 001]",
};

const char *BANNER = "\
███╗   ██╗███████╗████████╗ ██████╗\n\
████╗  ██║██╔════╝╚══██╔══╝██╔════╝\n\
██╔██╗ ██║█████╗     ██║   ██║     \n\
██║╚██╗██║██╔══╝     ██║   ██║     \n\
██║ ╚████║███████╗   ██║   ╚██████╗\n\
╚═╝  ╚═══╝╚══════╝   ╚═╝    ╚═════╝\n";

int main()
{
    int testsuite_result[6] = {0};
    testsuite_result[0] = tcp_test001();
    testsuite_result[1] = tcp_test002();
    testsuite_result[2] = udp_test001();
    testsuite_result[3] = udp_test002();
    testsuite_result[4] = http_test001();
    testsuite_result[5] = ws_test001();

    printf("\n\n\n%s", BANNER);

    printf("\n\n\n---RESULTS---\n");

    int testsuite_passed = 1;
    for (int i = 0; i < 6; ++i)
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
        printf(ANSI_GREEN "\n\nTEST SUITE PASSED! i fucking love you and it breaks my heart when i see you talk to someone else i just want to be your boyfriend and put a heart in my profile linking to your profile and have a bunch of dms of you saying cute things i want to play video games talk in discord all night and watch a movie together but you just seem so uninterested in me it fucking kills me and i cant take it anymore i want to remove you but i care too much about you so please i'm begging you to either love me back or remove me and NEVER contact me again it hurts so much to say this because i need you by my side but if you don't love me then i want you to leave because seeing your icon in my friendlist would kill me everyday of my pathetic life..\n%s", ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "\n\nTEST SUITE FAILED! fix me.\n%s", ANSI_RESET);
    };

    // return 0;

    // struct web_server server = {0};
    // struct sockaddr_in server_address = {
    //     .sin_family = AF_INET,
    //     .sin_port = htons(5001),
    //     .sin_addr.s_addr = INADDR_ANY,
    // };

    // if (web_server_init(&server, (struct sockaddr *)&server_address, BACKLOG) != 0) 
    // {
    //     netc_perror("web_server_init");
    //     return 1;
    // };

    // struct web_server_route main = { 
    //     .path = "/ziggy",
    //     .on_http_message = http_handling_moment,
    //     .on_ws_handshake_request = ws_start_handling_moment,
    //     .on_ws_malformed_frame = ws_start_failing_moment,
    //     .on_ws_message = ws_start_handling_messages_moment,
    //     .on_ws_close = ws_start_handling_closes_moment,
    // };

    // web_server_create_route(&server, &main);

    // pthread_t thread;
    // pthread_create(&thread, NULL, ws_server_start, &server);
    // pthread_join(thread, NULL);

    // struct web_client client = {0};
    // client.on_http_connect = ws_client_on_http_connect;
    // client.on_ws_connect = ws_client_on_ws_connect;
    // client.on_http_malformed_response = ws_client_on_http_malformed_response;
    // client.on_ws_malformed_frame = ws_client_on_ws_malformed_frame;
    // client.on_ws_message = ws_client_on_ws_message;
    // client.on_http_response = ws_client_on_http_response;
    // client.on_ws_disconnect = ws_client_on_close;

    // struct sockaddr_in cliaddr = {
    //     .sin_family = AF_INET,
    //     .sin_port = htons(5001)
    // };

    // if (inet_pton(AF_INET, "127.0.0.1", &cliaddr.sin_addr) < 0) perror("inet_pton");
    // if (web_client_init(&client, (struct sockaddr *)&cliaddr) < 0) netc_perror("web_client_init");
    
    // web_client_start(&client);

    return 0;
};