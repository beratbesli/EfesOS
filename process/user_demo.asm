[bits 32]

global user_demo_start
global user_demo_end

; Position-independent ring-3 probe. It exercises both a read-only syscall and
; an explicit yield without touching kernel addresses or assuming libc.
user_demo_start:
.loop:
    mov eax, 0
    int 0x80
    ; Deliberately cross a supervisor-only page. The kernel must terminate this
    ; task and continue running other tasks instead of panicking globally.
    mov dword [0x00010000], eax
    jmp .loop
user_demo_end:
