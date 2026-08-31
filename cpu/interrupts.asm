[bits 32]

extern interrupt_dispatch

section .text

%macro VECTOR_NO_ERROR 1
global interrupt_stub_%1
interrupt_stub_%1:
    push dword 0
    push dword %1
    jmp interrupt_common
%endmacro

%macro VECTOR_WITH_ERROR 1
global interrupt_stub_%1
interrupt_stub_%1:
    push dword %1
    jmp interrupt_common
%endmacro

VECTOR_NO_ERROR 0
VECTOR_NO_ERROR 1
VECTOR_NO_ERROR 2
VECTOR_NO_ERROR 3
VECTOR_NO_ERROR 4
VECTOR_NO_ERROR 5
VECTOR_NO_ERROR 6
VECTOR_NO_ERROR 7
VECTOR_WITH_ERROR 8
VECTOR_NO_ERROR 9
VECTOR_WITH_ERROR 10
VECTOR_WITH_ERROR 11
VECTOR_WITH_ERROR 12
VECTOR_WITH_ERROR 13
VECTOR_WITH_ERROR 14
VECTOR_NO_ERROR 15
VECTOR_NO_ERROR 16
VECTOR_WITH_ERROR 17
VECTOR_NO_ERROR 18
VECTOR_NO_ERROR 19
VECTOR_NO_ERROR 20
VECTOR_WITH_ERROR 21
VECTOR_NO_ERROR 22
VECTOR_NO_ERROR 23
VECTOR_NO_ERROR 24
VECTOR_NO_ERROR 25
VECTOR_NO_ERROR 26
VECTOR_NO_ERROR 27
VECTOR_NO_ERROR 28
VECTOR_WITH_ERROR 29
VECTOR_WITH_ERROR 30
VECTOR_NO_ERROR 31

%assign vector 32
%rep 18
VECTOR_NO_ERROR vector
%assign vector vector + 1
%endrep

%assign vector 50
%rep 79
VECTOR_NO_ERROR vector
%assign vector vector + 1
%endrep

interrupt_common:
    cld
    pushad

    xor eax, eax
    mov ax, ds
    push eax
    xor eax, eax
    mov ax, es
    push eax
    xor eax, eax
    mov ax, fs
    push eax
    xor eax, eax
    mov ax, gs
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_dispatch
    add esp, 4
    test eax, eax
    jnz .use_new_frame
    mov eax, esp
.use_new_frame:
    mov esp, eax

    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax
    pop eax
    mov ds, ax

    popad
    add esp, 8
    iretd

section .rodata

global interrupt_stub_table
interrupt_stub_table:
%assign vector 0
%rep 129
    dd interrupt_stub_%+vector
%assign vector vector + 1
%endrep
