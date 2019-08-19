#include <EEPROM.h>
#include "src/libmobile/mobile.h"

// Define this to print every byte sent and received
//#define DEBUG_SPI

// Define this to print every command sent and received
#define DEBUG_CMD

#ifdef DEBUG_SPI
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

char last_SPDR = 0xD2;
#endif

#ifdef DEBUG_CMD
static int serial_putchar(char c, FILE *f) { return Serial.write(c); }
static FILE serial_stdout;

#include "src/libmobile/debug_cmd.h"
#else
void mobile_board_debug_cmd(const int send, const struct mobile_packet *packet) {}
#endif

void mobile_board_reset_spi(void)
{
    SPCR = 0;
    pinMode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPDR = 0xD2;
}

void mobile_board_config_read(unsigned char *dest, const uintptr_t offset, const size_t size)
{
    for (size_t i = 0; i < size; i++) dest[i] = EEPROM.read(offset + i);
}

void mobile_board_config_write(const unsigned char *src, const uintptr_t offset, const size_t size)
{
    for (size_t i = 0; i < size; i++) EEPROM.write(offset + i, src[i]);
}

void setup()
{
    Serial.begin(2000000);
    mobile_init();

#ifdef DEBUG_SPI
    buf_in = 0;
    buf_out = 0;
#endif

#ifdef DEBUG_CMD
    // Redirect any printf to the Arduino serial.
    fdev_setup_stream(&serial_stdout, serial_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = &serial_stdout;
#endif

#if defined(DEBUG_SPI) || defined(DEBUG_CMD)
    Serial.println("----");
#endif
}

void loop()
{
    mobile_loop();

#ifdef DEBUG_SPI
    if (!buffer_isempty()) {
        Serial.print("In ");
        Serial.print(buffer_get(), HEX);
        while (buffer_isempty());
        Serial.print(" Out ");
        Serial.println(buffer_get(), HEX);
    }
#endif
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
