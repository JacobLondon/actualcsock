#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "acs.h"

#ifdef _WIN32

#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#else // UNIX-based

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#endif // _WIN32

static int initialized = 0;

struct acs {
#ifdef _WIN32
    SOCKET fd;
#else
    int fd;
#endif
    const char *host;
    const char *port;
};

/**
 * Dial a server
 */
static enum acs_code acs_dial(
    #ifdef _WIN32
        SOCKET *clientfd,
    #else
        int *clientfd,
    #endif
    const char *host,
    const char *port);

enum acs_code acs_init(void)
{
    #ifdef _WIN32
        WSADATA wsa;
        int rv;
    #endif

    assert(!initialized);

    #ifdef _WIN32
        // Windows being extra...
        rv = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rv != 0) {
            (void)fprintf(stderr, "WSAStartup: Error: %d\n", WSAGetLastError());
            return ACS_ERROR;
        }
    #endif // _WIN32

    initialized = 1;
    return ACS_OK;
}

void acs_cleanup(void)
{
    assert(initialized);
    initialized = 0;
}

struct acs *acs_new(const char *host, const char *port)
{
    struct acs *self;

    assert(initialized);
    assert(host);
    assert(port);

    self = malloc(sizeof(*self));
    if (!self) {
        return NULL;
    }

    #ifdef _WIN32
        self->fd = SOCKET_ERROR;
    #else
        self->fd = -1;
    #endif
    self->host = host;
    self->port = port;

    return self;
}

void acs_del(struct acs *self)
{
    assert(initialized);
    assert(self);

    #ifdef _WIN32
        if (self->fd != INVALID_SOCKET) {
            (void)closesocket(self->fd);
            self->fd = SOCKET_ERROR;
        }
        WSACleanup();
    #else
        if (self->fd != -1) {
            (void)close(self->fd);
            self->fd = -1;
        }
    #endif
    free(self);
}

enum acs_code acs_send(struct acs *self, char *buf, size_t bytes)
{
    int rv;
    long offset = 0;

    assert(initialized);
    assert(self);
    assert(buf);

    // dial host if ever not connected
    if (
        #ifdef _WIN32
            self->fd == SOCKET_ERROR
        #else
            self->fd == -1
        #endif
        )
    {
        rv = acs_dial(&self->fd, self->host, self->port);
        if (rv != 0) {
            return ACS_ERROR;
        }
    }

    while (1) {
        rv = send(self->fd, &buf[offset], bytes - offset, 0);

        // check for failure
        #ifdef _WIN32
            if (rv == SOCKET_ERROR) {
                #ifndef NDEBUG
                    (void)fprintf(stderr, "send: Error: %d\n", WSAGetLastError());
                #endif
                (void)closesocket(self->fd);
                self->fd = SOCKET_ERROR;
                return ACS_ERROR;
            }
        #else
            if (rv == -1) {
                #ifndef NDEBUG
                    (void)fprintf(stderr, "send: Error: %s\n", strerror(errno));
                #endif
                (void)close(self->fd);
                self->fd = -1;
                return ACS_ERROR;
            }
        #endif

        offset += rv;

        if (((long)bytes) - offset <= 0) {
            break;
        }
    }
    return ACS_OK;
}

enum acs_code acs_recv(struct acs *self, char *buf, size_t bytes)
{
    int rv;

    assert(initialized);
    assert(self);
    assert(buf);

    // dial host if ever not connected
    if (
        #ifdef _WIN32
            self->fd == SOCKET_ERROR
        #else
            self->fd == -1
        #endif
        )
    {
        rv = acs_dial(&self->fd, self->host, self->port);
        if (rv != 0) {
            return ACS_ERROR;
        }
    }

    rv = recv(self->fd, buf, bytes, 0);
    if (rv > 0) {
        return ACS_OK;
    }
    else if (rv == 0) {
        #ifndef NDEBUG
            (void)fprintf(stderr, "recv: Connection closed\n");
        #endif
        #ifdef _WIN32
            (void)closesocket(self->fd);
            self->fd = SOCKET_ERROR;
        #else
            (void)close(self->fd);
            self->fd = -1;
        #endif
        return ACS_RESET;
    }
    else {
        #ifndef NDEBUG
            #ifdef _WIN32
                (void)fprintf(stderr, "recv: Error: %d\n", WSAGetLastError());
            #else
                (void)fprintf(stderr, "recv: Error: %s\n", strerror(errno));
            #endif // _WIN32
        #endif
        #ifdef _WIN32
            (void)closesocket(self->fd);
            self->fd = SOCKET_ERROR;
        #else
            (void)close(self->fd);
            self->fd = -1;
        #endif
        return ACS_ERROR;
    }
}

static enum acs_code acs_dial(
    #ifdef _WIN32
        SOCKET *clientfd,
    #else
        int *clientfd,
    #endif
    const char *host,
    const char *port)
{
    int rv;
    struct addrinfo *ai, *aip;
    struct addrinfo hints;
    #ifdef _WIN32
        SOCKET sockfd = INVALID_SOCKET;
    #else
        int sockfd = -1;
    #endif // _WIN32

    assert(clientfd);
    assert(host);
    assert(port);

    (void)memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rv = getaddrinfo(host, port, &hints, &ai);
    if (rv != 0) {
        #ifndef NDEBUG
            (void)fprintf(stderr, " getaddrinfo: Error: %d\n", rv);
        #endif
        return ACS_ERROR;
    }

    // connect UP!
    for (aip = ai; aip != NULL; aip = aip->ai_next) {
        sockfd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
        #ifdef _WIN32
            if (sockfd == INVALID_SOCKET) {
                #ifndef NDEBUG
                    (void)fprintf(stderr, "socket: Error: %d\n", WSAGetLastError());
                #endif
                return ACS_ERROR;
            }
        #else
            if (sockfd == -1) {
                #ifndef NDEBUG
                    (void)fprintf(stderr, "socket: Error: %s\n", strerror(errno));
                #endif
                return ACS_ERROR;
            }
        #endif

        // connect to server
        rv = connect(sockfd, aip->ai_addr, (int)aip->ai_addrlen);
        #ifdef _WIN32
            if (rv == SOCKET_ERROR) {
                (void)closesocket(sockfd);
                sockfd = INVALID_SOCKET;
                continue;
            }
        #else
            if (rv == -1) {
                (void)close(sockfd);
                sockfd = -1;
                continue;
            }
        #endif // _WIN32

        // found it
        break;
    }

    freeaddrinfo(ai);

    // did we reach end of loop without finding it?
    #ifdef _WIN32
        if (sockfd == INVALID_SOCKET) {
            #ifndef NDEBUG
                (void)fprintf(stderr, "Unable to connect to server\n");
            #endif
            return ACS_ERROR;
        }
    #else
        if (sockfd == -1) {
            #ifndef NDEBUG
                (void)fprintf(stderr, "Unable to connect to server\n");
            #endif
            return ACS_ERROR;
        }
    #endif

    *clientfd = sockfd;
    return ACS_OK;
}
