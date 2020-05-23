#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "serial.h"
#include "commands.h"

enum mobile_adapter_device {
    // The clients.
    MOBILE_ADAPTER_GAMEBOY,
    MOBILE_ADAPTER_GAMEBOY_ADVANCE,

    // The adapters, this is what we're emulating.
    MOBILE_ADAPTER_BLUE = 8,
    MOBILE_ADAPTER_YELLOW,
    MOBILE_ADAPTER_GREEN,
    MOBILE_ADAPTER_RED
};

enum mobile_action {
    MOBILE_ACTION_NONE,
    MOBILE_ACTION_PROCESS_PACKET,
    MOBILE_ACTION_DROP_CONNECTION,
    MOBILE_ACTION_RESET_SERIAL
};

struct mobile_adapter_config {
    enum mobile_adapter_device device;
    unsigned p2p_port;

    // Signals Pokémon Crystal (jap) that the connection isn't metered,
    //   removing the time limit in mobile battles.
    // We have no idea of the effects of this in other games.
    bool unmetered;
};
#define MOBILE_ADAPTER_CONFIG_DEFAULT (struct mobile_adapter_config){ \
    .device = MOBILE_ADAPTER_BLUE, \
    .p2p_port = 2415, \
    .unmetered = false \
}

struct mobile_adapter {
    void *user;
    struct mobile_adapter_config config;
    struct mobile_adapter_serial serial;
    struct mobile_adapter_commands commands;
};

// Board-specific function prototypes (make sure these are defined elsewhere!)
// TODO: Actually document these functions, with expectations and assumptions.
void mobile_board_serial_disable(void *user);
void mobile_board_serial_enable(void *user);
void mobile_board_debug_cmd(void *user, const int send, const struct mobile_packet *packet);
bool mobile_board_config_read(void *user, void *dest, const uintptr_t offset, const size_t size);
bool mobile_board_config_write(void *user, const void *src, const uintptr_t offset, const size_t size);
void mobile_board_time_latch(void *user);
bool mobile_board_time_check_ms(void *user, const unsigned ms);
bool mobile_board_tcp_connect(void *user, unsigned conn, const unsigned char *host, const unsigned port);
bool mobile_board_tcp_listen(void *user, unsigned conn, const unsigned port);
bool mobile_board_tcp_accept(void *user, unsigned conn);
void mobile_board_tcp_disconnect(void *user, unsigned conn);
bool mobile_board_tcp_send(void *user, unsigned conn, const void *data, const unsigned size);
int mobile_board_tcp_receive(void *user, unsigned conn, void *data);
bool mobile_board_dns_query(unsigned char *ip, const char *host, const unsigned char  *dns1, const unsigned char *dns2);

enum mobile_action mobile_action_get(struct mobile_adapter *adapter);
void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action);
void mobile_loop(struct mobile_adapter *adapter);
void mobile_init(struct mobile_adapter *adapter, void *user, struct mobile_adapter_config *config);

#ifdef __cplusplus
}
#endif
