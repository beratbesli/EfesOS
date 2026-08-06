#ifndef BEEROS_PIT_H
#define BEEROS_PIT_H

typedef unsigned int pit_tick_t;

void pit_init(void);
void pit_irq_handler(void);
pit_tick_t pit_ticks(void);

#endif
