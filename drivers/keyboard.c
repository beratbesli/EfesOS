#include "io.h"
#include "keyboard.h"
#include "shell.h"

static const unsigned char english_keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x1C] = '\n',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' '
};

static const unsigned char english_shift_keymap[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b', [0x1C] = '\n',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D',
    [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x39] = ' '
};

static const unsigned char turkish_keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '*', [0x0D] = '-', [0x0E] = '\b', [0x1C] = '\n',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 0xFD, [0x18] = 'o', [0x19] = 'p',
    [0x1A] = 0xF0, [0x1B] = 0xFC, [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = 0xFE, [0x28] = 'i', [0x29] = '"', [0x2B] = ',',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = 0xF6, [0x34] = 0xE7, [0x35] = '.',
    [0x39] = ' '
};

static const unsigned char turkish_shift_keymap[128] = {
    [0x02] = '!', [0x03] = '"', [0x04] = '^', [0x05] = '+', [0x06] = '%',
    [0x07] = '&', [0x08] = '/', [0x09] = '(', [0x0A] = ')', [0x0B] = '=',
    [0x0C] = '?', [0x0D] = '_', [0x0E] = '\b', [0x1C] = '\n',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = 0xD0, [0x1B] = 0xDC, [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D',
    [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = 0xDE, [0x28] = 0xDD, [0x29] = 0xE9, [0x2B] = ';',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = 0xD6, [0x34] = 0xC7, [0x35] = ':',
    [0x39] = ' '
};

static const unsigned char *active_keymap = english_keymap;
static const unsigned char *active_shift_keymap = english_shift_keymap;
static unsigned char left_shift_active;
static unsigned char right_shift_active;

void keyboard_init(void)
{
    keyboard_set_layout(KEYBOARD_LAYOUT_ENGLISH);
    left_shift_active = 0;
    right_shift_active = 0;
}

void keyboard_set_layout(enum keyboard_layout layout)
{
    if (layout == KEYBOARD_LAYOUT_TURKISH) {
        active_keymap = turkish_keymap;
        active_shift_keymap = turkish_shift_keymap;
        return;
    }

    active_keymap = english_keymap;
    active_shift_keymap = english_shift_keymap;
}

void keyboard_irq_handler(void)
{
    unsigned char scan_code = inb(0x60);
    unsigned char key_code = scan_code & 0x7F;
    unsigned char character;

    if (key_code == 0x2A) {
        left_shift_active = (scan_code & 0x80) == 0;
        return;
    }

    if (key_code == 0x36) {
        right_shift_active = (scan_code & 0x80) == 0;
        return;
    }

    if ((scan_code & 0x80) != 0) {
        return;
    }

    if (left_shift_active != 0 || right_shift_active != 0) {
        character = active_shift_keymap[key_code];
    } else {
        character = active_keymap[key_code];
    }

    if (character != '\0') {
        shell_handle_char(character);
    }
}
