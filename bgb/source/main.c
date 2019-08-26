#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "libmobile/mobile.h"

#if defined(__unix__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#elif defined(__WIN32__)
#include <winsock2.h>
#endif

#include "libmobile/debug_cmd.h"

enum bgb_cmd {
    BGB_CMD_VERSION = 1,
    BGB_CMD_JOYPAD = 101,
    BGB_CMD_SYNC1 = 104,
    BGB_CMD_SYNC2,
    BGB_CMD_SYNC3,
    BGB_CMD_STATUS = 108,
    BGB_CMD_WANTDISCONNECT
};

struct bgb_packet {
    unsigned char cmd;
    unsigned char b2;
    unsigned char b3;
    unsigned char b4;
    uint32_t timestamp;
};

volatile uint32_t bgb_clock;
int bgb_sock;

int bgb_write(struct bgb_packet *buf)
{
    ssize_t num = send(bgb_sock, (char *)buf, sizeof(struct bgb_packet), 0);
    if (num == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    return num == sizeof(struct bgb_packet);
}

int bgb_read(struct bgb_packet *buf)
{
    ssize_t num = recv(bgb_sock, (char *)buf, sizeof(struct bgb_packet), 0);
    if (num == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    return num == sizeof(struct bgb_packet);
}

void bgb_loop(void)
{
    struct bgb_packet packet;

    unsigned char transfer_last = 0xD2;
    unsigned char transfer_cur;

    packet.cmd = BGB_CMD_VERSION;
    packet.b2 = 1;
    packet.b3 = 4;
    packet.b4 = 0;
    packet.timestamp = 0;
    if (!bgb_write(&packet)) return;

    bool set_status = false;

    for (;;) {
        if (!bgb_read(&packet)) return;

        switch (packet.cmd) {
        case BGB_CMD_VERSION:
            packet.cmd = BGB_CMD_STATUS;
            packet.b2 = 3;
            packet.b3 = 0;
            packet.b4 = 0;
            packet.timestamp = 0;
            if (!bgb_write(&packet)) return;
            break;

        case BGB_CMD_JOYPAD:
            // Not relevant
            break;

        case BGB_CMD_SYNC1:
            packet.cmd = BGB_CMD_SYNC2;
            transfer_cur = mobile_transfer(packet.b2);
            packet.b2 = transfer_last;
            transfer_last = transfer_cur;
            packet.b3 = 0x80;
            packet.b4 = 0;
            packet.timestamp = 0;
            if (!bgb_write(&packet)) return;
            break;

        case BGB_CMD_SYNC3:
            if (packet.b2 != 0) break;
            bgb_clock = packet.timestamp;
            if (!bgb_write(&packet)) return;
            break;

        case BGB_CMD_STATUS:
            if (!set_status) {
                packet.cmd = BGB_CMD_STATUS;
                packet.b2 = 1;
                packet.b3 = 0;
                packet.b4 = 0;
                packet.timestamp = 0;
                if (!bgb_write(&packet)) return;
                set_status = true;
            }
            break;

        default:
            fprintf(stderr, "Unknown BGB command: %d (%02X %02X %02X) @ %d\n",
                    packet.cmd, packet.b2, packet.b3, packet.b4, packet.timestamp);
        }
    }
}

#ifdef __WIN32__
#define A_UNUSED __attribute__((unused))
void mobile_board_disable_spi(void) {}
void mobile_board_enable_spi(void) {}
#endif

FILE *mobile_config;
uint32_t bgb_clock_latch;

void mobile_board_config_read(unsigned char *dest, const uintptr_t offset, const size_t size)
{
    fseek(mobile_config, offset, SEEK_SET);
    return fread(dest, 1, size, mobile_config) == size;
}

void mobile_board_config_write(const unsigned char *src, const uintptr_t offset, const size_t size)
{
    fseek(mobile_config, offset, SEEK_SET);
    return fwrite(src, 1, size, mobile_config) == size;
}

void mobile_board_time_latch(void)
{
    bgb_clock_latch = bgb_clock;
}

bool mobile_board_time_check_ms(unsigned ms)
{
    return ((bgb_clock - bgb_clock_latch) & 0x7FFFFFFF) >
        (uint32_t)((double)ms * (1 << 21) / 1000);
}

void *thread_mobile_loop(__attribute__((unused)) void *argp)
{
    for (;;) {
        usleep(50000);
        mobile_loop();
        fflush(stdout);
    }
}

char *program_name;

void show_help(void)
{
    fprintf(stderr, "%s [-h] [-c config] [address [ip]]\n", program_name);
    exit(EXIT_FAILURE);
}

int main(__attribute__((unused)) int argc, char *argv[])
{
    program_name = argv[0];

    char *host = "localhost";
    int port = 8765;

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
        port = strtol(*argv++, NULL, 0);
    }

    mobile_config = fopen(fname_config, "r+b");
    if (!mobile_config) mobile_config = fopen(fname_config, "w+b");
    if (!mobile_config) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fseek(mobile_config, 0, SEEK_END);

    // Make sure config file is at least MOBILE_CONFIG_DATA_SIZE bytes big
    for (int i = ftell(mobile_config); i < MOBILE_CONFIG_DATA_SIZE; i++) {
        fputc(0, mobile_config);
    }

#ifdef __WIN32__
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return EXIT_FAILURE;
    }
#endif

    struct hostent *hostinfo = gethostbyname(host);
    if (!hostinfo) {
        fprintf(stderr, "Unknown host %s.\n", host);
        return EXIT_FAILURE;
    }

    struct sockaddr_in sockaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = *(struct in_addr *)(hostinfo->h_addr_list[0])
    };

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int val = 1;
#if defined(__unix__)
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val)) == -1) {
#elif defined(__WIN32__)
    if (setsockopt(sock, SOL_SOCKET, TCP_NODELAY, (char *)&val, sizeof(val)) == -1) {
#endif
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }

    pthread_t mobile_thread;
    mobile_init();
    if (pthread_create(&mobile_thread, NULL, thread_mobile_loop, NULL)) {
        fprintf(stderr, "Failed to create thread.\n");
        return EXIT_FAILURE;
    }

    bgb_sock = sock;
    bgb_loop();

    pthread_cancel(mobile_thread);
    close(sock);

#ifdef __WIN32__
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
