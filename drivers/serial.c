#include "io.h"
#include "serial.h"

#define COM1_PORT 0x3F8U
#define SERIAL_TRANSMIT_READY 0x20U
#define SERIAL_WAIT_LIMIT 1000000U

static int ready;

static unsigned int serial_irq_save(void)
{
    unsigned int flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void serial_irq_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

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
    unsigned int flags = serial_irq_save();

    if (ready == 0) {
        serial_irq_restore(flags);
        return;
    }
    if (!wait_for_transmit()) {
        ready = 0;
        serial_irq_restore(flags);
        return;
    }

    outb(COM1_PORT, (unsigned char)character);
    serial_irq_restore(flags);
}

static void serial_write_unlocked(const char *text)
{
    if (text == 0) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            if (ready != 0 && wait_for_transmit()) {
                outb(COM1_PORT, '\r');
            }
        }
        if (ready != 0 && wait_for_transmit()) {
            outb(COM1_PORT, (unsigned char)*text);
        } else {
            ready = 0;
        }
        text++;
    }
}

void serial_write(const char *text)
{
    unsigned int flags = serial_irq_save();
    serial_write_unlocked(text);
    serial_irq_restore(flags);
}

void serial_write_n(const char *text, unsigned int length)
{
    unsigned int index;
    unsigned int flags = serial_irq_save();

    if (text == 0) {
        serial_irq_restore(flags);
        return;
    }
    for (index = 0; index < length; index++) {
        if (ready != 0 && wait_for_transmit()) {
            outb(COM1_PORT, (unsigned char)text[index]);
        } else {
            ready = 0;
            break;
        }
    }
    serial_irq_restore(flags);
}

void serial_write_hex(unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    unsigned int flags = serial_irq_save();

    serial_write_unlocked("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        if (ready != 0 && wait_for_transmit()) {
            outb(COM1_PORT, (unsigned char)digits[(value >> (unsigned int)shift) & 0x0FU]);
        } else {
            ready = 0;
            break;
        }
    }
    serial_irq_restore(flags);
}
