#ifndef BEEROS_VGA_H
#define BEEROS_VGA_H

enum vga_color {
    VGA_COLOR_GREEN,
    VGA_COLOR_WHITE,
    VGA_COLOR_RED,
    VGA_COLOR_BLUE,
    VGA_COLOR_YELLOW
};

void vga_init(void);
void vga_clear(void);
void vga_write_char(char character);
void vga_backspace(void);
void vga_write(const char *text);
void vga_write_unsigned(unsigned int value);
void vga_set_color(enum vga_color color);
enum vga_color vga_get_color(void);

#endif
