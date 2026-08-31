#ifndef EFESOS_IDT_H
#define EFESOS_IDT_H

typedef unsigned int interrupt_u32_t;

struct interrupt_frame {
    interrupt_u32_t gs;
    interrupt_u32_t fs;
    interrupt_u32_t es;
    interrupt_u32_t ds;
    interrupt_u32_t edi;
    interrupt_u32_t esi;
    interrupt_u32_t ebp;
    interrupt_u32_t esp_before_pushad;
    interrupt_u32_t ebx;
    interrupt_u32_t edx;
    interrupt_u32_t ecx;
    interrupt_u32_t eax;
    interrupt_u32_t vector;
    interrupt_u32_t error_code;
    interrupt_u32_t eip;
    interrupt_u32_t cs;
    interrupt_u32_t eflags;
};

void idt_init(void);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);

#endif
