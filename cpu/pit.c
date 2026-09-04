#include "io.h"
#include "hpet.h"
#include "pit.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL_ZERO 0x40
#define PIT_CHANNEL_TWO 0x42
#define PIT_SPEAKER_CONTROL_PORT 0x61
#define PIT_FREQUENCY 100U
#define PIT_DIVISOR (1193182U / PIT_FREQUENCY)
#define PIT_CHANNEL_TWO_ONESHOT 0xB0U
#define PIT_CHANNEL_TWO_GATE 0x01U
#define PIT_CHANNEL_TWO_SPEAKER 0x02U
#define PIT_CHANNEL_TWO_OUTPUT 0x20U
#define PIT_DELAY_CHUNK_MICROSECONDS 50000U
#define PIT_DELAY_POLL_LIMIT 10000000U

static volatile pit_tick_t tick_count;

void pit_init(void)
{
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL_ZERO, PIT_DIVISOR & 0xFFU);
    outb(PIT_CHANNEL_ZERO, (PIT_DIVISOR >> 8) & 0xFFU);
}

void pit_tick_handler(void)
{
    tick_count++;
    hpet_maintain();
}

pit_tick_t pit_ticks(void)
{
    return tick_count;
}

static int pit_poll_delay_chunk(unsigned int microseconds)
{
    unsigned int reload;
    unsigned int attempt;
    uint8_t saved_control;
    int output_was_low = 0;

    if (microseconds == 0U || microseconds > PIT_DELAY_CHUNK_MICROSECONDS) {
        return 0;
    }

    /* 1.194 MHz is rounded upward so the programmed interval is never
       shorter than the requested delay. */
    reload = (microseconds * 1194U + 999U) / 1000U;
    if (reload == 0U || reload > 0xFFFFU) {
        return 0;
    }

    saved_control = inb(PIT_SPEAKER_CONTROL_PORT);
    outb(PIT_SPEAKER_CONTROL_PORT,
        saved_control & ~(PIT_CHANNEL_TWO_GATE | PIT_CHANNEL_TWO_SPEAKER));
    outb(PIT_COMMAND_PORT, PIT_CHANNEL_TWO_ONESHOT);
    outb(PIT_CHANNEL_TWO, (uint8_t)reload);
    outb(PIT_CHANNEL_TWO, (uint8_t)(reload >> 8U));
    outb(PIT_SPEAKER_CONTROL_PORT,
        (saved_control & ~PIT_CHANNEL_TWO_SPEAKER) | PIT_CHANNEL_TWO_GATE);

    for (attempt = 0U; attempt < PIT_DELAY_POLL_LIMIT; attempt++) {
        if ((inb(PIT_SPEAKER_CONTROL_PORT) & PIT_CHANNEL_TWO_OUTPUT) == 0U) {
            output_was_low = 1;
        } else if (output_was_low) {
            outb(PIT_SPEAKER_CONTROL_PORT, saved_control);
            return 1;
        }
        __asm__ volatile ("pause");
    }
    outb(PIT_SPEAKER_CONTROL_PORT, saved_control);
    return 0;
}

int pit_poll_delay_microseconds(unsigned int microseconds)
{
    if (microseconds == 0U) {
        return 0;
    }
    while (microseconds != 0U) {
        unsigned int chunk = microseconds > PIT_DELAY_CHUNK_MICROSECONDS ?
            PIT_DELAY_CHUNK_MICROSECONDS : microseconds;

        if (!pit_poll_delay_chunk(chunk)) {
            return 0;
        }
        microseconds -= chunk;
    }
    return 1;
}
