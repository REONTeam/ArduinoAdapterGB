#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
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
    _Atomic uint32_t bgb_clock;
    _Atomic uint32_t bgb_clock_latch;
    FILE *config;
    int socket;
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

void mobile_board_time_latch(void *user)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    mobile->bgb_clock_latch = mobile->bgb_clock;
}

bool mobile_board_time_check_ms(void *user, unsigned ms)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    bool ret = ((mobile->bgb_clock - mobile->bgb_clock_latch) & 0x7FFFFFFF) >
        (uint32_t)((double)ms * (1 << 21) / 1000);
    return ret;
}

bool mobile_board_tcp_connect(void *user, const unsigned char *host, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;

    char s_host[4 * 4 + 1];
    char s_port[6];
    sprintf(s_host, "%hhu.%hhu.%hhu.%hhu", host[0], host[1], host[2], host[3]);
    sprintf(s_port, "%u", port & 0xFFFF);

    fprintf(stderr, "Connecting to: %s.%s\n", s_host, s_port);
    int sock = socket_connect(s_host, s_port);
    if (sock == -1) {
		fprintf(stderr, "Could not connect (%s:%s):", s_host, s_port);
        socket_perror(NULL);
        return false;
    }

    mobile->socket = sock;
    return true;
}

bool mobile_board_tcp_listen(void *user, const unsigned port)
{
    struct mobile_user *mobile = (struct mobile_user *)user;

    if (mobile->socket == -1) {
        char s_port[6];
        sprintf(s_port, "%u", port & 0xFFFF);

        int sock = socket_bind(s_port);
        if (sock == -1) {
            fprintf(stderr, "Could not bind (%s):", s_port);
            socket_perror(NULL);
            return false;
        }

		if (listen(sock, 1) == -1) {
			socket_perror("listen");
			close(sock);
			return false;
		}

        mobile->socket = sock;
    }

    if (socket_hasdata(mobile->socket, 1000000) <= 0) return false;
	int sock = accept(mobile->socket, NULL, NULL);
	if (sock == -1) {
		socket_perror("accept");
		return false;
	}
	close(mobile->socket);
	mobile->socket = sock;

    return true;
}

void mobile_board_tcp_disconnect(void *user)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
    socket_close(mobile->socket);
    mobile->socket = -1;
}

bool mobile_board_tcp_send(void *user, const void *data, const unsigned size)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
	if (send(mobile->socket, data, size, 0) == -1) {
		socket_perror("send");
		return false;
	}
    return true;
}

int mobile_board_tcp_receive(void *user, void *data)
{
    struct mobile_user *mobile = (struct mobile_user *)user;
#if defined(__unix__)
	ssize_t len = recv(mobile->socket, data, MOBILE_MAX_TCP_SIZE, MSG_DONTWAIT);
#elif defined(__WIN32__)
    if (!socket_hasdata(mobile->socket, 0)) return 0;
	ssize_t len = recv(mobile->socket, data, MOBILE_MAX_TCP_SIZE, 0);
#endif
	if (len != -1) return len;
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		socket_perror("recv");
		return -1;
	}
    return 0;
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
    fprintf(stderr, "%s [-h] [-c config] [address [port]]\n", program_name);
    exit(EXIT_FAILURE);
}

int main(A_UNUSED int argc, char *argv[])
{
    program_name = argv[0];
    setlocale(LC_ALL, "");

    char *host = "127.0.0.1";
    char *port = "8765";

    char *fname_config = "config.bin";

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
        } else {
            fprintf(stderr, "Unknown option: %s\n", *argv);
            show_help();
        }
    }

    if (*argv) {
        host = *argv++;
    }
    if (*argv) {
        port = *argv++;
    }

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

    int sock = socket_connect(host, port);
    if (sock == -1) {
		fprintf(stderr, "Could not connect (%s:%s):", host, port);
        socket_perror(NULL);
        return EXIT_FAILURE;
    }

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&(int){1}, sizeof(int)) == -1) {
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
    mobile->bgb_clock = mobile->bgb_clock_latch = 0;
    mobile->config = config;
    mobile->socket = -1;
    mobile_init(&mobile->adapter, mobile);

    pthread_t mobile_thread;
    int pthread_errno = pthread_create(&mobile_thread, NULL, thread_mobile_loop, mobile);
    if (pthread_errno) {
        fprintf(stderr, "pthread_create: %s\n", strerror(pthread_errno));
        return EXIT_FAILURE;
    }
    bgb_loop(sock, bgb_loop_transfer, bgb_loop_timestamp, mobile);
    pthread_cancel(mobile_thread);

    if (mobile->socket) socket_close(mobile->socket);
    socket_close(sock);

#ifdef __WIN32__
    WSACleanup();
#endif

    fclose(mobile->config);
    free(mobile);

    return EXIT_SUCCESS;
}
