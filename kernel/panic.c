#include "panic.h"
#include "serial.h"
#include "vga.h"

__attribute__((noreturn)) void kernel_panic(const char *message)
{
    __asm__ volatile ("cli" : : : "memory");

    serial_write("KERNEL PANIC: ");
    serial_write(message);
    serial_write("\n");

    vga_set_color(VGA_COLOR_RED);
    vga_write("\nKERNEL PANIC: ");
    vga_write(message);
    vga_write("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
