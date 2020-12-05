#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "acs_sync.h"

#define MAX_CLIENTS 16

struct flatdata {
    uint32_t uid;
    char data[32];
};

int main(void)
{
    struct flatdata me = { 0 };
    struct flatdata clients[MAX_CLIENTS] = { 0 };
    char *p;
    int i;

    struct acs_sync *sync;
    enum acs_sync_state state;

    acs_sync_init();

    sync = acs_sync_new("127.0.0.1", "9999", MAX_CLIENTS, &me, sizeof(me));
    acs_sync_run(sync);

    while (1) {
        printf("> ");
        fgets(me.data, sizeof(me.data), stdin);
        if (me.data[0] == 'X') {
            break;
        }

        // enforce finished communication each time instead of allowing latency
        while (1)
        {
            state = acs_sync_get_state(sync);
            switch (state) {
            case ACS_SYNC_WRITE:
                acs_sync_write(sync);
                break;

            case ACS_SYNC_READ:
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

                // ACS_SYNC_READ comes after ACS_SYNC_WRITE, so this iteration is done
                goto out;

            case ACS_SYNC_BUSY: // fallthrough
            default:
                break;
            }
        }

    out:
        // display ourselves and each client
        printf("Me: %s", me.data);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].uid != 0) {
                printf("Client: %s", clients[i].data);
            }
        }
    }

    acs_sync_del(sync);
    acs_sync_cleanup();

    return 0;
}
