#include "ipc.h"

struct ipc_message {
    unsigned int type;
    unsigned int length;
    unsigned char data[IPC_MESSAGE_MAX];
};

static struct ipc_message queue[IPC_QUEUE_CAPACITY];
static unsigned int head;
static unsigned int tail;
static unsigned int count;

static unsigned int irq_save(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0\n\tpopfl" : : "r"(flags) : "memory");
}

void ipc_init(void)
{
    unsigned int flags = irq_save();
    head = 0U;
    tail = 0U;
    count = 0U;
    irq_restore(flags);
}

int ipc_send(unsigned int type, const void *data, unsigned int length)
{
    unsigned int flags;
    unsigned int i;

    if (length > IPC_MESSAGE_MAX || (length != 0U && data == 0)) {
        return 0;
    }

    flags = irq_save();
    if (count >= IPC_QUEUE_CAPACITY) {
        irq_restore(flags);
        return 0;
    }
    queue[head].type = type;
    queue[head].length = length;
    for (i = 0U; i < length; i++) {
        queue[head].data[i] = ((const unsigned char *)data)[i];
    }
    head = (head + 1U) % IPC_QUEUE_CAPACITY;
    count++;
    irq_restore(flags);
    return 1;
}

int ipc_receive(unsigned int *type, void *data, unsigned int capacity, unsigned int *length)
{
    unsigned int flags;
    unsigned int i;
    unsigned int message_length;

    flags = irq_save();
    if (count == 0U) {
        irq_restore(flags);
        return 0;
    }
    message_length = queue[tail].length;
    if (message_length > capacity || (message_length != 0U && data == 0)) {
        irq_restore(flags);
        return 0;
    }
    if (type != 0) {
        *type = queue[tail].type;
    }
    if (length != 0) {
        *length = message_length;
    }
    for (i = 0U; i < message_length; i++) {
        ((unsigned char *)data)[i] = queue[tail].data[i];
    }
    tail = (tail + 1U) % IPC_QUEUE_CAPACITY;
    count--;
    irq_restore(flags);
    return 1;
}

unsigned int ipc_pending(void)
{
    unsigned int flags = irq_save();
    unsigned int pending = count;
    irq_restore(flags);
    return pending;
}

static int bytes_equal(const unsigned char *left, const unsigned char *right, unsigned int length)
{
    unsigned int i;

    for (i = 0U; i < length; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}

int ipc_self_test(void)
{
    static const unsigned char first[] = {'o', 'n', 'e'};
    static const unsigned char second[] = {'t', 'w', 'o'};
    static const unsigned char oversized[IPC_MESSAGE_MAX + 1U] = {0};
    unsigned char output[IPC_MESSAGE_MAX];
    unsigned int type;
    unsigned int length;
    unsigned int i;

    ipc_init();
    if (ipc_send(0U, oversized, sizeof(oversized)) || ipc_send(0U, 0, 1U) ||
        !ipc_send(1U, first, sizeof(first)) || !ipc_send(2U, second, sizeof(second)) ||
        ipc_pending() != 2U) {
        return 0;
    }
    if (!ipc_receive(&type, output, sizeof(output), &length) || type != 1U ||
        length != sizeof(first) || !bytes_equal(output, first, length)) {
        return 0;
    }
    if (!ipc_receive(&type, output, sizeof(output), &length) || type != 2U ||
        length != sizeof(second) || !bytes_equal(output, second, length)) {
        return 0;
    }
    for (i = 0U; i < IPC_QUEUE_CAPACITY; i++) {
        if (!ipc_send(i, 0, 0U)) {
            return 0;
        }
    }
    if (ipc_send(IPC_QUEUE_CAPACITY, 0, 0U) || ipc_pending() != IPC_QUEUE_CAPACITY) {
        return 0;
    }
    for (i = 0U; i < IPC_QUEUE_CAPACITY; i++) {
        if (!ipc_receive(&type, output, sizeof(output), &length) || type != i || length != 0U) {
            return 0;
        }
    }
    return ipc_pending() == 0U;
}
