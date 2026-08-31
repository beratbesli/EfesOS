#include "io.h"
#include "serial.h"

#define COM1_PORT 0x3F8U
#define SERIAL_TRANSMIT_READY 0x20U
#define SERIAL_WAIT_LIMIT 1000000U

static int ready;

static int wait_for_transmit(void)
{
    unsigned int remaining = SERIAL_WAIT_LIMIT;

    while (remaining != 0U) {
        if ((inb(COM1_PORT + 5U) & SERIAL_TRANSMIT_READY) != 0U) {
            return 1;
        }
        remaining--;
    }

    return 0;
}

int serial_init(void)
{
    outb(COM1_PORT + 1U, 0x00);
    outb(COM1_PORT + 3U, 0x80);
    outb(COM1_PORT + 0U, 0x03);
    outb(COM1_PORT + 1U, 0x00);
    outb(COM1_PORT + 3U, 0x03);
    outb(COM1_PORT + 2U, 0xC7);
    outb(COM1_PORT + 4U, 0x1E);
    outb(COM1_PORT + 0U, 0xAE);

    if (inb(COM1_PORT + 0U) != 0xAE) {
        ready = 0;
        return 0;
    }

    outb(COM1_PORT + 4U, 0x0F);
    ready = 1;
    return 1;
}

int serial_is_ready(void)
{
    return ready;
}

void serial_write_char(char character)
{
    if (ready == 0) {
        return;
    }
    if (!wait_for_transmit()) {
        ready = 0;
        return;
    }

    outb(COM1_PORT, (unsigned char)character);
}

void serial_write(const char *text)
{
    if (text == 0) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*text);
        text++;
    }
}

void serial_write_n(const char *text, unsigned int length)
{
    unsigned int index;

    if (text == 0) {
        return;
    }
    for (index = 0; index < length; index++) {
        serial_write_char(text[index]);
    }
}

void serial_write_hex(unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    serial_write("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        serial_write_char(digits[(value >> (unsigned int)shift) & 0x0FU]);
    }
}
