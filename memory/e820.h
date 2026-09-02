#ifndef EFESOS_E820_H
#define EFESOS_E820_H

#include "boot_info.h"

typedef unsigned long long e820_u64_t;
typedef void (*e820_range_handler_t)(e820_u64_t base, e820_u64_t length,
    void *context);

/* Applies usable ranges first and every active non-usable range second.
   The ordering gives reserved firmware regions precedence over overlapping
   usable records without trusting the order returned by BIOS. */
int e820_apply_memory_map(const struct boot_info *info,
    e820_range_handler_t usable_handler,
    e820_range_handler_t reserved_handler,
    void *context);

#endif
