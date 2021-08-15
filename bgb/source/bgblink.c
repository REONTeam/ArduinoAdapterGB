#include "bgblink.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "libmobile/compat.h"

#include "socket.h"

enum bgb_cmd {
    BGB_CMD_VERSION = 1,
    BGB_CMD_JOYPAD = 101,
    BGB_CMD_SYNC1 = 104,
    BGB_CMD_SYNC2,
    BGB_CMD_SYNC3,
    BGB_CMD_STATUS = 108,
    BGB_CMD_WANTDISCONNECT
};

A_PACKED(struct bgb_packet) {
    unsigned char cmd;
    unsigned char b2;
    unsigned char b3;
    unsigned char b4;
    uint32_t timestamp;
};

static const struct bgb_packet handshake = {
    .cmd = BGB_CMD_VERSION,
    .b2 = 1,
    .b3 = 4,
    .b4 = 0,
    .timestamp = 0,
};

static bool bgb_write(int socket, struct bgb_packet *buf)
{
    ssize_t num = send(socket, (char *)buf, sizeof(struct bgb_packet), 0);
    if (num == -1) {
        socket_perror("bgb_send");
        return false;
    }
    return num == sizeof(struct bgb_packet);
}

static bool bgb_read(int socket, struct bgb_packet *buf)
{
    ssize_t num = recv(socket, (char *)buf, sizeof(struct bgb_packet), 0);
    if (num == -1) {
        socket_perror("bgb_recv");
        return false;
    }
    return num == sizeof(struct bgb_packet);
}

void bgb_loop(int socket, unsigned char (*callback_transfer)(void *, unsigned char), void (*callback_timestamp)(void *, uint32_t), void *user)
{
    struct bgb_packet packet;

    unsigned char transfer_last = 0xD2;
    unsigned char transfer_cur;

    // Handshake
    memcpy(&packet, &handshake, sizeof(packet));
    if (!bgb_write(socket, &packet)) return;
    if (!bgb_read(socket, &packet)) return;
    if (memcmp(&packet, &handshake, sizeof(packet)) != 0) {
        fprintf(stderr, "bgb_loop: Invalid handshake\n");
        return;
    }

    // Send initial status
    packet.cmd = BGB_CMD_STATUS;
    packet.b2 = 3;
    packet.b3 = 0;
    packet.b4 = 0;
    packet.timestamp = 0;
    if (!bgb_write(socket, &packet)) return;

    bool set_status = false;

    for (;;) {
        if (!bgb_read(socket, &packet)) return;

        switch (packet.cmd) {
        case BGB_CMD_JOYPAD:
            // Not relevant
            break;

        case BGB_CMD_SYNC1:
            if (callback_timestamp) callback_timestamp(user, packet.timestamp);
            packet.cmd = BGB_CMD_SYNC2;
            transfer_cur = callback_transfer(user, packet.b2);
            packet.b2 = transfer_last;
            transfer_last = transfer_cur;
            packet.b3 = 0x80;
            packet.b4 = 0;
            packet.timestamp = 0;
            if (!bgb_write(socket, &packet)) return;
            break;

        case BGB_CMD_SYNC3:
            if (packet.b2 != 0) break;
            if (callback_timestamp) callback_timestamp(user, packet.timestamp);
            if (!bgb_write(socket, &packet)) return;
            break;

        case BGB_CMD_STATUS:
            if (!set_status) {
                packet.cmd = BGB_CMD_STATUS;
                packet.b2 = 1;
                packet.b3 = 0;
                packet.b4 = 0;
                packet.timestamp = 0;
                if (!bgb_write(socket, &packet)) return;
                set_status = true;
            }
            break;

        default:
            fprintf(stderr, "bgb_loop: Unknown command: %d (%02X %02X %02X) @ %d\n",
                    packet.cmd, packet.b2, packet.b3, packet.b4, packet.timestamp);
            return;
        }
    }
}
