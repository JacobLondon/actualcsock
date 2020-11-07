/**
 * Utilize ACS library to build a higher level library
 * for sharing data with a server that can have many
 * clients who are also sharing the same type of data.
 */

#ifndef ACS_SHARE_H
#define ACS_SHARE_H

#include "acs.h"

struct acs_share;
struct acs_data;

/**
 * Initialize the library
 */
enum acs_code acs_share_init(void);

/**
 * Cleanup the library
 */
void acs_share_cleanup(void);

/**
 * Create a new acs share util by binding \a data its \a size and a way to \a serialize that data
 * to and from a server. The serialize function takes the \a data and \a size and outs the
 * serialize length as \a written
 * 
 * \param host
 *      string IPV4 address
 * \param port
 *      16 bit port number as a string
 * \param data
 *      data to share with the server
 * \param size
 *      the sizeof \a data
 * \param network_buf_size
 *      the size in bytes of the network buffer. This will be a fixed size, and will
 *      assume to be able to fit ALL of every other client's serialized data and headers
 *      all at the same time. Adjust accordingly and carefully.
 * \param serialize
 *      callback to flatten \a data returning where the flat data resides and how long it is
 * \param deserialize
 *      callback to unflatten received data into the type of \a data
 */
struct acs_share *acs_share_new(const char *host, const char *port, void *data, size_t size,
                                size_t network_buf_size,
                                char *(*serialize)(void *data, size_t size, size_t *written),
                                void *(*deserialize)(void *data, size_t size));

/**
 * Delete the acs share struct
 */
void acs_share_del(struct acs_share *self);

/**
 * Run the acs share in another thread, reading and writing to and from the server
 * respectively
 */
int acs_share_run(struct acs_share *self);

/**
 * Call every time the other thread should send data to the server, most likely usefully
 * called in a loop every some period of time, as no calls to this will block from receiving
 * from the server.
 */
void acs_share_update(struct acs_share *self);

/**
 * Read our copy of the shared data from the server, which shall not include our own data.
 * Repeated calls will get the next item until NULL
 * 
 * \param self
 *      the acs_share object to interact with
 * \return
 *      the data from the server, the same type as the init data type
 *      NULL when finished
 * \warning
 *      the thread running acs_share_run will do NOTHING until this function
 *      finishes traversing all the shared data, so make sure you always finish
 *      traversing ALL of the data
 * 
 * \code
 * struct mystruct *dataptr;
 * for (dataptr = acs_share_get_shared(my_acs_share); dataptr; dataptr = acs_share_get_shared(my_acs_share)) {}
 * \endcode
 */
struct acs_data *acs_share_get_shared(struct acs_share *self);

/**
 * Get the UID given to us by the server, -1 if no connection is active.
 */
int acs_share_get_uid(struct acs_share *self);

/**
 * Get a UID from network data
 */
int acs_data_get_uid(struct acs_data *self);

/**
 * Get the data pointer from an acs_data
 */
void *acs_data_get_buf(struct acs_data *self);

#endif // ACS_SHARE_H
