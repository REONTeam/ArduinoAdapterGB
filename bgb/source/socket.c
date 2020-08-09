#include "socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#if defined(__unix__)
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#elif defined(__WIN32__)
#define UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#error "Unsupported OS"
#endif

void socket_perror(const char *func)
{
    if (func) fprintf(stderr, "%s:", func);
#if defined(__unix__)
    char error[0x100];
    if (strerror_r(errno, error, sizeof(error))) {
        putc('\n', stderr);
        return;
    }
    fprintf(stderr, " %s\n", error);
#elif defined(__WIN32__)
    LPWSTR error = NULL;
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                WSAGetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&error,
                0,
                NULL)) {
        putc('\n', stderr);
        return;
    }
    fwprintf(stderr, L" %s\n", error);
    LocalFree(error);
#endif
}

int socket_hasdata(int socket, int delay)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);
    fd_set exfds;
    FD_ZERO(&exfds);
    FD_SET(socket, &exfds);
    struct timeval tv = {.tv_sec = delay / 1000000, .tv_usec = delay % 1000000};
    int sel = select(socket + 1, &rfds, NULL, &exfds, &tv);
    if (sel == -1) {
        perror("select");
    }
    return sel;
}

int socket_connect(const char *host, const char *port)
{
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
	};
	struct addrinfo *result;
	int gai_errno = getaddrinfo(host, port, &hints, &result);
	if (gai_errno) {
#if defined(__unix__)
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_errno));
#elif defined(__WIN32__)
        fprintf(stderr, "getaddrinfo: Error %d", gai_errno);
        socket_perror(NULL);
#endif
		return -1;
	}

	int sock;
	struct addrinfo *info;
	for (info = result; info; info = info->ai_next) {
        errno = 0;
        sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
		if (sock == -1) continue;
		if (connect(sock, info->ai_addr, info->ai_addrlen) == 0) break;
		socket_close(sock);
	}
	freeaddrinfo(result);
	if (!info) return -1;
    return sock;
}
