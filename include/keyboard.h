#ifndef BEEROS_KEYBOARD_H
#define BEEROS_KEYBOARD_H

enum keyboard_layout {
    KEYBOARD_LAYOUT_ENGLISH,
    KEYBOARD_LAYOUT_TURKISH
};

void keyboard_init(void);
void keyboard_set_layout(enum keyboard_layout layout);
void keyboard_irq_handler(void);

#endif
