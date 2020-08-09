#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <pthread.h>
#include "libmobile/mobile.h"

#include "socket.h"
#include "bgblink.h"

#ifdef __GNUC__
#define A_UNUSED __attribute__((unused))
#else
#define A_UNUSED
#endif

#include "libmobile/debug_cmd.h"  // IWYU pragma: keep

struct mobile_user {
    pthread_mutex_t mutex_serial;
    pthread_mutex_t mutex_cond;
    pthread_cond_t cond;
    struct mobile_adapter adapter;
    enum mobile_action action;
    FILE *config;
    _Atomic uint32_t bgb_clock;
    _Atomic uint32_t bgb_clock_latch[MOBILE_MAX_TIMERS];
    int sockets[MOBILE_MAX_CONNECTIONS];
};

void mobile_board_serial_disable(void *user)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    pthread_mutex_lock(&mobile->mutex_serial);
}

void mobile_board_serial_enable(void *user)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    pthread_mutex_unlock(&mobile->mutex_serial);
}

bool mobile_board_config_read(void *user, void *dest, const uintptr_t offset, const size_t size)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    fseek(mobile->config, offset, SEEK_SET);
    return fread(dest, 1, size, mobile->config) == size;
}

bool mobile_board_config_write(void *user, const void *src, const uintptr_t offset, const size_t size)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    fseek(mobile->config, offset, SEEK_SET);
    return fwrite(src, 1, size, mobile->config) == size;
}

void mobile_board_time_latch(void *user, enum mobile_timers timer)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    mobile->bgb_clock_latch[timer] = mobile->bgb_clock;
}

bool mobile_board_time_check_ms(void *user, enum mobile_timers timer, unsigned ms)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    bool ret = (
        (mobile->bgb_clock - mobile->bgb_clock_latch[timer]) & 0x7FFFFFFF) >
        (uint32_t)((double)ms * (1 << 21) / 1000);
    return ret;
}

bool mobile_board_tcp_connect(void *user, unsigned conn, const unsigned char *host, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        socket_perror("socket");
        return false;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    memcpy(&addr.sin_addr.s_addr, host, 4);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        char s_host[4 * 4 + 1];
        char s_port[6];
        sprintf(s_host, "%u.%u.%u.%u", host[0], host[1], host[2], host[3]);
        sprintf(s_port, "%u", port & 0xFFFF);
        fprintf(stderr, "Could not connect (%s:%s):", s_host, s_port);
        socket_perror(NULL);
        socket_close(sock);
        return false;
    }

    mobile->sockets[conn] = sock;
    return true;
}

bool mobile_board_tcp_listen(void *user, unsigned conn, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        socket_perror("socket");
        return false;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        socket_perror("bind");
        socket_close(sock);
        return false;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            (char *)&(int){1}, sizeof(int)) == -1) {
        socket_close(sock);
        return -1;
    }

    if (listen(sock, 1) == -1) {
        socket_perror("listen");
        socket_close(sock);
        return false;
    }

    mobile->sockets[conn] = sock;
    return true;
}

bool mobile_board_tcp_accept(void *user, unsigned conn)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    if (socket_hasdata(mobile->sockets[conn], 1000000) <= 0) return false;
    int sock = accept(mobile->sockets[conn], NULL, NULL);
    if (sock == -1) {
        socket_perror("accept");
        return false;
    }
    socket_close(mobile->sockets[conn]);
    mobile->sockets[conn] = sock;
    return true;
}

void mobile_board_tcp_disconnect(void *user, unsigned conn)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    socket_close(mobile->sockets[conn]);
    mobile->sockets[conn] = -1;
}

bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned size)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    if (send(mobile->sockets[conn], data, size, 0) == -1) {
        socket_perror("send");
        return false;
    }
    return true;
}

int mobile_board_tcp_recv(void *user, unsigned conn, void *data, unsigned length)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    if (!socket_hasdata(mobile->sockets[conn], 0)) return 0;
    ssize_t len;
    if (data) {
        len = recv(mobile->sockets[conn], data, length, 0);
    } else {
        char c;
        len = recv(mobile->sockets[conn], &c, 1, MSG_PEEK);
        if (len == 1) return 0;
    }
    if (len == 0) return -2;  // End of file (disconnect received)
    if (len == -1) socket_perror("recv");
    return len;
}

bool mobile_board_udp_open(void *user, unsigned conn, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        socket_perror("socket");
        return false;
    }

    // Bind to requested port, random port if 0.
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        socket_perror("bind");
        socket_close(sock);
        return false;
    }
    mobile->sockets[conn] = sock;
    return true;
}

bool mobile_board_udp_sendto(void *user, unsigned conn, const void *data, const unsigned size, const unsigned char *host, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    memcpy(&addr.sin_addr.s_addr, host, 4);
    if (sendto(mobile->sockets[conn], data, size, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        socket_perror("send");
        return false;
    }
    return true;
}

int mobile_board_udp_recvfrom(void *user, unsigned conn, void *data, unsigned length, unsigned char *host, unsigned *port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    if (!socket_hasdata(mobile->sockets[conn], 0)) return 0;

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    ssize_t len;
    if (data) {
        len = recvfrom(mobile->sockets[conn], data, length, 0, (struct sockaddr *)&addr, &addr_len);
    } else {
        char c;
        len = recvfrom(mobile->sockets[conn], &c, 1, MSG_PEEK, (struct sockaddr *)&addr, &addr_len);
    }
    if (host && port) {
        if (addr_len != sizeof(addr)) {
            memset(host, 0, 4);
            *port = 0;
        } else {
            memcpy(host, &addr.sin_addr.s_addr, 4);
            *port = ntohs(addr.sin_port);
        }
    }
    if (len == 0) return -1;
    if (len == -1) socket_perror("recv");
    return len;
}

void mobile_board_udp_close(void *user, unsigned conn)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    socket_close(mobile->sockets[conn]);
    mobile->sockets[conn] = -1;
}

void *thread_mobile_loop(void *user)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    pthread_mutex_lock(&mobile->mutex_cond);
    for (;;) {
        // Process actions sent by bgb_loop_action
        while (mobile->action == MOBILE_ACTION_NONE) {
            pthread_cond_wait(&mobile->cond, &mobile->mutex_cond);
        }
        mobile_action_process(&mobile->adapter, mobile->action);
        mobile->action = MOBILE_ACTION_NONE;
        fflush(stdout);
    }
    pthread_mutex_unlock(&mobile->mutex_cond);
}

void bgb_loop_action(struct mobile_user *mobile)
{
    // If the thread isn't doing anything, queue up the next action.
    if (pthread_mutex_trylock(&mobile->mutex_cond) != 0) return;
    if (mobile->action == MOBILE_ACTION_NONE) {
        enum mobile_action action = mobile_action_get(&mobile->adapter);

        // MOBILE_ACTION_RESET_SERIAL is not relevant to an emulator,
        //   since the serial connection can't desync.
        if (action != MOBILE_ACTION_NONE &&
                action != MOBILE_ACTION_RESET_SERIAL) {
            mobile->action = action;
            pthread_cond_signal(&mobile->cond);
        }
    }
    pthread_mutex_unlock(&mobile->mutex_cond);
}

unsigned char bgb_loop_transfer(void *user, unsigned char c)
{
    // Transfer a byte over the serial port
    struct mobile_user *mobile = (struct mobile_user *)user;
    pthread_mutex_lock(&mobile->mutex_serial);
    c = mobile_transfer(&mobile->adapter, c);
    pthread_mutex_unlock(&mobile->mutex_serial);
    bgb_loop_action(mobile);
    return c;
}

void bgb_loop_timestamp(void *user, uint32_t t)
{
    // Update the timestamp sent by the emulator
    struct mobile_user *mobile = (struct mobile_user *)user;
    mobile->bgb_clock = t;
    bgb_loop_action(mobile);
}

char *program_name;

void show_help(void)
{
    fprintf(stderr, "%s [-h] [-c config] [host [port]]\n", program_name);
    exit(EXIT_FAILURE);
}

int main(A_UNUSED int argc, char *argv[])
{
    program_name = argv[0];
    setlocale(LC_ALL, "");

    char *host = "127.0.0.1";
    char *port = "8765";

    char *fname_config = "config.bin";

    struct mobile_adapter_config adapter_config = MOBILE_ADAPTER_CONFIG_DEFAULT;

    while (*++argv) {
        if ((*argv)[0] != '-' || strcmp(*argv, "--") == 0) {
            break;
        } else if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
            show_help();
        } else if (strcmp(*argv, "-c") == 0 || strcmp(*argv, "--config") == 0) {
            if (!argv[1]) {
                fprintf(stderr, "Missing parameter: %s\n", argv[0]);
                show_help();
            }
            fname_config = argv[1];
            argv += 1;
        } else if (strcmp(*argv, "--p2p_port") == 0) {
            if (!argv[1]) {
                fprintf(stderr, "Missing parameter: %s\n", argv[0]);
                show_help();
            }
            adapter_config.p2p_port = strtol(argv[1], NULL, 0);
            argv += 1;
        } else if (strcmp(*argv, "--device") == 0) {
            if (!argv[1]) {
                fprintf(stderr, "Missing parameter: %s\n", argv[0]);
                show_help();
            }
            adapter_config.device = strtol(argv[1], NULL, 0);
            argv += 1;
        } else if (strcmp(*argv, "--unmetered") == 0) {
            adapter_config.unmetered = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", *argv);
            show_help();
        }
    }

    if (*argv) host = *argv++;
    if (*argv) port = *argv++;

    FILE *config = fopen(fname_config, "r+b");
    if (!config) config = fopen(fname_config, "w+b");
    if (!config) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fseek(config, 0, SEEK_END);

    // Make sure config file is at least MOBILE_CONFIG_SIZE bytes big
    for (int i = ftell(config); i < MOBILE_CONFIG_SIZE; i++) {
        fputc(0, config);
    }

#ifdef __WIN32__
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return EXIT_FAILURE;
    }
#endif

    int bgb_sock = socket_connect(host, port);
    if (bgb_sock == -1) {
        fprintf(stderr, "Could not connect (%s:%s):", host, port);
        socket_perror(NULL);
        return EXIT_FAILURE;
    }

    if (setsockopt(bgb_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&(int){1}, sizeof(int)) == -1) {
        socket_perror("setsockopt");
        return EXIT_FAILURE;
    }

    struct mobile_user *mobile = malloc(sizeof(struct mobile_user));
    if (!mobile) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    mobile->mutex_serial = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    mobile->mutex_cond = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    mobile->cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    mobile->action = MOBILE_ACTION_NONE;
    mobile->config = config;
    mobile->bgb_clock = 0;
    for (int i = 0; i < MOBILE_MAX_TIMERS; i++) mobile->bgb_clock_latch[i] = 0;
    for (int i = 0; i < MOBILE_MAX_CONNECTIONS; i++) mobile->sockets[i] = -1;
    mobile_init(&mobile->adapter, mobile, &adapter_config);

    pthread_t mobile_thread;
    int pthread_errno = pthread_create(&mobile_thread, NULL, thread_mobile_loop, mobile);
    if (pthread_errno) {
        fprintf(stderr, "pthread_create: %s\n", strerror(pthread_errno));
        return EXIT_FAILURE;
    }
    bgb_loop(bgb_sock, bgb_loop_transfer, bgb_loop_timestamp, mobile);
    pthread_cancel(mobile_thread);

    for (unsigned i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
        if (mobile->sockets[i] != -1) socket_close(mobile->sockets[i]);
    }
    socket_close(bgb_sock);

#ifdef __WIN32__
    WSACleanup();
#endif

    fclose(mobile->config);
    free(mobile);

    return EXIT_SUCCESS;
}
