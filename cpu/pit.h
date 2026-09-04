#ifndef EFESOS_PIT_H
#define EFESOS_PIT_H

typedef unsigned int pit_tick_t;

void pit_init(void);
void pit_tick_handler(void);
pit_tick_t pit_ticks(void);
int pit_poll_delay_microseconds(unsigned int microseconds);

#endif
