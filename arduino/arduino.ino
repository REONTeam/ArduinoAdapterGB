#include <EEPROM.h>
#include "src/libmobile/mobile.h"

// Define this to print every byte sent and received
//#define DEBUG_SPI

// Define this to print every command sent and received
#define DEBUG_CMD

#ifdef DEBUG_SPI
#define BUF_LEN 0x100
volatile unsigned char buffer[BUF_LEN];
volatile unsigned buf_in = 0;
volatile unsigned buf_out = 0;

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

struct mobile_adapter adapter;

#ifdef DEBUG_CMD
int serial_putchar(char c, FILE *f) { return Serial.write(c); }
FILE serial_stdout;
#include "src/libmobile/debug_cmd.h"
#endif

unsigned long millis_latch = 0;

#define A_UNUSED __attribute__((unused))

void mobile_board_disable_spi(A_UNUSED void *user)
{
    SPCR = SPSR = 0;
}

void mobile_board_enable_spi(A_UNUSED void *user)
{
    pinMode(PIN_SPI_MISO, OUTPUT);
    SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);
    SPSR = 0;
    SPDR = 0xD2;
}

bool mobile_board_config_read(A_UNUSED void *user, void *dest, const uintptr_t offset, const size_t size)
{
    for (size_t i = 0; i < size; i++) ((char *)dest)[i] = EEPROM.read(offset + i);
    return true;
}

bool mobile_board_config_write(A_UNUSED void *user, const void *src, const uintptr_t offset, const size_t size)
{
    for (size_t i = 0; i < size; i++) EEPROM.write(offset + i, ((char *)src)[i]);
    return true;
}

void mobile_board_time_latch(A_UNUSED void *user)
{
    millis_latch = millis();
}

bool mobile_board_time_check_ms(A_UNUSED void *user, unsigned ms)
{
    return (millis() - millis_latch) > (unsigned long)ms;
}

void setup()
{
    Serial.begin(2000000);
    mobile_init(&adapter, NULL);

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
    mobile_loop(&adapter);

#ifdef DEBUG_SPI
    if (!buffer_isempty()) {
        Serial.print(buffer_get(), HEX);
        while (buffer_isempty());
        Serial.print("\t");
        Serial.println(buffer_get(), HEX);
    }
#endif
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
