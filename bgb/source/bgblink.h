#pragma once

#include <stdint.h>

void socket_perror(const char *func);
void bgb_loop(int socket, unsigned char (*callback_transfer)(void *, unsigned char), void (*callback_timestamp)(void *, uint32_t), void *user);
