#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "user_process.h"

#define USER_CODE_ADDRESS 0x00400000U
#define USER_STACK_ADDRESS 0x00800000U

extern unsigned char user_demo_start;
extern unsigned char user_demo_end;

static void copy_bytes(unsigned char *destination, const unsigned char *source, unsigned int length)
{
    unsigned int index;

    for (index = 0; index < length; index++) {
        destination[index] = source[index];
    }
}

int user_process_init(void)
{
    unsigned int code_size = (unsigned int)(&user_demo_end - &user_demo_start);
    unsigned int code_frame;
    unsigned int stack_frame;

    if (code_size == 0U || code_size > PAGE_SIZE || paging_is_mapped(USER_CODE_ADDRESS) ||
        paging_is_mapped(USER_STACK_ADDRESS)) {
        return 0;
    }
    code_frame = pmm_alloc_block();
    stack_frame = pmm_alloc_block();
    if (code_frame == 0U || stack_frame == 0U ||
        !paging_map_page(USER_CODE_ADDRESS, code_frame, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE) ||
        !paging_map_page(USER_STACK_ADDRESS, stack_frame, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        if (paging_is_mapped(USER_CODE_ADDRESS)) {
            paging_unmap_page(USER_CODE_ADDRESS);
        }
        if (paging_is_mapped(USER_STACK_ADDRESS)) {
            paging_unmap_page(USER_STACK_ADDRESS);
        }
        if (code_frame != 0U) {
            pmm_free_block(code_frame);
        }
        if (stack_frame != 0U) {
            pmm_free_block(stack_frame);
        }
        return 0;
    }
    copy_bytes((unsigned char *)USER_CODE_ADDRESS, &user_demo_start, code_size);
    if (!paging_protect_page(USER_CODE_ADDRESS, PAGE_FLAG_USER) ||
        !scheduler_add_user_task("user-demo", USER_CODE_ADDRESS, USER_STACK_ADDRESS + PAGE_SIZE)) {
        paging_unmap_page(USER_CODE_ADDRESS);
        paging_unmap_page(USER_STACK_ADDRESS);
        pmm_free_block(code_frame);
        pmm_free_block(stack_frame);
        return 0;
    }
    return 1;
}
