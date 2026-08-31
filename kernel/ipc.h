#ifndef EFESOS_IPC_H
#define EFESOS_IPC_H

#define IPC_QUEUE_CAPACITY 16U
#define IPC_MESSAGE_MAX 64U

void ipc_init(void);
int ipc_send(unsigned int type, const void *data, unsigned int length);
int ipc_receive(unsigned int *type, void *data, unsigned int capacity, unsigned int *length);
int ipc_send_from_to(unsigned int sender_id, unsigned int receiver_id,
    unsigned int type, const void *data, unsigned int length);
int ipc_receive_for(unsigned int receiver_id, unsigned int *type, void *data,
    unsigned int capacity, unsigned int *length);
unsigned int ipc_pending(void);
unsigned int ipc_pending_for(unsigned int receiver_id);
unsigned int ipc_purge_receiver(unsigned int receiver_id);
int ipc_self_test(void);

#endif
