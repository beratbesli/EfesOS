#include "programs.h"

static program_counter_t counter_runs;
static program_counter_t snake_steps;

void programs_init(void)
{
    counter_runs = 0;
    snake_steps = 0;
}

void counter_program(void)
{
    for (;;) {
        counter_runs++;
        __asm__ volatile ("hlt" : : : "memory");
    }
}

void snake_program(void)
{
    for (;;) {
        snake_steps++;
        __asm__ volatile ("hlt" : : : "memory");
    }
}

program_counter_t counter_program_runs(void)
{
    return counter_runs;
}

program_counter_t snake_program_steps(void)
{
    return snake_steps;
}
