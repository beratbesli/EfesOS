[bits 16]

global kernel_entry
extern vga_clear
extern vga_write

CODE_SEGMENT equ 0x08
DATA_SEGMENT equ 0x10

section .text

kernel_entry:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    a32 lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword CODE_SEGMENT:protected_mode_entry

[bits 32]

protected_mode_entry:
    mov ax, DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    mov [boot_drive], dl
    call vga_clear
    push dword kernel_message
    call vga_write
    add esp, 4

.halt:
    hlt
    jmp .halt

section .rodata

kernel_message:
    db 'BeerOS: protected mode ve VGA driver hazir.', 0

section .data

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .bss

boot_drive:
    resb 1
