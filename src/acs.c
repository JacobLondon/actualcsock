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
#ifdef _WIN32
    static SOCKET fd = SOCKET_ERROR;
#else
    static int fd = -1;
#endif

static int acs_dial(
    #ifdef _WIN32
        SOCKET *clientfd,
    #else
        int *clientfd,
    #endif
    const char *host,
    const char *port);

int acs_init(const char *host, const char *port)
{
    #ifdef _WIN32
        WSADATA wsa;
        int rv;
    #endif

    assert(!initialized);
    assert(host);
    assert(port);

    #ifdef _WIN32
        // Windows being extra...
        rv = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rv != 0) {
            (void)fprintf(stderr, "WSAStartup: Error: %d\n", WSAGetLastError());
            return 1;
        }
    #endif // _WIN32

    // dial that host
    if (acs_dial(&fd, host, port) != 0) {
        return 1;
    }

    initialized = 1;
    return 0;
}

void acs_cleanup(void)
{
    assert(initialized);
    #ifdef _WIN32
        if (fd != INVALID_SOCKET) {
            (void)closesocket(fd);
            fd = SOCKET_ERROR;
        }
        WSACleanup();
    #else
        if (fd != -1) {
            (void)close(fd);
            fd = -1;
        }
    #endif
    initialized = 0;
}

int acs_send(char *buf, size_t bytes)
{
    int rv;

    assert(initialized);
    assert(buf);

    rv = send(fd, buf, bytes, 0);
    #ifdef _WIN32
        if (rv == SOCKET_ERROR) {
            (void)fprintf(stderr, "send: Error: %d\n", WSAGetLastError());
            return -1;
        }
    #else
        if (rv == -1) {
            (void)fprintf(stderr, "send: Error: %s\n", strerror(errno));
            return -1;
        }
    #endif

    return rv;
}

int acs_recv(char *buf, size_t bytes)
{
    int rv;

    assert(initialized);
    assert(buf);

    rv = recv(fd, buf, bytes, 0);
    if (rv > 0) {
        return 1;
    }
    else if (rv == 0) {
        (void)fprintf(stderr, "recv: Connection closed\n");
        return 0;
    }
    else {
        #ifdef _WIN32
            (void)fprintf(stderr, "recv: Error: %d\n", WSAGetLastError());
        #else
            (void)fprintf(stderr, "recv: Error: %s\n", strerror(errno));
        #endif // _WIN32
        return -1;
    }
}

static int acs_dial(
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
        (void)fprintf(stderr, " getaddrinfo: Error: %d\n", rv);
        return 1;
    }

    // connect UP!
    for (aip = ai; aip != NULL; aip = aip->ai_next) {
        sockfd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
        #ifdef _WIN32
            if (sockfd == INVALID_SOCKET) {
                (void)fprintf(stderr, "socket: Error: %d\n", WSAGetLastError());
                return 2;
            }
        #else
            if (sockfd == -1) {
                (void)fprintf(stderr, "socket: Error: %s\n", strerror(errno));
                return 2;
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
            (void)fprintf(stderr, "Unable to connect to server\n");
            return 3;
        }
    #else
        if (sockfd == -1) {
            (void)fprintf(stderr, "Unable to connect to server\n");
            return 3;
        }
    #endif

    *clientfd = sockfd;
    return 0;
}
