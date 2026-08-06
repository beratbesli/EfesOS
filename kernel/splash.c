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
    ",ggggggggggg,                                *,gggggg,*         ,gg,",
    "dP\"\"\"88\"\"\"\"\"\"Y8,                            ,d8P\"\"d8P\"Y8b,      i8\"\"8i",
    "Yb,  88      `8b                           ,d8'   Y8   \"8b,dP   `8,,8'",
    " `\"  88      ,8P                           d8'    `Ybaaad88P'    `88'",
    "     88aaaad8P\"                            8P       `\"\"\"\"Y8      dP\"8,",
    "     88\"\"\"\"Y8ba  ,ggg,    ,ggg,    ,gggggg,8b            d8     dP' `8a",
    "     88      `8bi8\" \"8i  i8\" \"8i   dP\"\"\"\"8IY8,          ,8P    dP'   `Yb",
    "     88      ,8PI8, ,8I  I8, ,8I  ,8'    8I`Y8,        ,8P'_ ,dP'     I8",
    "     88_____,d8'`YbadP'  `YbadP' ,dP     Y8,`Y8b,,__,,d8P' \"888,,____,dP",
    "    88888888P\"  888P\"Y888888P\"Y8888P      `Y8  `\"Y8888P\"'   a8P\"Y88888P\""
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
        if (row >= 2 && row < 12) {
            for (column = text_length(splash_left[row]); column < 42; column++) {
                vga_write_char(' ');
            }
            vga_write(splash_right[row - 2]);
        }
        vga_write_char('\n');
    }

    vga_write_char('\n');
}
