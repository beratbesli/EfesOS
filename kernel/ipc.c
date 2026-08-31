#include "ipc.h"
#include "scheduler.h"

struct ipc_message {
    unsigned int sender_id;
    unsigned int receiver_id;
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

int ipc_send_from_to(unsigned int sender_id, unsigned int receiver_id,
    unsigned int type, const void *data, unsigned int length)
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
    queue[head].sender_id = sender_id;
    queue[head].receiver_id = receiver_id;
    queue[head].length = length;
    for (i = 0U; i < length; i++) {
        queue[head].data[i] = ((const unsigned char *)data)[i];
    }
    head = (head + 1U) % IPC_QUEUE_CAPACITY;
    count++;
    irq_restore(flags);
    if (receiver_id != 0U) {
        scheduler_wake_task_id(receiver_id);
    }
    return 1;
}

int ipc_send(unsigned int type, const void *data, unsigned int length)
{
    return ipc_send_from_to(0U, 0U, type, data, length);
}

static int message_matches(const struct ipc_message *message, unsigned int receiver_id)
{
    return receiver_id == 0U || message->receiver_id == 0U ||
        message->receiver_id == receiver_id;
}

static void copy_message(struct ipc_message *destination, const struct ipc_message *source)
{
    unsigned int index;

    destination->sender_id = source->sender_id;
    destination->receiver_id = source->receiver_id;
    destination->type = source->type;
    destination->length = source->length;
    for (index = 0U; index < IPC_MESSAGE_MAX; index++) {
        destination->data[index] = source->data[index];
    }
}

int ipc_receive_for(unsigned int receiver_id, unsigned int *type, void *data,
    unsigned int capacity, unsigned int *length)
{
    unsigned int flags;
    unsigned int i;
    unsigned int offset;
    unsigned int position;
    unsigned int message_length;

    flags = irq_save();
    for (offset = 0U; offset < count; offset++) {
        position = (tail + offset) % IPC_QUEUE_CAPACITY;
        if (message_matches(&queue[position], receiver_id)) {
            break;
        }
    }
    if (offset == count) {
        irq_restore(flags);
        return 0;
    }
    position = (tail + offset) % IPC_QUEUE_CAPACITY;
    message_length = queue[position].length;
    if (message_length > capacity || (message_length != 0U && data == 0)) {
        irq_restore(flags);
        return 0;
    }
    if (type != 0) {
        *type = queue[position].type;
    }
    if (length != 0) {
        *length = message_length;
    }
    for (i = 0U; i < message_length; i++) {
        ((unsigned char *)data)[i] = queue[position].data[i];
    }
    /* Remove an interior message while retaining FIFO order for all others. */
    while (offset != 0U) {
        unsigned int previous = (position + IPC_QUEUE_CAPACITY - 1U) % IPC_QUEUE_CAPACITY;
        copy_message(&queue[position], &queue[previous]);
        position = previous;
        offset--;
    }
    tail = (tail + 1U) % IPC_QUEUE_CAPACITY;
    count--;
    irq_restore(flags);
    return 1;
}

int ipc_receive(unsigned int *type, void *data, unsigned int capacity, unsigned int *length)
{
    return ipc_receive_for(0U, type, data, capacity, length);
}

unsigned int ipc_pending(void)
{
    unsigned int flags = irq_save();
    unsigned int pending = count;
    irq_restore(flags);
    return pending;
}

unsigned int ipc_pending_for(unsigned int receiver_id)
{
    unsigned int flags = irq_save();
    unsigned int offset;
    unsigned int pending = 0U;

    for (offset = 0U; offset < count; offset++) {
        unsigned int position = (tail + offset) % IPC_QUEUE_CAPACITY;
        if (message_matches(&queue[position], receiver_id)) {
            pending++;
        }
    }
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
    if (ipc_pending() != 0U) {
        return 0;
    }
    ipc_init();
    if (!ipc_send_from_to(0x101U, 0x202U, 7U, first, sizeof(first)) ||
        !ipc_send_from_to(0x303U, 0x404U, 8U, second, sizeof(second)) ||
        !ipc_send(9U, 0, 0U) || ipc_pending_for(0x202U) != 2U ||
        ipc_pending_for(0x404U) != 2U) {
        return 0;
    }
    if (!ipc_receive_for(0x202U, &type, output, sizeof(output), &length) ||
        type != 7U || length != sizeof(first) || !bytes_equal(output, first, length) ||
        ipc_pending_for(0x202U) != 1U) {
        return 0;
    }
    if (!ipc_receive_for(0x404U, &type, output, sizeof(output), &length) ||
        type != 8U || length != sizeof(second) || !bytes_equal(output, second, length) ||
        ipc_pending_for(0x404U) != 1U) {
        return 0;
    }
    return ipc_receive_for(0x505U, &type, output, sizeof(output), &length) &&
        type == 9U && length == 0U && ipc_pending() == 0U;
}
