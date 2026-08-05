#include "io.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x0E] = '\b', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x39] = ' '
};

static unsigned char shift_active;

static void acknowledge_master_pic(void)
{
    outb(0x20, 0x20);
}

void keyboard_init(void)
{
}

void keyboard_irq_handler(void)
{
    unsigned char scan_code = inb(0x60);
    unsigned char key_code = scan_code & 0x7F;
    char character;

    if (key_code == 0x2A || key_code == 0x36) {
        shift_active = (scan_code & 0x80) == 0;
        acknowledge_master_pic();
        return;
    }

    if ((scan_code & 0x80) != 0) {
        acknowledge_master_pic();
        return;
    }

    character = keymap[key_code];
    if (character >= 'a' && character <= 'z' && shift_active != 0) {
        character -= 'a' - 'A';
    }

    if (character != '\0') {
        shell_handle_char(character);
    }

    acknowledge_master_pic();
}
