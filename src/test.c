#include <assert.h>
#include <stdio.h>

#include "acs.h"

int main(void)
{
    char buf[128] = "Whoaaa hello!";
    struct acs *com;

    acs_init();

    com = acs_new("127.0.0.1", "9999");
    assert(com);

    acs_send(com, buf, sizeof(buf));

    acs_recv(com, buf, sizeof(buf));
    printf("%s\n", buf);

    acs_del(com);
    acs_cleanup();

    #ifdef _WIN32
        getchar();
    #endif

    return 0;
}
