#include "keyboard.h"
#include "ata.h"
#include "language.h"
#include "games.h"
#include "heap.h"
#include "pmm.h"
#include "pci.h"
#include "pit.h"
#include "programs.h"
#include "ramfs.h"
#include "scheduler.h"
#include "shell.h"
#include "system.h"
#include "vga.h"

#define SHELL_INPUT_MAX 128
#define SHELL_HISTORY_MAX 8

static char input[SHELL_INPUT_MAX];
static char history[SHELL_HISTORY_MAX][SHELL_INPUT_MAX];
static unsigned int input_length;
static unsigned int history_count;

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

static int string_starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text != *prefix) {
            return 0;
        }
        text++;
        prefix++;
    }

    return 1;
}

static void copy_string(char *destination, const char *source)
{
    while (*source != '\0') {
        *destination = *source;
        destination++;
        source++;
    }
    *destination = '\0';
}

static void print_prompt(void)
{
    vga_write("efesos> ");
}

static void print_help(void)
{
    if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
        vga_write("Komutlar: help clear about mem heap input uptime ps demo pci disk counter snake slot\n");
        vga_write("echo history color ls cat write rm reboot shutdown tr en\n");
    } else {
        vga_write("Commands: help clear about mem heap input uptime ps demo pci disk counter snake slot\n");
        vga_write("echo history color ls cat write rm reboot shutdown tr en\n");
    }
}

static void print_uptime(void)
{
    pit_tick_t ticks = pit_ticks();

    if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
        vga_write("Calisma suresi: ");
    } else {
        vga_write("Uptime: ");
    }
    vga_write_unsigned(ticks / 100U);
    vga_write(" s\n");
}

static void print_processes(void)
{
    unsigned int index;

    if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
        vga_write("Gorevler:\n");
    } else {
        vga_write("Tasks:\n");
    }

    for (index = 0; index < scheduler_task_count(); index++) {
        vga_write_unsigned(index);
        vga_write(" ");
        vga_write(scheduler_task_name(index));
        vga_write(" switches=");
        vga_write_unsigned(scheduler_task_runs(index));
        vga_write_char('\n');
    }
}

static void print_demo(void)
{
    vga_write("counter=");
    vga_write_unsigned(counter_program_runs());
    vga_write(" snake=");
    vga_write_unsigned(snake_program_steps());
    vga_write_char('\n');
}

static void print_pci(void)
{
    unsigned int index;

    vga_write("PCI devices: ");
    vga_write_unsigned(pci_device_count());
    vga_write_char('\n');
    for (index = 0; index < pci_device_count(); index++) {
        const struct pci_device *device = pci_device_at(index);

        vga_write_unsigned(device->bus);
        vga_write_char(':');
        vga_write_unsigned(device->slot);
        vga_write_char('.');
        vga_write_unsigned(device->function);
        vga_write(" vendor=");
        vga_write_unsigned(device->vendor_id);
        vga_write(" device=");
        vga_write_unsigned(device->device_id);
        vga_write(" class=");
        vga_write_unsigned(device->class_code);
        vga_write_char('/');
        vga_write_unsigned(device->subclass);
        vga_write_char('\n');
    }
}

static void print_disk(void)
{
    vga_write("ATA primary master: ");
    vga_write(ata_present() ? "present sectors=" : "not present\n");
    if (ata_present()) {
        vga_write_unsigned(ata_sector_count());
        vga_write_char('\n');
    }
}

static void print_history(void)
{
    unsigned int index;

    for (index = 0; index < history_count; index++) {
        vga_write_unsigned(index + 1U);
        vga_write(": ");
        vga_write(history[index]);
        vga_write_char('\n');
    }
}

static void print_files(void)
{
    unsigned int index;

    for (index = 0; index < ramfs_file_count(); index++) {
        vga_write(ramfs_file_name(index));
        vga_write_char('\n');
    }
}

static void print_file(const char *name)
{
    const char *contents = ramfs_file_contents(name);

    if (contents == 0) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("Dosya bulunamadi.\n");
        } else {
            vga_write("File not found.\n");
        }
        return;
    }

    vga_write(contents);
}

static void write_file_command(void)
{
    char *name = input + 6;
    char *separator = name;

    while (*separator != '\0' && *separator != ' ') {
        separator++;
    }
    if (*separator == '\0') {
        vga_write("Usage: write NAME CONTENT\n");
        return;
    }
    *separator = '\0';
    if (!ramfs_write_file(name, separator + 1)) {
        vga_write("Write rejected (name/content too long or filesystem full).\n");
    }
    *separator = ' ';
}

static int set_color(const char *name)
{
    if (string_equals(name, "green")) {
        vga_set_color(VGA_COLOR_GREEN);
    } else if (string_equals(name, "white")) {
        vga_set_color(VGA_COLOR_WHITE);
    } else if (string_equals(name, "red")) {
        vga_set_color(VGA_COLOR_RED);
    } else if (string_equals(name, "blue")) {
        vga_set_color(VGA_COLOR_BLUE);
    } else if (string_equals(name, "yellow")) {
        vga_set_color(VGA_COLOR_YELLOW);
    } else {
        return 0;
    }

    return 1;
}

static void append_history(void)
{
    unsigned int index;

    if (input_length == 0) {
        return;
    }

    if (history_count == SHELL_HISTORY_MAX) {
        for (index = 1; index < SHELL_HISTORY_MAX; index++) {
            copy_string(history[index - 1], history[index]);
        }
        history_count--;
    }

    copy_string(history[history_count], input);
    history_count++;
}

static void execute_command(void)
{
    if (string_equals(input, "help")) {
        print_help();
    } else if (string_equals(input, "clear")) {
        vga_clear();
    } else if (string_equals(input, "about")) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("EfesOS x86 isletim sistemi.\n");
        } else {
            vga_write("EfesOS x86 operating system.\n");
        }
    } else if (string_equals(input, "mem")) {
        if (language_get() == SYSTEM_LANGUAGE_TURKISH) {
            vga_write("Fiziksel bellek: toplam=");
        } else {
            vga_write("Physical memory: total=");
        }
        vga_write_unsigned(pmm_total_blocks() / 256U);
        vga_write(" MiB used=");
        vga_write_unsigned(pmm_used_blocks() * 4U);
        vga_write(" KiB free=");
        vga_write_unsigned(pmm_free_blocks() / 256U);
        vga_write(" MiB\n");
    } else if (string_equals(input, "heap")) {
        vga_write("Kernel heap: mapped=");
        vga_write_unsigned(heap_mapped_bytes() / 1024U);
        vga_write(" KiB used=");
        vga_write_unsigned(heap_used_bytes());
        vga_write(" bytes\n");
    } else if (string_equals(input, "input")) {
        vga_write("Keyboard queue dropped=");
        vga_write_unsigned(keyboard_dropped_input_count());
        vga_write_char('\n');
    } else if (string_equals(input, "uptime")) {
        print_uptime();
    } else if (string_equals(input, "ps")) {
        print_processes();
    } else if (string_equals(input, "demo")) {
        print_demo();
    } else if (string_equals(input, "pci")) {
        print_pci();
    } else if (string_equals(input, "disk")) {
        print_disk();
    } else if (string_equals(input, "counter")) {
        vga_write("counter=");
        vga_write_unsigned(counter_program_runs());
        vga_write_char('\n');
    } else if (string_equals(input, "snake")) {
        games_start_snake();
    } else if (string_equals(input, "slot")) {
        games_start_slot();
    } else if (string_equals(input, "history")) {
        print_history();
    } else if (string_equals(input, "ls")) {
        print_files();
    } else if (string_starts_with(input, "cat ")) {
        print_file(input + 4);
    } else if (string_starts_with(input, "write ")) {
        write_file_command();
    } else if (string_starts_with(input, "rm ")) {
        if (!ramfs_remove_file(input + 3)) {
            vga_write("File not found or invalid name.\n");
        }
    } else if (string_starts_with(input, "echo ")) {
        vga_write(input + 5);
        vga_write_char('\n');
    } else if (string_starts_with(input, "color ")) {
        if (!set_color(input + 6)) {
            vga_write("Colors: green white red blue yellow\n");
        }
    } else if (string_equals(input, "reboot")) {
        system_reboot();
    } else if (string_equals(input, "shutdown")) {
        system_shutdown();
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
    history_count = 0;
    input[0] = '\0';
    vga_write("EfesOS shell ready. Type help.\n");
    print_prompt();
}

void shell_handle_char(unsigned char character)
{
    if (games_is_active()) {
        if (games_handle_char(character)) {
            vga_clear();
            vga_write("Game finished. Score: ");
            vga_write_unsigned(games_last_score());
            vga_write_char('\n');
            print_prompt();
        }
        return;
    }

    if (character == '\n') {
        vga_write_char('\n');
        append_history();
        execute_command();
        input_length = 0;
        input[0] = '\0';
        if (!games_is_active()) {
            print_prompt();
        }
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
