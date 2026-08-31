[bits 16]

global kernel_entry
extern kernel_main
extern __bss_start
extern __bss_end

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

    ; C requires all objects in .bss to start as zero. The raw kernel image does
    ; not contain SHT_NOBITS bytes, so the loader cannot do this for us.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    mov esp, 0x90000
    push esi
    call kernel_main
    add esp, 4

.halt:
    hlt
    jmp .halt

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
