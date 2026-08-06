#include "splash.h"
#include "vga.h"

static const char *const splash_left[] = {
    "             \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "          \xDB\xDB\xDB\xDB       \xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "         \xDB\xDB               \xDB\xDB",
    "        \xDB\xDB           \xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "        \xDB\xDB \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB    \xDB\xDB\xDB\xDB\xDB\xDB",
    "         \xDB\xDB\xDB\xDB\xDB\xDB \xDB\xDB\xDB\xDB      \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "                          \xDB\xDB    \xDB\xDB",
    "                          \xDB\xDB    \xDB\xDB",
    "          \xDB\xDB              \xDB\xDB    \xDB\xDB",
    "          \xDB\xDB              \xDB\xDB    \xDB\xDB",
    "          \xDB\xDB              \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "          \xDB\xDB              \xDB\xDB\xDB\xDB\xDB\xDB",
    "          \xDB\xDB              \xDB\xDB",
    "          \xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB"
};

static const char *const splash_right[] = {
    " ___               ___  ___ ",
    "| _ ) ___ ___ _ _ / _ \\/ __|",
    "| _ \\/ -*) -*) '*| (*) \\__ \\",
    "|***/\\_\\_\\_\\_\\_\\_|\\_|  \\_\\_\\_\\_/|***/"
};

void splash_show(void)
{
    unsigned int row;

    for (row = 0; row < 14; row++) {
        vga_write(splash_left[row]);
        vga_write_char('\n');
    }

    vga_write_char('\n');
    for (row = 0; row < 4; row++) {
        vga_write("          ");
        vga_write(splash_right[row]);
        vga_write_char('\n');
    }
}
