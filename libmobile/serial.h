#pragma once

#include <stdint.h>

#include "atomic.h"
#include "commands.h"
struct mobile_adapter;

enum mobile_serial_state {
    MOBILE_SERIAL_WAITING,
    MOBILE_SERIAL_DATA,
    MOBILE_SERIAL_CHECKSUM,
    MOBILE_SERIAL_ACKNOWLEDGE,
    MOBILE_SERIAL_RESPONSE_WAITING,
    MOBILE_SERIAL_RESPONSE_START,
    MOBILE_SERIAL_RESPONSE_DATA,
    MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE
#if __GNUC__ && __AVR__
// Required for AVR _Atomic (it has no libatomic).
} __attribute__((packed));
#else
};
#endif

enum mobile_serial_error {
    MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND = 0xF0,
    MOBILE_SERIAL_ERROR_CHECKSUM,

    // Returned when:
    // - Transfer buffer is full and the transfer command is used
    // - Current command was canceled before sending a reply due to a serial
    //    timeout bigger than the command's timeout (>2s), but the device
    //    wasn't reset yet (>3s).
    MOBILE_SERIAL_ERROR_INTERNAL
};

struct mobile_adapter_serial {
    _Atomic enum mobile_serial_state state;
    _Atomic bool mode_32bit;
    _Atomic bool mode_32bit_cur;
    unsigned current;
    unsigned char buffer[4 + MOBILE_MAX_DATA_SIZE + 2 + 3];  // header, content, checksum + alignment to 4 bytes
    unsigned data_size;
    uint16_t checksum;
    enum mobile_serial_error error;
};

void mobile_serial_reset(struct mobile_adapter *adapter);
unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c);

#undef _Atomic  // "atomic.h"
