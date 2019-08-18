#include "src/libmobile/mobile.h"

#define BUF_LEN 0x100
volatile unsigned char buffer[BUF_LEN];
volatile unsigned buf_in;
volatile unsigned buf_out;

int buffer_isempty()
{
    return buf_in == buf_out;
}

int buffer_isfull()
{
    return (buf_in + 1) % BUF_LEN == buf_out;
}

void buffer_put(unsigned char c)
{
    volatile unsigned in = buf_in;
    buffer[in++] = c;
    buf_in = in % BUF_LEN;
}

unsigned char buffer_get()
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
    pinMode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPDR = last_SPDR = 0xD2;
}

void setup()
{
    Serial.begin(9600);
    mobile_init();

    buf_in = 0;
    buf_out = 0;

    Serial.println("----");
}

void loop()
{
    mobile_loop();

    if (!buffer_isempty()) {
        Serial.print("In ");
        Serial.print(buffer_get(), HEX);
        while (buffer_isempty());
        Serial.print(" Out ");
        Serial.println(buffer_get(), HEX);
    }
}

ISR (SPI_STC_vect)
{
    if (!buffer_isfull()) buffer_put(SPDR);
    if (!buffer_isfull()) buffer_put(last_SPDR);
    SPDR = last_SPDR = mobile_transfer(SPDR);
}
