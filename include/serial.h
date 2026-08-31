#ifndef EFESOS_SERIAL_H
#define EFESOS_SERIAL_H

int serial_init(void);
int serial_is_ready(void);
void serial_write_char(char character);
void serial_write(const char *text);
void serial_write_n(const char *text, unsigned int length);
void serial_write_hex(unsigned int value);

#endif
