#ifndef EFESOS_GAMES_H
#define EFESOS_GAMES_H

int games_is_active(void);
void games_start_snake(void);
void games_start_slot(void);
int games_handle_char(unsigned char character);
void games_tick(void);
unsigned int games_last_score(void);

#endif
