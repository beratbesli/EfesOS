#include "vga.h"
#include "pci.h"

enum {
    TEXT_WIDTH = 80,
    TEXT_HEIGHT = 25,
    TEXT_SIZE = TEXT_WIDTH * TEXT_HEIGHT,
    GRAPHICS_WIDTH = 1024,
    GRAPHICS_HEIGHT = 768,
    GRAPHICS_COLUMNS = GRAPHICS_WIDTH / 8,
    GRAPHICS_ROWS = GRAPHICS_HEIGHT / 16,
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
    VGA_PCI_COMMAND_IO_AND_MEMORY = 0x0003,
    VGA_VENDOR_ID = 0x1234,
    VGA_DEVICE_ID = 0x1111,
    VGA_COLOR_LIGHT_GREEN = 0x0A
};

static volatile uint16_t *const text_buffer = (uint16_t *)0xB8000;
static volatile uint32_t *const graphics_buffer = (uint32_t *)VBE_FRAMEBUFFER_VIRTUAL;
static volatile uint8_t *font_data;
static unsigned short text_cursor;
static unsigned short graphics_cursor;
static int graphics_active;
static uint8_t text_color = VGA_COLOR_LIGHT_GREEN;
static uint32_t graphics_color = 0x0000FF00U;
static enum vga_color active_color = VGA_COLOR_GREEN;

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

static void bga_write(uint16_t index, uint16_t value)
{
    outw(BGA_INDEX_PORT, index);
    outw(BGA_DATA_PORT, value);
}

static uint16_t bga_read(uint16_t index)
{
    outw(BGA_INDEX_PORT, index);
    return inw(BGA_DATA_PORT);
}

static uint32_t pci_config_address(const struct pci_device *device, uint8_t offset)
{
    return 0x80000000U | ((uint32_t)device->bus << 16U) |
        ((uint32_t)device->slot << 11U) | ((uint32_t)device->function << 8U) |
        ((uint32_t)offset & 0xFCU);
}

static const struct pci_device *find_bga_device(void)
{
    unsigned int index;

    for (index = 0U; index < pci_device_count(); index++) {
        const struct pci_device *device = pci_device_at(index);
        if (device != 0 && device->vendor_id == VGA_VENDOR_ID &&
            device->device_id == VGA_DEVICE_ID && device->class_code == 0x03U) {
            return device;
        }
    }
    return 0;
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

static void scroll_graphics(void)
{
    unsigned int index;
    unsigned int visible_pixels = GRAPHICS_WIDTH * (GRAPHICS_HEIGHT - 16);
    unsigned int row_pixels = GRAPHICS_WIDTH * 16;

    for (index = 0; index < visible_pixels; index++) {
        graphics_buffer[index] = graphics_buffer[index + row_pixels];
    }

    for (; index < GRAPHICS_WIDTH * GRAPHICS_HEIGHT; index++) {
        graphics_buffer[index] = 0;
    }
}

static void scroll_text(void)
{
    unsigned int index;

    for (index = 0; index < TEXT_SIZE - TEXT_WIDTH; index++) {
        text_buffer[index] = text_buffer[index + TEXT_WIDTH];
    }

    for (; index < TEXT_SIZE; index++) {
        text_buffer[index] = ((uint16_t)text_color << 8) | ' ';
    }
}

static void graphics_newline(void)
{
    graphics_cursor += GRAPHICS_COLUMNS - (graphics_cursor % GRAPHICS_COLUMNS);
    if (graphics_cursor >= GRAPHICS_COLUMNS * GRAPHICS_ROWS) {
        scroll_graphics();
        graphics_cursor = (GRAPHICS_ROWS - 1) * GRAPHICS_COLUMNS;
    }
}

static void text_newline(void)
{
    text_cursor += TEXT_WIDTH - (text_cursor % TEXT_WIDTH);
    if (text_cursor >= TEXT_SIZE) {
        scroll_text();
        text_cursor = (TEXT_HEIGHT - 1) * TEXT_WIDTH;
    }
}

static void draw_graphics_char(char character)
{
    unsigned short row;
    unsigned short column;
    unsigned short y;

    if (character == '\n') {
        graphics_newline();
        return;
    }

    if (graphics_cursor == GRAPHICS_COLUMNS * GRAPHICS_ROWS) {
        scroll_graphics();
        graphics_cursor = (GRAPHICS_ROWS - 1) * GRAPHICS_COLUMNS;
    }

    row = graphics_cursor / GRAPHICS_COLUMNS;
    column = graphics_cursor % GRAPHICS_COLUMNS;
    for (y = 0; y < 16; y++) {
        unsigned short x;
        uint8_t pixels = glyph_row((unsigned char)character, y);

        for (x = 0; x < 8; x++) {
            uint32_t color = (pixels & (0x80U >> x)) ? graphics_color : 0;
            graphics_buffer[((row * 16U + y) * GRAPHICS_WIDTH) + (column * 8U + x)] = color;
        }
    }

    graphics_cursor++;
    if (graphics_cursor == GRAPHICS_COLUMNS * GRAPHICS_ROWS) {
        graphics_cursor = 0;
    }
}

void vga_init(const struct boot_info *boot_info)
{
    const struct pci_device *device;

    graphics_active = 0;
    if (boot_info == 0 ||
        (boot_info->video_flags & BOOT_VIDEO_FONT_AVAILABLE) == 0U ||
        boot_info->vga_font_address < 0x1000U ||
        boot_info->vga_font_address >= 0x003FF000U) {
        return;
    }

    device = find_bga_device();
    if (device == 0) {
        return;
    }

    font_data = (volatile uint8_t *)boot_info->vga_font_address;
    outl(PCI_CONFIG_ADDRESS_PORT, pci_config_address(device, 0x10U));
    outl(PCI_CONFIG_DATA_PORT, VBE_FRAMEBUFFER_VIRTUAL);
    outl(PCI_CONFIG_ADDRESS_PORT, pci_config_address(device, 0x04U));
    outw(PCI_CONFIG_DATA_PORT, VGA_PCI_COMMAND_IO_AND_MEMORY);
    bga_write(BGA_ID, BGA_ID4);
    if (bga_read(BGA_ID) != BGA_ID4) {
        return;
    }
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
        text_newline();
        return;
    }

    if (text_cursor == TEXT_SIZE) {
        scroll_text();
        text_cursor = (TEXT_HEIGHT - 1) * TEXT_WIDTH;
    }

    text_buffer[text_cursor] = ((uint16_t)text_color << 8) | glyph_index((unsigned char)character);
    text_cursor++;

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
    text_buffer[text_cursor] = ((uint16_t)text_color << 8) | ' ';
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
        text_buffer[index] = ((uint16_t)text_color << 8) | ' ';
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

void vga_write_unsigned(unsigned int value)
{
    char digits[10];
    unsigned int count = 0;

    if (value == 0) {
        vga_write_char('0');
        return;
    }

    while (value != 0) {
        digits[count] = '0' + (value % 10U);
        count++;
        value /= 10U;
    }

    while (count != 0) {
        count--;
        vga_write_char(digits[count]);
    }
}

void vga_set_color(enum vga_color color)
{
    active_color = color;

    if (color == VGA_COLOR_WHITE) {
        text_color = 0x0F;
        graphics_color = 0x00FFFFFFU;
    } else if (color == VGA_COLOR_RED) {
        text_color = 0x0C;
        graphics_color = 0x00FF0000U;
    } else if (color == VGA_COLOR_BLUE) {
        text_color = 0x09;
        graphics_color = 0x000000FFU;
    } else if (color == VGA_COLOR_YELLOW) {
        text_color = 0x0E;
        graphics_color = 0x00FFFF00U;
    } else {
        text_color = VGA_COLOR_LIGHT_GREEN;
        graphics_color = 0x0000FF00U;
        active_color = VGA_COLOR_GREEN;
    }
}

enum vga_color vga_get_color(void)
{
    return active_color;
}
