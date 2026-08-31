[bits 32]

global user_demo_start
global user_demo_end

; Position-independent ring-3 probe. It exercises both a read-only syscall and
; an explicit yield without touching kernel addresses or assuming libc.
user_demo_start:
.loop:
    mov eax, 0
    int 0x80
    mov eax, 1
    int 0x80
    jmp .loop
user_demo_end:
