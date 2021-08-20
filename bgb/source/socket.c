#include "socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#if defined(__unix__)
#include <poll.h>
#include <fcntl.h>
#include <netdb.h>
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
    if (!FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, WSAGetLastError(), 0, (LPWSTR)&error, 0, NULL)) {
        putc('\n', stderr);
        return;
    }
    fwprintf(stderr, L" %S", error);
    LocalFree(error);
#endif
}

int socket_straddr(char *res, unsigned res_len, char *res_port, struct sockaddr *addr, socklen_t addrlen)
{
#if defined(__unix__)
    (void)addrlen;
    void *inaddr = NULL;
    unsigned inport = 0;
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        inaddr = &addr4->sin_addr;
        inport = ntohs(addr4->sin_port) & 0xFFFF;
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        inaddr = &addr6->sin6_addr;
        inport = ntohs(addr6->sin6_port) & 0xFFFF;
    } else {
        return -1;
    }

    if (!inet_ntop(addr->sa_family, inaddr, res, res_len)) return -1;
    sprintf(res_port, "%u", inport);
    return 0;
#elif defined(__WIN32__)
    unsigned inport = 0;
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        inport = ntohs(addr4->sin_port) & 0xFFFF;
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        inport = ntohs(addr6->sin6_port) & 0xFFFF;
    } else {
        return -1;
    }

    DWORD res_len_r = res_len;
    if (WSAAddressToStringA(addr, addrlen, NULL, res, &res_len_r)
            == SOCKET_ERROR) {
        return -1;
    }
    sprintf(res_port, "%u", inport);
    return 0;
#endif
}

int socket_hasdata(int socket, int delay)
{
#if defined(__unix__)
    struct pollfd fd = {
        .fd = socket,
        .events = POLLIN | POLLPRI,
    };
    int rc = poll(&fd, 1, delay);
    if (rc == -1) perror("poll");
    return rc;
#elif defined(__WIN32__)
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);
    fd_set exfds;
    FD_ZERO(&exfds);
    FD_SET(socket, &exfds);
    struct timeval tv = {.tv_sec = delay / 1000, .tv_usec = (delay % 1000) * 1000};
    int rc = select(socket + 1, &rfds, NULL, &exfds, &tv);
    if (rc == -1) perror("select");
    return rc;
#endif
}

int socket_isconnected(int socket, int delay)
{
#if defined(__unix__)
    struct pollfd fd = {
        .fd = socket,
        .events = POLLOUT,
    };
    int rc = poll(&fd, 1, delay);
    if (rc == -1) perror("poll");
    if (rc <= 0) return rc;
#elif defined(__WIN32__)
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(socket, &wfds);
    struct timeval tv = {.tv_sec = delay / 1000, .tv_usec = (delay % 1000) * 1000};
    int rc = select(socket + 1, NULL, &wfds, NULL, &tv);
    if (rc == -1) perror("select");
    if (rc <= 0) return rc;
#endif

    int err;
    socklen_t err_len = sizeof(err);
    getsockopt(socket, SOL_SOCKET, SO_ERROR, (char *)&err, &err_len);
    socket_seterror(err);
    if (err) return -1;
    return 1;
}

int socket_setblocking(int socket, int flag)
{
#if defined(__unix__)
    int flags = fcntl(socket, F_GETFL);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }
    flags &= ~O_NONBLOCK;
    if (!flag) flags |= O_NONBLOCK;
    if (fcntl(socket, F_SETFL, flags) == -1) {
        perror("fcntl");
        return -1;
    }
#elif defined(__WIN32__)
    u_long mode = !flag;
    if (ioctlsocket(socket, FIONBIO, &mode) != NO_ERROR) return -1;
#endif
    return 0;
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
        fprintf(stderr, "getaddrinfo: Error %d: ", gai_errno);
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
