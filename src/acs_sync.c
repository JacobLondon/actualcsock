#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <memory.h>

// millisleep util
#if defined(WIN32)
#include <windows.h>
#elif defined(__unix__)
#include <time.h>
#include <unistd.h>
#else
#endif

#include <tinycthread.h>

#include "acs_sync.h"
#include "list.h"

/*
 * Macros
 */

#define BIT_SET(BYTE_POINTER, BITNO) \
    do { \
        ((BYTE_POINTER)[(BITNO) / 8]) |= (1 << ((BITNO) % 8)); \
    } while (0)

#define BIT_TEST(BYTE_POINTER, BITNO) \
    ((BYTE_POINTER)[(BITNO) / 8] & (1 << ((BITNO) % 8)))

/*
 * Data Types
 */

struct send_data {
    void *flatdata;
    size_t flatsize;
};

struct acs_sync {
    struct acs *sock;

    thrd_t thread;
    int thread_done;

    // send data
    mtx_t send_mutex;
    struct send_data data_main;   // version from main thread
    struct send_data data_thread; // local copy for thread to have

    // recv data
    mtx_t recv_mutex;
    struct list *recv_data;
    struct node **cursor_main;

    // record who is connected in a bitmap, set means connected
    unsigned char *client_bitmap;
    size_t client_bitmap_size;
};

/*
 * Static Function Prototypes
 */

static int millisleep(unsigned ms);
static int data_cmp(void *value, void *query);
static int thread_func(void *client);

/*
 * Static Variables
 */

static int initialized = 0;

/*
 * Static Function Definitions
 */

static int millisleep(unsigned ms)
{
    // https://stackoverflow.com/questions/14812233/sleeping-for-milliseconds-on-windows-linux-solaris-hp-ux-ibm-aix-vxworks-w
#if defined(WIN32)

    SetLastError(0);
    Sleep(ms);
    return GetLastError() ? -1 : 0;

#elif _POSIX_C_SOURCE >= 199309L

    /* prefer to use nanosleep() */

    const struct timespec ts = {
        ms / 1000,                /* seconds */
        (ms % 1000) * 1000 * 1000 /* nano seconds */
    };

    return nanosleep(&ts, NULL);

#elif _BSD_SOURCE ||                             \
    (_XOPEN_SOURCE >= 500 ||                     \
     _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED) && \
        !(_POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)

    /* else fallback to obsolte usleep() */

    return usleep(1000 * ms);

#else

#warning "No millisecond sleep available for this platform!"
    return -1;

#endif
}

static int data_cmp(void *value, void *query)
{
    /*
     * value and query both MUST contain flatdata, so it starts with
     * a uint32_t uid
     */

    return ((*(uint32_t *)value) == (*(uint32_t *)query)) ? 0 : 1;
}

static int thread_func(void *client)
{
    struct {
        uint32_t uid;       // this user's unique ID
        uint32_t obj_count; // the number of objects to receive
    } header;

    uint32_t uid;
    struct node *tmp;
    struct node **cursor;
    enum acs_code code;
    struct acs_sync *self;
    void *buf; // only lock main thread to copy from shared region to here

    assert(initialized);
    assert(client);

    self = client;
    buf = malloc(self->data_thread.flatsize);
    assert(buf);

    while (self->thread_done == 0) {
        /*
         * Send as acs_sync_write's counterpart
         */
        mtx_lock(&self->send_mutex);
        (void)memcpy(buf, self->data_thread.flatdata, self->data_thread.flatsize);

        /*
         * Upon any error, the server expects us to send before it responds, so
         * keep trying
         */
    send:
        if (self->thread_done) {
            break;
        }

        while (1) {
            // now we are free to do network IO without blocking/locking the main thread
            code = acs_send(self->sock, buf, self->data_thread.flatsize);

            if (self->thread_done) {
                goto out;
            }

            if (code == ACS_OK) {
                break;
            }

            // reset UID / wait before retrying to connect
            *(uint32_t *)self->data_main.flatdata = 0;
            (void)millisleep(10);
        }

        /*
         * Recv into list for acs_sync_read_next to get
         */

        // receive header
        code = acs_recv(self->sock, (char *)&header, sizeof(header));
        if (code != ACS_OK) {
            *(uint32_t *)self->data_main.flatdata = 0;
            goto send;
        }

        if (self->thread_done) {
            break;
        }

        // send message to uid which is READONLY from the main thread
        *(uint32_t *)self->data_main.flatdata = header.uid;

        // no we can fill in who is there or not locally
        (void)memset(self->client_bitmap, 0, self->client_bitmap_size);

        // remember the data MUST start with a uint32_t unique ID for the other clients
        // we don't care if we use 'buf' here, it is the correct size to store the data
        for ( ; header.obj_count > 0; header.obj_count--) {
            code = acs_recv(self->sock, buf, self->data_thread.flatsize);
            if (code != ACS_OK) {
                *(uint32_t *)self->data_main.flatdata = 0;
                goto send;
            }

            if (self->thread_done) {
                goto out;
            }

            // record who was received
            uid = *(uint32_t *)buf; // beginning MUST be an int32_t uid
            BIT_SET(self->client_bitmap, uid);

            // now we may put the data into the list
            tmp = list_find(self->recv_data, buf, data_cmp);

            // new client who dis
            if (tmp == NULL) {
                // have the node take buf and we will need to make a new buf
                mtx_lock(&self->recv_mutex);
                list_push_back(self->recv_data, buf);
                mtx_unlock(&self->recv_mutex);

                buf = malloc(self->data_thread.flatsize);
                assert(buf);
            }
            // update existing client
            else {
                // copy buf into the node's version
                mtx_lock(&self->recv_mutex);
                (void)memcpy(tmp->value, buf, self->data_thread.flatsize);
                mtx_unlock(&self->recv_mutex);
            }
        }

        if (self->thread_done) {
            break;
        }

        // look for clients who are disconnected and delete them from the list
        // if a client is in the recv list and their bit is not set/high, then
        // they are disconnected
        mtx_lock(&self->recv_mutex);

        for (cursor = list_iter_begin(self->recv_data);
             !list_iter_done(cursor);
             list_iter_continue(&cursor))
        {
            uid = *(uint32_t *)list_iter_value(cursor);

            // client has DC'ed
            if (BIT_TEST(self->client_bitmap, uid) == 0) {
                tmp = *cursor;
                list_iter_continue(&cursor);
                list_remove(self->recv_data, tmp);
                if (list_iter_done(cursor)) {
                    break;
                }
            }
        }

        mtx_unlock(&self->recv_mutex);
    }

out:
    free(buf);
    return 0;
}

/*
 * Public Function Definitions
 */

enum acs_code acs_sync_init(void)
{
    enum acs_code rv;

    assert(initialized == 0);

    rv = acs_init();
    initialized = 1;
    return rv;
}

void acs_sync_cleanup(void)
{
    assert(initialized == 1);
    acs_cleanup();
    initialized = 0;
}

struct acs_sync *acs_sync_new(const char *host, const char *port, size_t max_clients, void *flatdata, size_t flatsize)
{
    struct acs_sync *self;

    assert(initialized);
    assert(host);
    assert(port);
    assert(flatdata);

    self = calloc(1, sizeof(*self));
    assert(self);

    self->sock = acs_new(host, port);
    assert(self->sock);

    // not doing anything
    self->thread_done = 1;

    /*
     * send stuff
     */

    mtx_init(&self->send_mutex, mtx_plain);

    self->data_main.flatdata = flatdata;
    self->data_main.flatsize = flatsize;

    self->data_thread.flatdata = malloc(flatsize);
    assert(self->data_thread.flatdata);
    self->data_thread.flatsize = flatsize;

    /*
     * recv stuff
     */
    mtx_init(&self->recv_mutex, mtx_plain);
    self->recv_data = list_new(free);
    assert(self->recv_data);
    self->cursor_main = NULL;

    // just enough bits to hold all client info
    self->client_bitmap_size = (size_t)ceil((double)max_clients / 8.0);
    self->client_bitmap = malloc(self->client_bitmap_size);
    assert(self->client_bitmap);

    return self;
}

void acs_sync_del(struct acs_sync *self)
{
    assert(initialized);
    assert(self);

    (void)mtx_unlock(&self->send_mutex);
    if (self->thread_done == 0) {
        self->thread_done = 1;
        (void)thrd_join(self->thread, NULL);
    }

    if (self->recv_data) {
        list_free(self->recv_data);
    }

    if (self->sock) {
        acs_del(self->sock);
    }

    if (self->data_thread.flatdata) {
        free(self->data_thread.flatdata);
    }

    if (self->client_bitmap) {
        free(self->client_bitmap);
    }

    mtx_destroy(&self->send_mutex);
    mtx_destroy(&self->recv_mutex);

    free(self);
}


int acs_sync_run(struct acs_sync *self)
{
    assert(initialized);
    assert(self);
    assert(self->thread_done == 1);

    if (thrd_create(&self->thread, thread_func, self) == thrd_success) {
        self->thread_done = 0;
        return 0;
    }
    return 1;
}

void acs_sync_write(struct acs_sync *self)
{
    assert(initialized);
    assert(self);

    (void)memcpy(self->data_thread.flatdata, self->data_main.flatdata, self->data_thread.flatsize);
    mtx_unlock(&self->send_mutex);
}

void *acs_sync_read_next(struct acs_sync *self)
{
    assert(initialized);
    assert(self);

    if (self->cursor_main == NULL) {
        mtx_lock(&self->recv_mutex);
        self->cursor_main = list_iter_begin(self->recv_data);
    }
    else {
        list_iter_continue(&self->cursor_main);
    }

    if (list_iter_done(self->cursor_main)) {
        mtx_unlock(&self->recv_mutex);
        self->cursor_main = NULL;
        return NULL;
    }

    return list_iter_value(self->cursor_main);
}
