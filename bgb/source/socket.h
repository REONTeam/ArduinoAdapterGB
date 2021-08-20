#pragma once

#include <unistd.h>

#if defined(__unix__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#elif defined(__WIN32__)
#define UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#error "Unsupported OS"
#endif

#if defined(__unix__)
#define socket_close close
#define socket_geterror() errno
#define socket_seterror(e) errno = (e)
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#define SOCKET_EINPROGRESS EINPROGRESS
#define SOCKET_EALREADY EALREADY
#elif defined(__WIN32__)
#define socket_close closesocket
#define socket_geterror() WSAGetLastError()
#define socket_seterror(e) WSASetLastError(e)
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCKET_EINPROGRESS WSAEINPROGRESS
#define SOCKET_EALREADY WSAEALREADY
#endif

void socket_perror(const char *func);
int socket_straddr(char *res, unsigned res_len, char *res_port, struct sockaddr *addr, socklen_t addrlen);
int socket_hasdata(int socket, int delay);
int socket_isconnected(int socket, int delay);
int socket_setblocking(int socket, int flag);
int socket_connect(const char *host, const char *port);
