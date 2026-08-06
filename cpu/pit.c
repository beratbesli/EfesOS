#include "io.h"
#include "pit.h"
#include "scheduler.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL_ZERO 0x40
#define PIT_FREQUENCY 100U
#define PIT_DIVISOR (1193182U / PIT_FREQUENCY)

static volatile pit_tick_t tick_count;

void pit_init(void)
{
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL_ZERO, PIT_DIVISOR & 0xFFU);
    outb(PIT_CHANNEL_ZERO, (PIT_DIVISOR >> 8) & 0xFFU);
}

void pit_irq_handler(void)
{
    tick_count++;
    scheduler_tick();
    outb(0x20, 0x20);
}

pit_tick_t pit_ticks(void)
{
    return tick_count;
}
