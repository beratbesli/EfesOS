#ifndef EFESOS_SYSCALL_H
#define EFESOS_SYSCALL_H

struct interrupt_frame;

#define SYSCALL_GET_TICKS 0U
#define SYSCALL_YIELD 1U
#define SYSCALL_WRITE 2U
#define SYSCALL_MAX_WRITE 128U
#define SYSCALL_EFAULT 0xFFFFFFF2U
#define SYSCALL_E2BIG 0xFFFFFFFBU

void syscall_init(void);
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
unsigned int syscall_user_call_count(void);
unsigned int syscall_user_pointer_reject_count(void);

#endif
