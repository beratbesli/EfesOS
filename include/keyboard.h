#ifndef EFESOS_KEYBOARD_H
#define EFESOS_KEYBOARD_H

enum keyboard_layout {
    KEYBOARD_LAYOUT_ENGLISH,
    KEYBOARD_LAYOUT_TURKISH
};

void keyboard_init(void);
void keyboard_set_layout(enum keyboard_layout layout);
void keyboard_irq_handler(void);
int keyboard_has_pending(void);
int keyboard_read_char(unsigned char *character);
unsigned int keyboard_dropped_input_count(void);

#endif
