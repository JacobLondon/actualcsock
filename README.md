# ACS
The reason for this library is due to the lack of simple interfaces for having shared state between one server and many clients. There are many utilities for creating such a thing, but all of them are very large, not necessarily cross platform, or need some kind of autotools/cmake or something. Too complicated. All I want is something that can just work with this library as a submodule or something. Furthermore, there is so much complexity in handling clients for some other utilities, such as callback functions, etc. I figured the simplest possible interface would be as follows: send my client info, and traverse all other clients' info. This library allows for that to be possible.
```bash
git clone git@github.com:/JacobLondon/actualcsock --recurse-submodules
```

## Libraries
* ACS
  * EZ socket communication for send/recv
* ACS_SYNC
  * Synchronize state among many clients and a server with simple interface
* Linked List
  * what is says

### ACS
```C
#include <stdio.h>
#include "acs.h"

int main(void)
{
    char buf[128] = "Hello, World!";
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
```

### ACS_SYNC
How to use: Run `python servetest.py` and as many instances of this program as you want. They will all share chat data.
```C
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
```

### Linked List
```C
	struct list_node *tmp;
	struct list_node **cursor;
	struct list *mylist = list_new(free);

	list_push_front(mylist, strdup("1"));
	printf("Second element: %p\n", list_tail(mylist));
	tmp = list_push_back(mylist, strdup("3"));
	printf("Second element: %p\n", list_tail(mylist));
	tmp = list_insert_next(mylist, tmp, strdup("4"));
	list_insert_next(mylist, mylist->head, strdup("2"));
	list_push_back(mylist, strdup("4"));
	list_push_front(mylist, strdup("0"));
	list_pop_front(mylist);
	list_pop_back(mylist);
	list_remove(mylist, tmp);

	tmp = list_find(mylist, "1", (int (*)(void *, void *))strcmp);
	printf("Found '1'?: %p\n", tmp);
	list_remove(mylist, tmp);
	tmp = list_find(mylist, "1", (int (*)(void *, void *))strcmp);
	printf("Found '1' after remove?: %p\n", tmp);

	for (cursor = list_iter_begin(mylist); !list_iter_done(cursor); list_iter_continue(&cursor)) {
		printf("%s\n", (char *)list_iter_value(cursor));
	}

	while (mylist->head != NULL) {
		list_remove(mylist, mylist->head);
	}

	list_push_front(mylist, strdup("1"));
	printf("Second element: %p\n", list_tail(mylist));
	tmp = list_push_back(mylist, strdup("3"));
	printf("Second element: %p\n", list_tail(mylist));
	tmp = list_insert_next(mylist, tmp, strdup("4"));
	list_insert_next(mylist, mylist->head, strdup("2"));
	list_push_back(mylist, strdup("4"));
	list_push_front(mylist, strdup("0"));
	list_pop_front(mylist);
	list_pop_back(mylist);
	list_remove(mylist, tmp);

	tmp = list_find(mylist, "1", (int (*)(void *, void *))strcmp);
	printf("Found '1'?: %p\n", tmp);
	list_remove(mylist, tmp);
	tmp = list_find(mylist, "1", (int (*)(void *, void *))strcmp);
	printf("Found '1' after remove?: %p\n", tmp);

	for (cursor = list_iter_begin(mylist); !list_iter_done(cursor); list_iter_continue(&cursor)) {
		printf("%s\n", (char *)list_iter_value(cursor));
	}

	printf("Done\n");
	list_free(mylist);
```
