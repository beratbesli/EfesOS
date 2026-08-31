#ifndef EFESOS_SYSCALL_H
#define EFESOS_SYSCALL_H

struct interrupt_frame;

#define SYSCALL_GET_TICKS 0U
#define SYSCALL_YIELD 1U

void syscall_init(void);
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
unsigned int syscall_user_call_count(void);

#endif
