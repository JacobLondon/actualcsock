#ifndef ACTUAL_C_SOCKETS_H
#define ACTUAL_C_SOCKETS_H

/**
 * ACS just client sockets, use something
 * easier for servers like Python's socketserver
 * 
 * Works on Windows (Visual C)
 * Works on Unix-based
 */

#include <stddef.h> // size_t

struct acs;

enum acs_code {
    ACS_OK,
    ACS_ERROR,
    ACS_RESET,
};

enum acs_code acs_init(void);
void acs_cleanup(void);

/**
 * Create an acs struct to connect with, acs_init must have been called
 */
struct acs *acs_new(const char *host, const char *port);

/**
 * Delete an acs struct
 */
void acs_del(struct acs *self);

/**
 * Send \a bytes of \a buf
 * 
 * \return
 *       0 success
 *      -1 acs_dial error
 *      -2 send error
 */
enum acs_code acs_send(struct acs *self, char *buf, size_t bytes);
enum acs_code acs_recv(struct acs *self, char *buf, size_t bytes);

#endif // ACTUAL_C_SOCKETS_H
