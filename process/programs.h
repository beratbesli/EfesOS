#ifndef BEEROS_PROGRAMS_H
#define BEEROS_PROGRAMS_H

typedef unsigned int program_counter_t;

void programs_init(void);
void counter_program(void);
void snake_program(void);
program_counter_t counter_program_runs(void);
program_counter_t snake_program_steps(void);

#endif
