#include "serial.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "utils.h"

#define SERIAL_BUFFER_SIZE 0x40

struct serial_buffer {
    volatile unsigned char buffer[SERIAL_BUFFER_SIZE];
    volatile unsigned head;
    volatile unsigned tail;
};

static volatile struct serial_buffer serial_rx;
static volatile struct serial_buffer serial_tx;

__attribute__((always_inline))
static inline int serial_buffer_isempty(volatile struct serial_buffer *buffer)
{
    return buffer->head == buffer->tail;
}

__attribute__((always_inline))
static inline int serial_buffer_isfull(volatile struct serial_buffer *buffer)
{
    return ((buffer->head + 1) % SERIAL_BUFFER_SIZE) == buffer->tail;
}

__attribute__((always_inline))
static inline void serial_buffer_put(volatile struct serial_buffer *buffer, char c)
{
    volatile unsigned head = buffer->head;
    buffer->buffer[head++] = c;
    buffer->head = head % SERIAL_BUFFER_SIZE;
}

__attribute__((always_inline))
static inline char serial_buffer_get(volatile struct serial_buffer *buffer)
{
    volatile unsigned tail = buffer->tail;
    char c = buffer->buffer[tail++];
    buffer->tail = tail % SERIAL_BUFFER_SIZE;
    return c;
}

static void serial_transmit(void)
{
    // Called when UDR0 is ready to receive new data

    UDR0 = serial_buffer_get(&serial_tx);

    // If we've sent everything, disable the interrupt
    if (serial_buffer_isempty(&serial_tx)) cbi(UCSR0B, UDRIE0);
}

static void serial_receive(void)
{
    // Called when UDR0 contains new data

    // Discard the byte if a parity error has occurred or the buffer is full
    if (bit_is_set(UCSR0A, UPE0) || serial_buffer_isfull(&serial_rx)) {
        UDR0;
        return;
    }

    // There's not much we can do with a full buffer, unfortunately.
    // Delaying the read will only cause it to be replaced with further data.

    serial_buffer_put(&serial_rx, UDR0);
}

ISR(USART_UDRE_vect) { serial_transmit(); }
ISR(USART_RX_vect) { serial_receive(); }

__attribute__((always_inline))
inline int serial_putchar_inline(const char c)
{
    // If the data register and buffer are empty, just send it straight away
    if (bit_is_set(UCSR0A, UDRE0) && serial_buffer_isempty(&serial_tx)) {
        UDR0 = c;
        return 1;
    }

    // Make sure there's space in the buffer and put the character
    while (serial_buffer_isfull(&serial_tx));
    serial_buffer_put(&serial_tx, c);

    // Enable the interrupt to transmit as soon as we can
    sbi(UCSR0B, UDRIE0);

    return 1;
}

int serial_putchar(const char c) { return serial_putchar_inline(c); }

__attribute__((always_inline))
inline int serial_getchar_inline(void)
{
    // Wait until there's something in the buffer
    while (serial_buffer_isempty(&serial_rx));

    // Read the next character
    return serial_buffer_get(&serial_rx);
}

int serial_getchar(void) { return serial_getchar_inline(); }

static int stdio_serial_putchar(const char c, __attribute__((unused)) FILE *stream)
{ return serial_putchar(c); }
static int stdio_serial_getchar(__attribute__((unused)) FILE *stream)
{ return serial_getchar(); }
static FILE serial = FDEV_SETUP_STREAM(stdio_serial_putchar, stdio_serial_getchar, _FDEV_SETUP_RW);

unsigned serial_available(void)
{
    return ((unsigned)(SERIAL_BUFFER_SIZE + serial_rx.head - serial_rx.tail)) % SERIAL_BUFFER_SIZE;
}

void serial_drain(void)
{
    while (bit_is_set(UCSR0B, UDRIE0) || bit_is_set(UCSR0A, RXC0));
}

void serial_init_config(const unsigned long bauds, const uint8_t config)
{
    // Calculate UBRRn value as per the datasheet
    uint16_t ubrr = F_CPU / 4 / 2 / bauds - 1;
    UBRR0H = (ubrr >> 8) & 0xFF;
    UBRR0L = ubrr & 0xFF;

    // Configure the serial
    UCSR0A = _BV(U2X0);  // Double speed
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);  // Enable it and the receive interrupt
    UCSR0C = config;

    // Setup stdio streams
    stdout = stderr = stdin = &serial;
}

void serial_init(const unsigned long bauds) { serial_init_config(bauds, SERIAL_8N1); }
