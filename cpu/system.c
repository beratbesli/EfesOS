#include "io.h"
#include "system.h"

static void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void stop_cpu(void)
{
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void system_reboot(void)
{
    outb(0x64, 0xFE);
    stop_cpu();
}

void system_shutdown(void)
{
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    stop_cpu();
}
