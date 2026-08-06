#include "keyboard.h"
#include "language.h"
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
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("Komutlar: help clear about mem tr en\n");
        } else {
            vga_write("Commands: help clear about mem tr en\n");
        }
    } else if (string_equals(input, "clear")) {
        vga_clear();
    } else if (string_equals(input, "about")) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("BeerOS x86 isletim sistemi.\n");
        } else {
            vga_write("BeerOS x86 operating system.\n");
        }
    } else if (string_equals(input, "mem")) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("16 MiB fiziksel alan, ilk 4 MiB paging ile eslendi.\n");
        } else {
            vga_write("16 MiB physical memory, first 4 MiB mapped with paging.\n");
        }
    } else if (string_equals(input, "en")) {
        language_set(SYSTEM_LANGUAGE_ENGLISH);
        keyboard_set_layout(KEYBOARD_LAYOUT_ENGLISH);
        vga_write("Language and keyboard: English (US).\n");
    } else if (string_equals(input, "tr")) {
        language_set(SYSTEM_LANGUAGE_TURKISH);
        keyboard_set_layout(KEYBOARD_LAYOUT_TURKISH);
        vga_write("Dil ve klavye: Turkce Q.\n");
    } else if (input_length != 0) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("Bilinmeyen komut. help yazabilirsin.\n");
        } else {
            vga_write("Unknown command. Type help.\n");
        }
    }
}

void shell_init(void)
{
    input_length = 0;
    input[0] = '\0';
    vga_write("BeerOS shell ready. Type help.\n");
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
