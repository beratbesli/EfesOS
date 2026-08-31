#ifndef EFESOS_SYSCALL_H
#define EFESOS_SYSCALL_H

struct interrupt_frame;

#define SYSCALL_GET_TICKS 0U
#define SYSCALL_YIELD 1U
#define SYSCALL_WRITE 2U
#define SYSCALL_IPC_SEND 3U
#define SYSCALL_IPC_RECEIVE 4U
#define SYSCALL_GET_PID 5U
#define SYSCALL_IPC_SEND_TO 6U
#define SYSCALL_MAX_WRITE 128U
#define SYSCALL_MAX_IPC 64U
#define SYSCALL_EFAULT 0xFFFFFFF2U
#define SYSCALL_E2BIG 0xFFFFFFFBU
#define SYSCALL_EAGAIN 0xFFFFFFF5U

void syscall_init(void);
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
unsigned int syscall_user_call_count(void);
unsigned int syscall_user_pointer_reject_count(void);
unsigned int syscall_user_address_space_call_count(void);
unsigned int syscall_user_ipc_call_count(void);
unsigned int syscall_user_ipc_reject_count(void);
unsigned int syscall_user_ipc_target_count(void);
unsigned int syscall_user_pid_call_count(void);

#endif
