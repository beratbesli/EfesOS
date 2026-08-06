#include "vga.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

enum {
    TEXT_WIDTH = 80,
    TEXT_HEIGHT = 25,
    TEXT_SIZE = TEXT_WIDTH * TEXT_HEIGHT,
    GRAPHICS_WIDTH = 1024,
    GRAPHICS_HEIGHT = 768,
    GRAPHICS_COLUMNS = GRAPHICS_WIDTH / 8,
    GRAPHICS_ROWS = GRAPHICS_HEIGHT / 16,
    VBE_MODE_FLAG_ADDRESS = 0x04F0,
    VBE_FONT_SEGMENT_ADDRESS = 0x04F2,
    VBE_FONT_OFFSET_ADDRESS = 0x04F4,
    VBE_FRAMEBUFFER_VIRTUAL = 0xE0000000,
    BGA_INDEX_PORT = 0x01CE,
    BGA_DATA_PORT = 0x01CF,
    BGA_ID = 0,
    BGA_ID4 = 0xB0C4,
    BGA_XRES = 1,
    BGA_YRES = 2,
    BGA_BPP = 3,
    BGA_ENABLE = 4,
    BGA_ENABLED_WITH_LFB = 0x41,
    PCI_CONFIG_ADDRESS_PORT = 0x0CF8,
    PCI_CONFIG_DATA_PORT = 0x0CFC,
    VGA_PCI_CONFIG_COMMAND = 0x80001004U,
    VGA_PCI_CONFIG_BAR0 = 0x80001010U,
    VGA_PCI_COMMAND_IO_AND_MEMORY = 0x0003,
    VGA_COLOR_LIGHT_GREEN = 0x0A
};

static volatile uint16_t *const text_buffer = (uint16_t *)0xB8000;
static volatile uint32_t *const graphics_buffer = (uint32_t *)VBE_FRAMEBUFFER_VIRTUAL;
static volatile uint8_t *font_data;
static unsigned short text_cursor;
static unsigned short graphics_cursor;
static int graphics_active;

static uint8_t glyph_index(uint8_t character)
{
    switch (character) {
    case 0xC7:
        return 0x80;
    case 0xFC:
        return 0x81;
    case 0xD6:
        return 0x99;
    case 0xDC:
        return 0x9A;
    case 0xF6:
        return 0x94;
    case 0xE7:
        return 0x87;
    case 0xF0:
        return 'g';
    case 0xD0:
        return 'G';
    case 0xFD:
        return 'i';
    case 0xDD:
        return 'I';
    case 0xFE:
        return 's';
    case 0xDE:
        return 'S';
    default:
        return character;
    }
}

static uint8_t glyph_row(uint8_t character, uint8_t row)
{
    uint8_t pixels = font_data[(glyph_index(character) * 16U) + row];

    if ((character == 0xF0 || character == 0xD0) && row == 1) {
        return 0x18;
    }
    if ((character == 0xF0 || character == 0xD0) && row == 2) {
        return 0x24;
    }
    if ((character == 0xF0 || character == 0xD0) && row == 3) {
        return 0x18;
    }
    if (character == 0xFD && row < 5) {
        return 0;
    }
    if (character == 0xDD && row == 1) {
        return 0x18;
    }
    if ((character == 0xFE || character == 0xDE) && row == 14) {
        return 0x18;
    }
    if ((character == 0xFE || character == 0xDE) && row == 15) {
        return 0x10;
    }

    return pixels;
}

static void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static void bga_write(uint16_t index, uint16_t value)
{
    outw(BGA_INDEX_PORT, index);
    outw(BGA_DATA_PORT, value);
}

static void clear_graphics_cell(unsigned short cell)
{
    unsigned short row = cell / GRAPHICS_COLUMNS;
    unsigned short column = cell % GRAPHICS_COLUMNS;
    unsigned short y;

    for (y = 0; y < 16; y++) {
        unsigned short x;

        for (x = 0; x < 8; x++) {
            graphics_buffer[((row * 16U + y) * GRAPHICS_WIDTH) + (column * 8U + x)] = 0;
        }
    }
}

static void draw_graphics_char(char character)
{
    unsigned short row;
    unsigned short column;
    unsigned short y;

    if (character == '\n') {
        graphics_cursor += GRAPHICS_COLUMNS - (graphics_cursor % GRAPHICS_COLUMNS);
        if (graphics_cursor >= GRAPHICS_COLUMNS * GRAPHICS_ROWS) {
            graphics_cursor = 0;
        }
        return;
    }

    row = graphics_cursor / GRAPHICS_COLUMNS;
    column = graphics_cursor % GRAPHICS_COLUMNS;
    for (y = 0; y < 16; y++) {
        unsigned short x;
        uint8_t pixels = glyph_row((unsigned char)character, y);

        for (x = 0; x < 8; x++) {
            uint32_t color = (pixels & (0x80U >> x)) ? 0x0000FF00U : 0;
            graphics_buffer[((row * 16U + y) * GRAPHICS_WIDTH) + (column * 8U + x)] = color;
        }
    }

    graphics_cursor++;
    if (graphics_cursor == GRAPHICS_COLUMNS * GRAPHICS_ROWS) {
        graphics_cursor = 0;
    }
}

void vga_init(void)
{
    uint16_t segment;
    uint16_t offset;

    if (*(volatile uint16_t *)VBE_MODE_FLAG_ADDRESS != 0xB33FU) {
        return;
    }

    segment = *(volatile uint16_t *)VBE_FONT_SEGMENT_ADDRESS;
    offset = *(volatile uint16_t *)VBE_FONT_OFFSET_ADDRESS;
    font_data = (volatile uint8_t *)(((uint32_t)segment << 4) + offset);
    outl(PCI_CONFIG_ADDRESS_PORT, VGA_PCI_CONFIG_BAR0);
    outl(PCI_CONFIG_DATA_PORT, VBE_FRAMEBUFFER_VIRTUAL);
    outl(PCI_CONFIG_ADDRESS_PORT, VGA_PCI_CONFIG_COMMAND);
    outw(PCI_CONFIG_DATA_PORT, VGA_PCI_COMMAND_IO_AND_MEMORY);
    bga_write(BGA_ID, BGA_ID4);
    bga_write(BGA_ENABLE, 0);
    bga_write(BGA_XRES, GRAPHICS_WIDTH);
    bga_write(BGA_YRES, GRAPHICS_HEIGHT);
    bga_write(BGA_BPP, 32);
    bga_write(BGA_ENABLE, BGA_ENABLED_WITH_LFB);
    graphics_active = 1;
}

void vga_write_char(char character)
{
    if (graphics_active) {
        draw_graphics_char(character);
        return;
    }

    if (character == '\n') {
        text_cursor += TEXT_WIDTH - (text_cursor % TEXT_WIDTH);
        if (text_cursor >= TEXT_SIZE) {
            text_cursor = 0;
        }
        return;
    }

    text_buffer[text_cursor] = ((uint16_t)VGA_COLOR_LIGHT_GREEN << 8) | glyph_index((unsigned char)character);
    text_cursor++;

    if (text_cursor == TEXT_SIZE) {
        text_cursor = 0;
    }
}

void vga_backspace(void)
{
    if (graphics_active) {
        if (graphics_cursor == 0) {
            return;
        }
        graphics_cursor--;
        clear_graphics_cell(graphics_cursor);
        return;
    }

    if (text_cursor == 0) {
        return;
    }

    text_cursor--;
    text_buffer[text_cursor] = ((uint16_t)VGA_COLOR_LIGHT_GREEN << 8) | ' ';
}

void vga_clear(void)
{
    unsigned int index;

    if (graphics_active) {
        for (index = 0; index < GRAPHICS_WIDTH * GRAPHICS_HEIGHT; index++) {
            graphics_buffer[index] = 0;
        }
        graphics_cursor = 0;
        return;
    }

    for (index = 0; index < TEXT_SIZE; index++) {
        text_buffer[index] = ((uint16_t)VGA_COLOR_LIGHT_GREEN << 8) | ' ';
    }

    text_cursor = 0;
}

void vga_write(const char *text)
{
    while (*text != '\0') {
        vga_write_char(*text);
        text++;
    }
}
