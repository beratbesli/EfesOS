#ifndef EFESOS_IPC_H
#define EFESOS_IPC_H

#define IPC_QUEUE_CAPACITY 16U
#define IPC_MESSAGE_MAX 64U

void ipc_init(void);
int ipc_send(unsigned int type, const void *data, unsigned int length);
int ipc_receive(unsigned int *type, void *data, unsigned int capacity, unsigned int *length);
unsigned int ipc_pending(void);
int ipc_self_test(void);

#endif
