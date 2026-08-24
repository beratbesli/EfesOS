#ifndef AYRANOS_IO_H
#define AYRANOS_IO_H

typedef unsigned char uint8_t;

static inline uint8_t inb(unsigned short port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(unsigned short port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

#endif
