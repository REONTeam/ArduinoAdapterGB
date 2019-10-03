#pragma once

#include <unistd.h>

#if defined(__unix__)

// IWYU pragma: begin_exports
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
// IWYU pragma: end_exports

#define socket_close close

#elif defined(__WIN32__)

#define UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#undef s_host  // Wtf windows?

#define socket_close closesocket

#else
#error "Unsupported OS"
#endif

void socket_perror(const char *func);
int socket_hasdata(int socket, int delay);
int socket_connect(const char *host, const char *port);
int socket_bind(const char *port);
