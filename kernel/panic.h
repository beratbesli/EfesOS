#ifndef EFESOS_PANIC_H
#define EFESOS_PANIC_H

__attribute__((noreturn)) void kernel_panic(const char *message);

#endif
