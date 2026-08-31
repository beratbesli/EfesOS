ENTRY(kernel_entry)

SECTIONS
{
    . = 0x10000;
    __kernel_start = .;

    .text ALIGN(4096) :
    {
        __text_start = .;
        *(.text*)
        __text_end = .;
    }
    .rodata ALIGN(4096) :
    {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    }
    .data ALIGN(4096) :
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }
    .bss ALIGN(4096) :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;

    ASSERT(__kernel_end < 0x80000, "kernel overlaps the reserved bootstrap stack area")
}
