#include "idt.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"

static volatile unsigned int user_call_count;

void syscall_init(void)
{
    user_call_count = 0;
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame)
{
    if (frame == 0) {
        return 0;
    }

    if ((frame->cs & 3U) == 3U) {
        user_call_count++;
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

unsigned int syscall_user_call_count(void)
{
    return user_call_count;
}
