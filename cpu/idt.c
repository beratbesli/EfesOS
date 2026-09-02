#include "idt.h"
#include "ata.h"
#include "io.h"
#include "keyboard.h"
#include "panic.h"
#include "pit.h"
#include "serial.h"
#include "scheduler.h"
#include "syscall.h"
#include "vga.h"

typedef unsigned short idt_u16_t;

#define IDT_ENTRY_COUNT 256U
#define PIC_MASTER_COMMAND 0x20U
#define PIC_MASTER_DATA 0x21U
#define PIC_SLAVE_COMMAND 0xA0U
#define PIC_SLAVE_DATA 0xA1U
#define PIC_EOI 0x20U
#define PIC_READ_ISR 0x0BU
#define IRQ_BASE 32U
#define IRQ_LIMIT 48U

struct idt_entry {
    idt_u16_t offset_low;
    idt_u16_t selector;
    uint8_t zero;
    uint8_t type_attributes;
    idt_u16_t offset_high;
} __attribute__((packed));

struct idt_descriptor {
    idt_u16_t limit;
    interrupt_u32_t base;
} __attribute__((packed));

extern void (*interrupt_stub_table[])(void);

static struct idt_entry idt_entries[IDT_ENTRY_COUNT];
static struct idt_descriptor idt_descriptor;
static uint8_t pic_master_mask = 0xFFU;
static uint8_t pic_slave_mask = 0xFFU;
static int idt_ready;

static const char *const exception_names[32] = {
    "Divide error", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "BOUND range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point exception", "Alignment check", "Machine check", "SIMD floating-point exception",
    "Virtualization exception", "Control protection exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection exception", "VMM communication exception", "Security exception", "Reserved"
};

static void io_wait(void)
{
    outb(0x80, 0);
}

static void pic_remap(void)
{
    outb(PIC_MASTER_COMMAND, 0x11);
    io_wait();
    outb(PIC_SLAVE_COMMAND, 0x11);
    io_wait();
    outb(PIC_MASTER_DATA, IRQ_BASE);
    io_wait();
    outb(PIC_SLAVE_DATA, IRQ_BASE + 8U);
    io_wait();
    outb(PIC_MASTER_DATA, 0x04);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x02);
    io_wait();
    outb(PIC_MASTER_DATA, 0x01);
    io_wait();
    outb(PIC_SLAVE_DATA, 0x01);
    io_wait();
    /* Keep every hardware line masked until its driver has completed
       initialization. This prevents an early timer/device interrupt from
       entering scheduler or driver state that is not ready yet. */
    pic_master_mask = 0xFFU;
    pic_slave_mask = 0xFFU;
    outb(PIC_MASTER_DATA, pic_master_mask);
    outb(PIC_SLAVE_DATA, pic_slave_mask);
}

static interrupt_u32_t interrupt_save(void)
{
    interrupt_u32_t flags;

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void interrupt_restore(interrupt_u32_t flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

static uint8_t pic_read_isr(unsigned short command_port)
{
    outb(command_port, PIC_READ_ISR);
    return inb(command_port);
}

static int irq_is_spurious(interrupt_u32_t irq)
{
    if (irq == 7U) {
        return (pic_read_isr(PIC_MASTER_COMMAND) & 0x80U) == 0U;
    }
    if (irq == 15U && (pic_read_isr(PIC_SLAVE_COMMAND) & 0x80U) == 0U) {
        outb(PIC_MASTER_COMMAND, PIC_EOI);
        return 1;
    }
    return 0;
}

static void pic_acknowledge(interrupt_u32_t irq)
{
    if (irq >= 8U) {
        outb(PIC_SLAVE_COMMAND, PIC_EOI);
    }
    outb(PIC_MASTER_COMMAND, PIC_EOI);
}

static void idt_set_gate(uint8_t vector, interrupt_u32_t address)
{
    idt_entries[vector].offset_low = (idt_u16_t)(address & 0xFFFFU);
    idt_entries[vector].selector = 0x08;
    idt_entries[vector].zero = 0;
    idt_entries[vector].type_attributes = 0x8E;
    idt_entries[vector].offset_high = (idt_u16_t)((address >> 16U) & 0xFFFFU);
}

static void idt_set_user_gate(uint8_t vector, interrupt_u32_t address)
{
    idt_set_gate(vector, address);
    idt_entries[vector].type_attributes = 0xEE;
}

void idt_init(void)
{
    interrupt_u32_t vector;

    idt_ready = 0;
    for (vector = 0; vector < IDT_ENTRY_COUNT; vector++) {
        idt_entries[vector].offset_low = 0;
        idt_entries[vector].selector = 0;
        idt_entries[vector].zero = 0;
        idt_entries[vector].type_attributes = 0;
        idt_entries[vector].offset_high = 0;
    }
    for (vector = 0; vector < IDT_ENTRY_COUNT; vector++) {
        idt_set_gate((uint8_t)vector, (interrupt_u32_t)interrupt_stub_table[vector]);
    }
    idt_set_user_gate(0x80U, (interrupt_u32_t)interrupt_stub_table[0x80U]);

    idt_descriptor.limit = sizeof(idt_entries) - 1U;
    idt_descriptor.base = (interrupt_u32_t)idt_entries;
    pic_remap();
    __asm__ volatile ("lidtl %0" : : "m"(idt_descriptor));
    idt_ready = 1;
}

int idt_enable_irq_line(interrupt_u32_t irq)
{
    interrupt_u32_t flags;

    if (!idt_ready || (irq != 0U && irq != 1U && irq != 14U)) {
        return 0;
    }
    flags = interrupt_save();
    if (irq >= 8U) {
        pic_slave_mask &= (uint8_t)~(1U << (irq - 8U));
        outb(PIC_SLAVE_DATA, pic_slave_mask);
        pic_master_mask &= (uint8_t)~(1U << 2U);
        outb(PIC_MASTER_DATA, pic_master_mask);
    } else {
        pic_master_mask &= (uint8_t)~(1U << irq);
        outb(PIC_MASTER_DATA, pic_master_mask);
    }
    interrupt_restore(flags);
    return 1;
}

int idt_irq_line_enabled(interrupt_u32_t irq)
{
    if (!idt_ready || irq >= 16U) {
        return 0;
    }
    if (irq >= 8U) {
        return (pic_master_mask & (1U << 2U)) == 0U &&
            (pic_slave_mask & (1U << (irq - 8U))) == 0U;
    }
    return (pic_master_mask & (1U << irq)) == 0U;
}

static struct interrupt_frame *handle_exception(struct interrupt_frame *frame)
{
    interrupt_u32_t fault_address = 0;
    const char *name = "Unknown CPU exception";

    if (frame->vector < 32U) {
        name = exception_names[frame->vector];
    }

    serial_write("EXCEPTION vector=");
    serial_write_hex(frame->vector);
    serial_write(" error=");
    serial_write_hex(frame->error_code);
    serial_write(" eip=");
    serial_write_hex(frame->eip);
    if (frame->vector == 14U) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
        serial_write(" cr2=");
        serial_write_hex(fault_address);
    }
    serial_write("\n");

    if (frame->vector == 3U && (frame->cs & 3U) != 3U) {
        serial_write("EfesOS: breakpoint exception self-test passed.\n");
        return frame;
    }
    if ((frame->cs & 3U) == 3U) {
        serial_write("EfesOS: user exception isolated.\n");
        return scheduler_on_user_fault(frame);
    }

    vga_write("CPU exception: ");
    vga_write(name);
    vga_write(" vector=");
    vga_write_unsigned(frame->vector);
    vga_write(" error=");
    vga_write_unsigned(frame->error_code);
    vga_write("\n");
    kernel_panic(name);
    return frame;
}

static struct interrupt_frame *handle_irq(struct interrupt_frame *frame)
{
    interrupt_u32_t irq = frame->vector - IRQ_BASE;

    if (irq_is_spurious(irq)) {
        return frame;
    }

    if (irq == 0U) {
        pit_irq_handler();
    } else if (irq == 1U) {
        keyboard_irq_handler();
    } else if (irq == 14U) {
        ata_irq_handler();
    }
    pic_acknowledge(irq);
    if (irq == 0U) {
        return scheduler_on_timer(frame);
    }
    return frame;
}

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame == 0) {
        kernel_panic("Null interrupt frame.");
    }
    if (frame->vector < 32U) {
        return handle_exception(frame);
    } else if (frame->vector < IRQ_LIMIT) {
        return handle_irq(frame);
    } else if (frame->vector == 48U) {
        serial_write("EfesOS: software interrupt handler running.\n");
        return frame;
    } else if (frame->vector == 49U) {
        return scheduler_on_yield(frame);
    } else if (frame->vector == 0x80U) {
        return syscall_dispatch(frame);
    } else {
        kernel_panic("Unexpected interrupt vector.");
    }
    return frame;
}
