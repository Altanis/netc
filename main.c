#include "tests/tcp/test001.c"
#include "tests/tcp/test002.c"


int main()
{
    int testsuite_result[5];
    testsuite_result[0] = test001();
    testsuite_result[1] = test002();

    printf("\n\n\n---RESULTS---\n");

    int testsuite_passed = 1;
    for (int i = 1; i < 3; ++i)
    {
        if (testsuite_result[i - 1] == 1)
        {
            testsuite_passed = 0;
            printf(ANSI_RED "[TEST CASE 00%d] failed\n%s", i, ANSI_RESET);
        }
        else
        {
            printf(ANSI_GREEN "[TEST CASE 00%d] passed\n%s", i, ANSI_RESET);
        }
    };

    if (testsuite_passed == 1)
    {
        printf(ANSI_GREEN "\n\nTEST SUITE PASSED! good job.\n%s", ANSI_RESET);
    }
    else
    {
        printf(ANSI_RED "\n\nTEST SUITE FAILED! fix me.\n%s", ANSI_RESET);
    }

    return 0;
};