#include "vga.h"

typedef unsigned short uint16_t;

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_SIZE = VGA_WIDTH * VGA_HEIGHT,
    VGA_COLOR_LIGHT_GREEN = 0x0A
};

static volatile uint16_t *const vga_buffer = (uint16_t *)0xB8000;
static unsigned short cursor;

static void vga_put(char character)
{
    if (character == '\n') {
        cursor += VGA_WIDTH - (cursor % VGA_WIDTH);
        return;
    }

    vga_buffer[cursor] = ((uint16_t)VGA_COLOR_LIGHT_GREEN << 8) | (unsigned char)character;
    cursor++;

    if (cursor == VGA_SIZE) {
        cursor = 0;
    }
}

void vga_clear(void)
{
    unsigned short index;

    for (index = 0; index < VGA_SIZE; index++) {
        vga_buffer[index] = ((uint16_t)VGA_COLOR_LIGHT_GREEN << 8) | ' ';
    }

    cursor = 0;
}

void vga_write(const char *text)
{
    while (*text != '\0') {
        vga_put(*text);
        text++;
    }
}
