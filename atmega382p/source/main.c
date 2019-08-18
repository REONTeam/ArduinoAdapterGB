#include <stdio.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "utils.h"
#include "pins.h"
#include "serial.h"
#include "libmobile/mobile.h"

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

char last_SPDR;

void mobile_board_reset_spi(void)
{
    SPCR = 0;
    pinmode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPDR = last_SPDR = 0xD2;
}

int main(void)
{
    serial_init(9600);
    mobile_init();

    buf_in = 0;
    buf_out = 0;

    printf("----\r\n");

    for (;;) {
        mobile_loop();

        if (!buffer_isempty()) {
            printf("In %02X ", buffer_get());
            while (buffer_isempty());
            printf("Out %02X\r\n", buffer_get());
        }
    }
}


ISR (SPI_STC_vect)
{
    if (!buffer_isfull()) buffer_put(SPDR);
    if (!buffer_isfull()) buffer_put(last_SPDR);
    SPDR = last_SPDR = mobile_transfer(SPDR);
}
