#include "idt.h"
#include "paging.h"
#include "pit.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"

static volatile unsigned int user_call_count;
static volatile unsigned int user_pointer_reject_count;
static volatile unsigned int user_address_space_call_count;

void syscall_init(void)
{
    user_call_count = 0;
    user_pointer_reject_count = 0;
    user_address_space_call_count = 0;
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame)
{
    if (frame == 0) {
        return 0;
    }

    if ((frame->cs & 3U) == 3U) {
        user_call_count++;
        if (paging_current_directory() != paging_kernel_directory()) {
            user_address_space_call_count++;
        }
    }

    if (frame->eax == SYSCALL_GET_TICKS) {
        frame->eax = pit_ticks();
    } else if (frame->eax == SYSCALL_YIELD) {
        frame = scheduler_on_yield(frame);
    } else if (frame->eax == SYSCALL_WRITE) {
        char buffer[SYSCALL_MAX_WRITE];

        if (frame->ecx > SYSCALL_MAX_WRITE) {
            frame->eax = SYSCALL_E2BIG;
        } else if (!paging_copy_from_user(buffer, frame->ebx, frame->ecx)) {
            if ((frame->cs & 3U) == 3U) {
                user_pointer_reject_count++;
            }
            frame->eax = SYSCALL_EFAULT;
        } else {
            serial_write_n(buffer, frame->ecx);
            frame->eax = frame->ecx;
        }
    } else {
        frame->eax = 0xFFFFFFFFU;
    }
    return frame;
}

unsigned int syscall_user_call_count(void)
{
    return user_call_count;
}

unsigned int syscall_user_pointer_reject_count(void)
{
    return user_pointer_reject_count;
}

unsigned int syscall_user_address_space_call_count(void)
{
    return user_address_space_call_count;
}
