#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "elf_loader.h"
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

static void set_u16(unsigned char *data, unsigned int offset, unsigned short value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)(value >> 8U);
}

static void set_u32(unsigned char *data, unsigned int offset, unsigned int value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)((value >> 8U) & 0xFFU);
    data[offset + 2U] = (unsigned char)((value >> 16U) & 0xFFU);
    data[offset + 3U] = (unsigned char)(value >> 24U);
}

int user_process_init(void)
{
    unsigned int code_size = (unsigned int)(&user_demo_end - &user_demo_start);
    unsigned int stack_frame;
    unsigned char image[4096];
    unsigned int image_size = 116U + code_size;
    unsigned int entry;
    unsigned int loaded_base = 0;
    unsigned int loaded_end = 0;
    int image_loaded = 0;
    unsigned int index;

    if (code_size == 0U || code_size > PAGE_SIZE || paging_is_mapped(USER_CODE_ADDRESS) ||
        paging_is_mapped(USER_STACK_ADDRESS) || image_size > sizeof(image)) {
        return 0;
    }
    stack_frame = pmm_alloc_block();
    if (stack_frame == 0U ||
        !paging_map_page(USER_STACK_ADDRESS, stack_frame, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        if (paging_is_mapped(USER_STACK_ADDRESS)) {
            paging_unmap_page(USER_STACK_ADDRESS);
        }
        if (stack_frame != 0U) {
            pmm_free_block(stack_frame);
        }
        return 0;
    }
    for (index = 0; index < sizeof(image); index++) {
        image[index] = 0;
    }
    image[0] = 0x7F;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1;
    image[5] = 1;
    image[6] = 1;
    set_u16(image, 16U, 2U);
    set_u16(image, 18U, 3U);
    set_u32(image, 20U, 1U);
    set_u32(image, 24U, USER_CODE_ADDRESS);
    set_u32(image, 28U, 52U);
    set_u16(image, 42U, 32U);
    set_u16(image, 44U, 1U);
    set_u32(image, 52U, 1U);
    set_u32(image, 56U, 116U);
    set_u32(image, 60U, USER_CODE_ADDRESS);
    set_u32(image, 68U, code_size);
    set_u32(image, 72U, PAGE_SIZE);
    set_u32(image, 76U, 1U);
    set_u32(image, 80U, 1U);
    copy_bytes(image + 116U, &user_demo_start, code_size);
    image_loaded = elf_load_image(image, image_size, &entry, &loaded_base, &loaded_end);
    if (!image_loaded ||
        entry != USER_CODE_ADDRESS || loaded_base != USER_CODE_ADDRESS ||
        loaded_end != USER_CODE_ADDRESS + PAGE_SIZE ||
        !scheduler_add_user_task("user-demo", entry, USER_STACK_ADDRESS + PAGE_SIZE)) {
        if (image_loaded) {
            elf_unload_image(loaded_base, loaded_end);
        }
        paging_unmap_page(USER_STACK_ADDRESS);
        pmm_free_block(stack_frame);
        return 0;
    }
    return 1;
}
