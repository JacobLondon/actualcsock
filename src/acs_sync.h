#ifndef ACS_SYNC_H
#define ACS_SYNC_H

#include "acs.h"

struct acs_sync;

enum acs_code acs_sync_init(void);
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
 */
void acs_sync_write(struct acs_sync *self);

/**
 * You use this function like an iterator reader to copy into your
 * version of the data
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
 * }
 * @endcode
 * 
 * @warning
 *   DO NOT BREAK OUT OF THIS LOOP, CONTINUE THRU THE REST OF IT ELSE DEADLOCK
 */
void *acs_sync_read_next(struct acs_sync *self);

#endif // ACS_SYNC_H
