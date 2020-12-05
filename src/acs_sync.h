#ifndef ACS_SYNC_H
#define ACS_SYNC_H

#include "acs.h"

struct acs_sync;

/**
 * When acs_sync_get_state returns the corresponding enum,
 * the described operations are allowed
 */
enum acs_sync_state {
    ACS_SYNC_BUSY,  /** System is NOT ready for reading or writing */
    ACS_SYNC_READ,  /** acs_sync_read_next is allowed */
    ACS_SYNC_WRITE, /** acs_sync_write is allowed */
};

/**
 * Initialize the library
 */
enum acs_code acs_sync_init(void);

/**
 * Clean up the library
 */
void acs_sync_cleanup(void);

/**
 * @note
 *   @a flatdata MUST MUST MUST be flat and the first item in the structure MUST be
 *   an "int32_t uid;" This member will be READONLY after initialization. During
 *   initialization, you MUST set it to 0
 * 
 * @code
 * struct mystruct {
 *   uint32_t uid;
 *   ... flat data ...
 * }
 * @endcode
 */
struct acs_sync *acs_sync_new(const char *host, const char *port, size_t max_clients, void *flatdata, size_t flatsize);

/**
 * Join the thread and free all heap memory
 */
void acs_sync_del(struct acs_sync *self);

/**
 * Begin comms in other thread, return 0 on success, 1 on failure
 */
int acs_sync_run(struct acs_sync *self);

/**
 * Tell the thread to send whatever is in your flatdata. Please note
 * that you must fill flatdata up whenever you are prepared to send.
 * 
 * You only call this function when you are prepared to send the flatdata.
 * Calling this function will unblock the thread to send your data and receive
 * the most recent client data for extraction with @see acs_sync_read_next
 * 
 * @warning
 *   ONLY CALL THIS FUNCTION IF THE STATE IS ACS_SYNC_WRITE
 */
void acs_sync_write(struct acs_sync *self);

/**
 * You use this function like an iterator reader to copy into your
 * version of the data. It will return NULL when there is no more
 * data.
 * 
 * @code
 * struct mystruct global_items[16];
 * int i = 0;
 * struct mystruct *p;
 * 
 * for (p = acs_sync_read_next(my_acs_sync);
 *      p != NULL;
 *      p = acs_sync_read_next(my_acs_sync))
 * {
 *   if (i == 16) continue;
 *   memcpy(&global_items[i++], p, sizeof(struct mystruct));
 *   // or index into items with the UID
 *   //memcpy(&global_items[p->uid], p, sizeof(struct mystruct));
 * }
 * @endcode
 * 
 * @warning
 *   DO NOT BREAK OUT OF THIS LOOP, CONTINUE THRU THE REST OF IT ELSE DEADLOCK
 * @warning
 *   ONLY CALL THIS FUNCTION IF THE STATE IS ACS_SYNC_READ
 */
void *acs_sync_read_next(struct acs_sync *self);

/**
 * Get the current state. May be polled at any time.
 */
enum acs_sync_state acs_sync_get_state(struct acs_sync *self);

#endif // ACS_SYNC_H
