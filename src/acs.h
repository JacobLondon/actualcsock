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

int acs_init(const char *host, const char *port);
void acs_cleanup(void);

int acs_send(char *buf, size_t bytes);
int acs_recv(char *buf, size_t bytes);

#endif // ACTUAL_C_SOCKETS_H
