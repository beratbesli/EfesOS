[bits 32]

global interrupt_test_stub
extern interrupt_handler

section .text

interrupt_test_stub:
    pushad
    call interrupt_handler
    popad
    iretd
