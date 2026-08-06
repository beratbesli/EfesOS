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
    "|***/\\_\\_\\_\\_\\_\\_|\\_|  \\_\\_\\_\\_/|***/",
    ""
};

static unsigned int text_length(const char *text)
{
    unsigned int length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

void splash_show(void)
{
    unsigned int row;

    for (row = 0; row < 14; row++) {
        unsigned int column;

        vga_write(splash_left[row]);
        if (row < 5) {
            for (column = text_length(splash_left[row]); column < 48; column++) {
                vga_write_char(' ');
            }
            vga_write(splash_right[row]);
        }
        vga_write_char('\n');
    }
}
