#include "idt.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"

void syscall_init(void)
{
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame)
{
    if (frame == 0) {
        return 0;
    }

    if (frame->eax == SYSCALL_GET_TICKS) {
        frame->eax = pit_ticks();
    } else if (frame->eax == SYSCALL_YIELD) {
        frame = scheduler_on_yield(frame);
    } else {
        frame->eax = 0xFFFFFFFFU;
    }
    return frame;
}
