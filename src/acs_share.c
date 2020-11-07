#include <assert.h>
#include <memory.h>
#include <stdint.h>

#include <tinycthread.h>

#include "acs_share.h"
#include "list.h"

/**
 * Sending to server:
 * 
 *      char[data] -> (serialize) -> acs_data[uid, serialized_data] -> server
 * 
 * 
 * Receiving from server:
 * 
 *      server -> char[return_header, acs_data0[uid, serialized_data], acs_data1[uid, serialized_data], ... ]
 * 
 *      We read the return_header to get our UID and how many other clients' data is in the stream.
 *      We can pass the next sections of serialized_data into (deserialize) and return a pointer
 *      to that data to the user upon subsequent calls to acs_share_get_shared.
 * 
 *      The acs_data will contain other clients' uid, to get ours we will have to use the acs_share to get
 *      it back out as it is saved internally.
 */

struct return_header {
    int32_t uid;    // the receiving user's uid
    uint32_t count; // how many acs_data items are in the packet after this header
};

/**
 * This data structure will have data filled with the user's data upon sending to
 * the server OR other client's data when receiving from the server. Its fields
 * may be accessed with the related getter functions. These will go into the recv_list
 */
struct acs_data {
    int32_t uid; // a client's uid
    char serialized_data[sizeof(int32_t)]; // blank buffer, will allocate the length
    // of the user's data rounded up to align with int32_t.
};

struct data_buffer {
    unsigned char *buf;
    size_t bufsize;
    size_t usage;
};

struct acs_share {
    void *data_user; // struct to share
    size_t data_serialized_size;

    struct acs_data *data_serialized; // pointer to this client's data, beware for thread safety
    struct acs_data *data_serialized_thread; // the thread's COPY of data_serialized

    struct data_buffer buffer_send;
    struct data_buffer buffer_recv;

    struct acs *node; // network node
    char *(*serialize)(void *data, size_t size, size_t *written);
    void *(*deserialize)(void *data, size_t size);
    struct list *recv_list;
    struct node **recv_cursor;
    int32_t uid;        // readonly from main thread
    int thread_running; // flag for whether the thread is thread_running, set to false the thread should exit, thread readonly
    thrd_t thread;
    mtx_t mutex_main2thrd;   // thread can't send data which hasn't been copied to data_serialized_thread yet
    mtx_t mutex_server2thrd; // thread can't write over received data until main thread finishes getting all of it
};

/**
 * Static Variables
 */

static int initialized = 0;

/**
 * Static Function Prototypes
 */

static struct acs_data *acs_data_new(struct acs_share *share, char *buf);
static void acs_data_del(struct acs_data *self);
static size_t acs_data_calc_size(struct acs_data *self);

static int data_buffer_init(struct data_buffer *self, size_t bufsize);
static void data_buffer_cleanup(struct data_buffer *self);

static int thread_loop(void *share);
/** \warning MUST BE CALLED IN mutex_server2thrd CRITICAL SECTION */
static void thread_process_recv(struct acs_share *self);

/**
 * Static Function Definitions
 */

static struct acs_data *acs_data_new(struct acs_share *share, char *buf)
{
    struct acs_data *self;

    assert(initialized);
    assert(share);
    assert(buf);

    /**
     * We will be likely be allocating much longer than acs_data to fit buf.
     * This means that IT MUST BE ALIGNED WITH INT32_T or a crash may occur.
     * 
     * The size is known by \a share because the idea is to share a single
     * data struct over the network. Therefore, share has this info but
     * acs_data doesn't need to.
     */

    self = malloc(sizeof(*self));
    if (!self) {
        return NULL;
    }

    self->uid = -1;
}

static void acs_data_del(struct acs_data *self)
{
    assert(initialized);
    assert(self);
    free(self);
}

static size_t acs_data_calc_size(struct acs_share *self)
{
    assert(initialized);
    assert(self);

    return self->
}

static int data_buffer_init(struct data_buffer *self, size_t bufsize)
{
    struct data_buffer *self;

    assert(initialized);
    assert(self);

    self = malloc(sizeof(*self));
    if (!self) {
        return 0;
    }

    self->buf = malloc(bufsize);
    if (!self->buf) {
        free(self);
        return 0;
    }

    self->bufsize = bufsize;
    self->usage = 0;
}

static void data_buffer_cleanup(struct data_buffer *self)
{
    assert(initialized);
    assert(self);
    if (self->buf) {
        free(self->buf);
    }

    (void)memset(self, 0, sizeof(*self));
    free(self);
}

/**
 * Loop in another thread, if running is set to 0, then we finish
 */
static int thread_loop(void *share)
{
    struct acs_share *self = (struct acs_share *)share;
    enum acs_code rv;

    assert(self);

    while (self->thread_running) {
        
        // ...

        {
            // while this is locked, the data we want to send isn't valid yet
            mtx_lock(&self->mutex_main2thrd);

            // we may send until OK if lost connection
            rv = acs_send(self->node, self->buffer_send.buf, self->buffer_send.bufsize);

            mtx_unlock(&self->mutex_main2thrd);
        }

        // ...

        if (rv == ACS_OK) {
            rv = acs_recv(self->node, self->buffer_recv.buf, self->buffer_recv.bufsize);
            if (rv != ACS_OK) {
                goto skip;
            }

            // only update stuff accessible from main thread here
            mtx_lock(&self->mutex_server2thrd);
            thread_process_recv(self);
            mtx_unlock(&self->mutex_server2thrd);
        }
        else {
    skip:
            self->uid = -1;
        }

        // ...
    }

    return 0;
}

/**
 * Expected format:
 * [return_header, 0 or more acs_data, ...]
 * 
 * \warning
 *      MUST BE CALLED IN mutex_server2thrd CRITICAL SECTION FROM RUNNING THREAD
 */
static void thread_process_recv(struct acs_share *self)
{
    int32_t i;
    unsigned char *bp;
    struct return_header *header = self->buffer_recv.buf;
    struct acs_data *received;
    void *data;

    self->uid = header->uid;

    // traverse the received data, put into the list
    for (i = 0, bp = &self->buffer_recv.buf[sizeof(*header)];
         i < header->count;
         i++, bp = &bp[acs_data_calc_size(self)])
    {
        received = bp;

        data = self->deserialize(received->serialized_data, /* TODO */);

    }

}

/**
 * Public Function Definitions
 */

enum acs_code acs_share_init(void)
{
    enum acs_code rv;

    assert(initialized == 0);

    rv = acs_init();
    initialized = 1;
    return rv;
}

void acs_share_cleanup(void)
{
    assert(initialized == 1);
    acs_cleanup();
    initialized = 0;
}

struct acs_share *acs_share_new(const char *host, const char *port, void *data, size_t serialized_size,
                                char *(*serialize)(void *data, size_t size, size_t *written),
                                void *(*deserialize)(void *data, size_t size))
{
    struct acs_share *self;

    assert(initialized);
    assert(host);
    assert(port);
    assert(data);
    assert(serialize);
    assert(deserialize);

    self = malloc(sizeof(*self));
    if (!self) {
        return NULL;
    }

    self->recv_list = list_new(acs_data_del);
    if (!self->recv_list) {
        free(self);
        return NULL;
    }
    self->recv_cursor = NULL;

    self->node = acs_new(host, port);
    if (!self->node) {
        list_free(self->recv_list);
        free(self);
        return NULL;
    }

    if (!data_buffer_init(&self->buffer_send, serialized_size)) {
        list_free(self->recv_list);
        acs_del(self->node);
        free(self);
        return NULL;
    }

    if (!data_buffer_init(&self->buffer_recv, ))

    self->network_buf_size = network_buf_size;
    self->network_buf_usage = 0;
    self->network_buf = malloc(network_buf_size);
    if (!self->network_buf) {
        list_free(self->recv_list);
        acs_del(self->node);
        free(self);
        return NULL;
    }
    self->serialize = serialize;
    self->deserialize = deserialize;
    self->uid = -1;
    self->thread_running = 0;

    mtx_init(&self->mutex_main2thrd, mtx_plain);
    mtx_init(&self->mutex_server2thrd, mtx_plain);
    mtx_init(&self->mutex_uid, mtx_plain);

    return self;
}

void acs_share_del(struct acs_share *self)
{
    assert(initialized);
    assert(self);

    if (self->thread_running) {
        self->thread_running = 0;
        thrd_join(&self->thread, NULL);
    }

    mtx_destroy(&self->mutex_main2thrd);
    mtx_destroy(&self->mutex_server2thrd);

    if (self->node) {
        acs_del(self->node);
    }

    if (self->recv_list) {
        list_free(self->recv_list);
    }

    if (self->network_buf) {
        free(self->network_buf);
    }

    (void)memset(self, 0, sizeof(*self));
    free(self);
}

int acs_share_run(struct acs_share *self)
{
    assert(initialized);
    assert(self);
    assert(self->thread_running == 0);

    self->thread_running = 1;
    return (thrd_create(&self->thread, thread_loop, self) == thrd_success);
}

void acs_share_update(struct acs_share *self)
{
    assert(initialized);
    assert(self);
    assert(self->thread_running);

    mtx_lock(&self->mutex_main2thrd);

    // copy self->data to thread's section
    (void)memcpy(self->data_serialized_thread, self->data_serialized, acs_data_calc_size(self->data_serialized));

    mtx_unlock(&self->mutex_main2thrd);
}

struct acs_data *acs_share_get_shared(struct acs_share *self)
{
    assert(initialized);
    assert(self);
    assert(self->thread_running);

    /**
     * Keep calling this until NULL is returned so unlock the data
     */

    if (self->recv_cursor == NULL) {
        mtx_lock(&self->mutex_server2thrd);
        self->recv_cursor = list_iter_begin(self->recv_list);
    }
    else {
        list_iter_continue(&self->recv_cursor);
    }

    if (list_iter_done(self->recv_cursor)) {
        mtx_unlock(&self->mutex_server2thrd);
        self->recv_cursor = NULL;
        return NULL;
    }

    return list_iter_value(self->recv_cursor);
}

int acs_share_get_uid(struct acs_share *self)
{
    assert(initialized);
    assert(self);
    return self->uid;
}

int acs_data_get_uid(struct acs_data *self)
{
    assert(initialized);
    assert(self);
    return self->uid;
}

void *acs_data_get_buf(struct acs_data *self)
{
    assert(initialized);
    assert(self);
    return self->serialized_data;
}
