#include "idt.h"
#include "vga.h"
#include "io.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attributes;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void interrupt_test_stub(void);
extern void keyboard_irq_stub(void);

static struct idt_entry idt_entries[256];
static struct idt_descriptor idt_descriptor;

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

static void idt_set_gate(uint8_t vector, uint32_t address)
{
    idt_entries[vector].offset_low = (uint16_t)(address & 0xFFFF);
    idt_entries[vector].selector = 0x08;
    idt_entries[vector].zero = 0;
    idt_entries[vector].type_attributes = 0x8E;
    idt_entries[vector].offset_high = (uint16_t)((address >> 16) & 0xFFFF);
}

void idt_init(void)
{
    idt_set_gate(0x30, (uint32_t)interrupt_test_stub);
    idt_set_gate(0x21, (uint32_t)keyboard_irq_stub);
    idt_descriptor.limit = sizeof(idt_entries) - 1;
    idt_descriptor.base = (uint32_t)idt_entries;
    pic_remap();
    __asm__ volatile ("lidtl %0" : : "m"(idt_descriptor));
}

void interrupt_handler(void)
{
    vga_write("BeerOS: IDT ve kesme handler calisiyor.");
}
