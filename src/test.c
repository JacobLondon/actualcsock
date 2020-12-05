#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "acs_sync.h"

struct flatdata {
    uint32_t uid;
    char data[32];
};

#define MAX_CLIENTS 16

int main(void)
{
    /*
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
    */

    struct flatdata me = { 0 };
    struct flatdata clients[MAX_CLIENTS] = { 0 };

    char *p;
    int i;

    acs_sync_init();

    struct acs_sync *sync = acs_sync_new("127.0.0.1", "9999", MAX_CLIENTS, &me, sizeof(me));

    acs_sync_run(sync);
    while (1) {
        printf("> ");
        fgets(me.data, sizeof(me.data), stdin);
        if (me.data[0] == 'X') {
            break;
        }
        acs_sync_write(sync);

        // clear clients each iteration to fill with new data
        memset(clients, 0, sizeof(clients));
        i = 0;
        for (p = acs_sync_read_next(sync);
             p != NULL;
             p = acs_sync_read_next(sync))
        {
            if (i >= MAX_CLIENTS) continue;
            memcpy(&clients[i++], p, sizeof(*clients));
        }

        // display ourselves and each client
        printf("Me: %s\n", me.data);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].uid != 0) {
                printf("Client: %s\n", clients[i].data);
            }
        }
    }

    acs_sync_del(sync);
    acs_sync_cleanup();

    return 0;
}
