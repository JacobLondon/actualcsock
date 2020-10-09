#include <stdio.h>

#include "acs.h"

int main(void)
{
    char buf[128] = "Whoaaa hello!";

    acs_init("127.0.0.1", "9999");

    acs_send(buf, sizeof(buf));

    acs_recv(buf, sizeof(buf));
    printf("%s", buf);

    acs_cleanup();

    getchar();

    return 0;
}
