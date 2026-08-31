[bits 32]

global user_demo_start
global user_demo_end

; Position-independent ring-3 probe. It exercises both a read-only syscall and
; an explicit yield without touching kernel addresses or assuming libc.
user_demo_start:
.loop:
    call .get_pc
.get_pc:
    pop esi
    lea ebx, [esi + msg - .get_pc]
    mov ecx, msg_end - msg
    mov eax, 2
    int 0x80
    ; Invalid supervisor address: the syscall must reject it without touching
    ; kernel memory or raising a kernel fault.
    mov ebx, 0x00010000
    mov ecx, 1
    mov eax, 2
    int 0x80
    mov eax, 0
    int 0x80
    ; Deliberately cross a supervisor-only page. The kernel must terminate this
    ; task and continue running other tasks instead of panicking globally.
    mov dword [0x00010000], eax
    jmp .loop

msg db 'EfesOS ring3 buffer syscall passed.', 13, 10
msg_end:
user_demo_end:
