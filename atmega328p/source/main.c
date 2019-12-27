#include <stdint.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include "libmobile/mobile.h"

#include "utils.h"
#include "pins.h"
#include "serial.h"

// Define this to print every byte sent and received
//#define DEBUG_SPI

// Define this to print every command sent and received
#define DEBUG_CMD

#ifdef DEBUG_SPI
#define BUF_LEN 0x100
volatile unsigned char buffer[BUF_LEN];
volatile unsigned buf_in = 0;
volatile unsigned buf_out = 0;

int buffer_isempty(void)
{
    return buf_in == buf_out;
}

int buffer_isfull(void)
{
    return (buf_in + 1) % BUF_LEN == buf_out;
}

void buffer_put(unsigned char c)
{
    volatile unsigned in = buf_in;
    buffer[in++] = c;
    buf_in = in % BUF_LEN;
}

unsigned char buffer_get(void)
{
    volatile unsigned out = buf_out;
    unsigned char c = buffer[out++];
    buf_out = out % BUF_LEN;
    return c;
}

char last_SPDR = 0xD2;
#endif

struct mobile_adapter adapter;

#ifdef DEBUG_CMD
#include "libmobile/debug_cmd.h"
#endif

volatile uint32_t micros = 0;
uint32_t micros_latch = 0;

#define A_UNUSED __attribute__((unused))

void mobile_board_serial_disable(A_UNUSED void *user)
{
    SPCR = SPSR = 0;
}

void mobile_board_serial_enable(A_UNUSED void *user)
{
    pinmode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPSR = 0;
    SPDR = 0xD2;
}

bool mobile_board_config_read(A_UNUSED void *user, void *dest, const uintptr_t offset, const size_t size)
{
    eeprom_read_block(dest, (void *)offset, size);
    return true;
}

bool mobile_board_config_write(A_UNUSED void *user, const void *src, const uintptr_t offset, const size_t size)
{
    eeprom_write_block(src, (void *)offset, size);
    return true;
}

void mobile_board_time_latch(A_UNUSED void *user)
{
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        micros_latch = micros;
    }
}

bool mobile_board_time_check_ms(A_UNUSED void *user, unsigned ms)
{
    bool ret;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        ret = (micros - micros_latch) > ((uint32_t)ms * 1000);
    }
    return ret;
}

int main(void)
{
    serial_init(2000000);
    mobile_init(&adapter, NULL, NULL);

    // Set up timer 0
    TCNT0 = 0;
    TCCR0B = _BV(CS01) | _BV(CS00);  // Prescale by 1/64
    TIMSK0 = _BV(TOIE0);  // Enable the interrupt

    sei();

#if defined(DEBUG_SPI) || defined(DEBUG_CMD)
    printf("----\r\n");
#endif

    for (;;) {
        mobile_loop(&adapter);

#ifdef DEBUG_SPI
        if (!buffer_isempty()) {
            printf("%02X\t", buffer_get());
            while (buffer_isempty());
            printf("%02X\r\n", buffer_get());
        }
#endif
    }
}

ISR (SPI_STC_vect)
{
#ifdef DEBUG_SPI
    if (!buffer_isfull()) buffer_put(SPDR);
    if (!buffer_isfull()) buffer_put(last_SPDR);
    SPDR = last_SPDR = mobile_transfer(&adapter, SPDR);
#else
    SPDR = mobile_transfer(&adapter, SPDR);
#endif
}

ISR (TIMER0_OVF_vect)
{
    micros += (64 * 256) / (F_CPU / 1000000L);
}
