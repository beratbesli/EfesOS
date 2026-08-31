#ifndef EFESOS_HEAP_H
#define EFESOS_HEAP_H

int heap_init(void);
int heap_self_test(void);
void *kmalloc(unsigned int size);
void *kcalloc(unsigned int count, unsigned int size);
void kfree(void *pointer);
unsigned int heap_mapped_bytes(void);
unsigned int heap_used_bytes(void);

#endif
