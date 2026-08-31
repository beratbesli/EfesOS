#include "idt.h"
#include "ipc.h"
#include "paging.h"
#include "pit.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"

static volatile unsigned int user_call_count;
static volatile unsigned int user_pointer_reject_count;
static volatile unsigned int user_address_space_call_count;
static volatile unsigned int user_ipc_call_count;
static volatile unsigned int user_ipc_reject_count;

void syscall_init(void)
{
    user_call_count = 0;
    user_pointer_reject_count = 0;
    user_address_space_call_count = 0;
    user_ipc_call_count = 0;
    user_ipc_reject_count = 0;
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame)
{
    if (frame == 0) {
        return 0;
    }

    if ((frame->cs & 3U) == 3U) {
        user_call_count++;
        if (frame->eax == SYSCALL_IPC_SEND || frame->eax == SYSCALL_IPC_RECEIVE) {
            user_ipc_call_count++;
        }
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
    } else if (frame->eax == SYSCALL_IPC_SEND) {
        unsigned char buffer[SYSCALL_MAX_IPC];

        if (frame->edx > SYSCALL_MAX_IPC) {
            if ((frame->cs & 3U) == 3U) {
                user_ipc_reject_count++;
            }
            frame->eax = SYSCALL_E2BIG;
        } else if (!paging_copy_from_user(buffer, frame->ecx, frame->edx)) {
            if ((frame->cs & 3U) == 3U) {
                user_ipc_reject_count++;
            }
            frame->eax = SYSCALL_EFAULT;
        } else if (!ipc_send(frame->ebx, buffer, frame->edx)) {
            frame->eax = SYSCALL_EAGAIN;
        } else {
            frame->eax = frame->edx;
        }
    } else if (frame->eax == SYSCALL_IPC_RECEIVE) {
        unsigned char buffer[SYSCALL_MAX_IPC];
        unsigned int message_type;
        unsigned int message_length;

        if (frame->ecx > SYSCALL_MAX_IPC ||
            !paging_validate_user_range(frame->ebx, frame->ecx, 1) ||
            !paging_validate_user_range(frame->edx, sizeof(message_type), 1)) {
            if ((frame->cs & 3U) == 3U) {
                user_ipc_reject_count++;
            }
            frame->eax = frame->ecx > SYSCALL_MAX_IPC ? SYSCALL_E2BIG : SYSCALL_EFAULT;
        } else if (!ipc_receive(&message_type, buffer, frame->ecx, &message_length)) {
            frame->eax = ipc_pending() != 0U ? SYSCALL_E2BIG : SYSCALL_EAGAIN;
        } else if (!paging_copy_to_user(frame->ebx, buffer, message_length) ||
                   !paging_copy_to_user(frame->edx, &message_type, sizeof(message_type))) {
            frame->eax = SYSCALL_EFAULT;
        } else {
            frame->eax = message_length;
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

unsigned int syscall_user_ipc_call_count(void)
{
    return user_ipc_call_count;
}

unsigned int syscall_user_ipc_reject_count(void)
{
    return user_ipc_reject_count;
}
