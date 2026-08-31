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
    dq 0x00CFFA000000FFFF
    dq 0x00CFF2000000FFFF
    dw tss_end - tss - 1
    dw 0
    db 0
    db 0x89
    db (tss_end - tss - 1) >> 16
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

global tss_init
tss_init:
    mov eax, tss
    mov word [gdt_start + 42], ax
    shr eax, 16
    mov byte [gdt_start + 44], al
    mov byte [gdt_start + 47], ah
    mov dword [tss + 4], 0x00090000
    mov word [tss + 8], DATA_SEGMENT
    mov word [tss + 102], 104
    mov ax, 0x28
    ltr ax
    ret

align 4
tss:
    times 104 db 0
tss_end:
