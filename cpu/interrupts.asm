[bits 32]

global interrupt_test_stub
global keyboard_irq_stub
global timer_irq_stub
extern interrupt_handler
extern keyboard_irq_handler
extern pit_irq_handler

section .text

interrupt_test_stub:
    pushad
    call interrupt_handler
    popad
    iretd

keyboard_irq_stub:
    pushad
    call keyboard_irq_handler
    popad
    iretd

timer_irq_stub:
    pushad
    call pit_irq_handler
    popad
    iretd
