#pragma once

#include <stdbool.h>

#include "atomic.h"
#include "limits.h"
struct mobile_adapter;

#define MOBILE_MAX_DATA_SIZE 0xFF

enum mobile_command {
    MOBILE_COMMAND_BEGIN_SESSION = 0x10,
    MOBILE_COMMAND_END_SESSION,
    MOBILE_COMMAND_DIAL_TELEPHONE,
    MOBILE_COMMAND_HANG_UP_TELEPHONE,
    MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL,
    MOBILE_COMMAND_TRANSFER_DATA,
    MOBILE_COMMAND_RESET,
    MOBILE_COMMAND_TELEPHONE_STATUS,
    MOBILE_COMMAND_SIO32_MODE,
    MOBILE_COMMAND_READ_CONFIGURATION_DATA,
    MOBILE_COMMAND_WRITE_CONFIGURATION_DATA,
    MOBILE_COMMAND_TRANSFER_DATA_END = 0x1F,
    MOBILE_COMMAND_ISP_LOGIN = 0x21,
    MOBILE_COMMAND_ISP_LOGOUT,
    MOBILE_COMMAND_OPEN_TCP_CONNECTION,
    MOBILE_COMMAND_CLOSE_TCP_CONNECTION,
    MOBILE_COMMAND_OPEN_UDP_CONNECTION,
    MOBILE_COMMAND_CLOSE_UDP_CONNECTION,
    MOBILE_COMMAND_DNS_QUERY = 0x28,
    MOBILE_COMMAND_FIRMWARE_VERSION = 0x3F,
    MOBILE_COMMAND_ERROR = 0x6E,
};

enum mobile_connection_state {
    MOBILE_CONNECTION_DISCONNECTED,
    MOBILE_CONNECTION_CALL,
    MOBILE_CONNECTION_INTERNET,
};

struct mobile_packet {
    enum mobile_command command;
    unsigned length;
    unsigned char data[MOBILE_MAX_DATA_SIZE];
};

struct mobile_adapter_commands {
    _Atomic bool session_begun;
    bool packet_parsed;
    struct mobile_packet packet;

    unsigned processing;
    unsigned char processing_data[4];

    enum mobile_connection_state state;
    bool connections[MOBILE_MAX_CONNECTIONS];
    unsigned call_packets_sent;
    unsigned char dns1[4];
    unsigned char dns2[4];
};

void mobile_commands_reset(struct mobile_adapter *adapter);
struct mobile_packet *mobile_commands_process(struct mobile_adapter *adapter, struct mobile_packet *packet);

#undef _Atomic  // "atomic.h"
