#include <stdint.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
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
volatile unsigned buf_in;
volatile unsigned buf_out;

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

#ifdef DEBUG_CMD
#include "libmobile/debug_cmd.h"
#endif

volatile uint32_t micros = 0;
uint32_t micros_latch = 0;

void mobile_board_disable_spi(void)
{
    SPCR = SPSR = 0;
}

void mobile_board_enable_spi(void)
{
    pinmode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPDR = 0xD2;
}

void mobile_board_config_read(unsigned char *dest, const uintptr_t offset, const size_t size)
{
    eeprom_read_block(dest, (void *)offset, size);
}

void mobile_board_config_write(const unsigned char *src, const uintptr_t offset, const size_t size)
{
    eeprom_write_block(src, (void *)offset, size);
}

void mobile_board_time_latch(void)
{
	uint8_t old_SREG = SREG;
    cli();
    micros_latch = micros;
    SREG = old_SREG;
}

bool mobile_board_time_check_ms(unsigned ms)
{
    uint8_t old_SREG = SREG;
    cli();
    bool ret = micros > micros_latch + ((uint32_t)ms * 1000);
    SREG = old_SREG;
    return ret;
}

int main(void)
{
    serial_init(2000000);
    mobile_init();

    // Set up timer 0
    micros = 0;
    TCNT0 = 0;
    TCCR0B = _BV(CS01) | _BV(CS00);  // Prescale by 1/64
    TIMSK0 = _BV(TOIE0);  // Enable the interrupt

    sei();

#ifdef DEBUG_SPI
    buf_in = 0;
    buf_out = 0;
#endif

#if defined(DEBUG_SPI) || defined(DEBUG_CMD)
    printf("----\r\n");
#endif

    for (;;) {
        mobile_loop();

#ifdef DEBUG_SPI
        if (!buffer_isempty()) {
            printf("In %02X ", buffer_get());
            while (buffer_isempty());
            printf("Out %02X\r\n", buffer_get());
        }
#endif
    }
}

ISR (SPI_STC_vect)
{
#ifdef DEBUG_SPI
    if (!buffer_isfull()) buffer_put(SPDR);
    if (!buffer_isfull()) buffer_put(last_SPDR);
    SPDR = last_SPDR = mobile_transfer(SPDR);
#else
    SPDR = mobile_transfer(SPDR);
#endif
}

ISR (TIMER0_OVF_vect)
{
    micros += (64 * 256) / (F_CPU / 1000000L);
}
