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

    /* Keep the image below the VGA/option-ROM window while leaving bounded
       headroom for loader and security self-tests. */
    ASSERT(__kernel_end < 0x90000, "kernel overlaps the reserved low-memory area")
}
