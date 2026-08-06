#include "keyboard.h"
#include "shell.h"
#include "vga.h"

#define SHELL_INPUT_MAX 128

static char input[SHELL_INPUT_MAX];
static unsigned int input_length;

static int string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }

    return *left == *right;
}

static void print_prompt(void)
{
    vga_write("beeros> ");
}

static void execute_command(void)
{
    if (string_equals(input, "help")) {
        vga_write("Komutlar: help clear about mem keymap en keymap tr\n");
    } else if (string_equals(input, "clear")) {
        vga_clear();
    } else if (string_equals(input, "about")) {
        vga_write("BeerOS x86 hobby isletim sistemi.\n");
    } else if (string_equals(input, "mem")) {
        vga_write("16 MiB fiziksel alan, ilk 4 MiB paging ile eslendi.\n");
    } else if (string_equals(input, "keymap en")) {
        keyboard_set_layout(KEYBOARD_LAYOUT_ENGLISH);
        vga_write("Klavye duzeni: English (US).\n");
    } else if (string_equals(input, "keymap tr")) {
        keyboard_set_layout(KEYBOARD_LAYOUT_TURKISH);
        vga_write("Klavye duzeni: Turkce Q.\n");
    } else if (input_length != 0) {
        vga_write("Bilinmeyen komut. help yazabilirsin.\n");
    }
}

void shell_init(void)
{
    input_length = 0;
    input[0] = '\0';
    vga_write("BeerOS shell hazir. help yazabilirsin.\n");
    print_prompt();
}

void shell_handle_char(unsigned char character)
{
    if (character == '\n') {
        vga_write_char('\n');
        execute_command();
        input_length = 0;
        input[0] = '\0';
        print_prompt();
        return;
    }

    if (character == '\b') {
        if (input_length != 0) {
            input_length--;
            input[input_length] = '\0';
            vga_backspace();
        }
        return;
    }

    if (character >= ' ' && input_length < SHELL_INPUT_MAX - 1) {
        input[input_length] = character;
        input_length++;
        input[input_length] = '\0';
        vga_write_char(character);
    }
}
