#include "elf_loader.h"

#define ELF32_HEADER_SIZE 52U
#define ELF32_PROGRAM_HEADER_SIZE 32U
#define ELF_IDENT_OFFSET 0U
#define ELF_TYPE_OFFSET 16U
#define ELF_MACHINE_OFFSET 18U
#define ELF_VERSION_OFFSET 20U
#define ELF_ENTRY_OFFSET 24U
#define ELF_PHOFF_OFFSET 28U
#define ELF_PHENTSIZE_OFFSET 42U
#define ELF_PHNUM_OFFSET 44U
#define ELF_PT_TYPE_OFFSET 0U
#define ELF_PT_OFFSET_OFFSET 4U
#define ELF_PT_VADDR_OFFSET 8U
#define ELF_PT_FILESZ_OFFSET 16U
#define ELF_PT_MEMSZ_OFFSET 20U
#define ELF_PT_FLAGS_OFFSET 24U
#define ELF_PT_ALIGN_OFFSET 28U
#define ELF_PT_LOAD 1U
#define ELF_ET_EXEC 2U
#define ELF_EM_386 3U
#define ELF_CLASS_32 1U
#define ELF_DATA_LSB 1U
#define ELF_VERSION_CURRENT 1U
#define ELF_FLAG_EXECUTABLE 1U
#define ELF_FLAG_WRITABLE 2U
#define USER_MIN_ADDRESS 0x00400000U
#define USER_MAX_ADDRESS 0x40000000U
#define ELF_MAX_PROGRAM_HEADERS 32U

static unsigned short read_u16(const unsigned char *data, unsigned int offset)
{
    return (unsigned short)data[offset] | ((unsigned short)data[offset + 1U] << 8U);
}

static unsigned int read_u32(const unsigned char *data, unsigned int offset)
{
    return (unsigned int)data[offset] | ((unsigned int)data[offset + 1U] << 8U) |
        ((unsigned int)data[offset + 2U] << 16U) | ((unsigned int)data[offset + 3U] << 24U);
}

static int range_is_inside(unsigned int offset, unsigned int length, unsigned int size)
{
    return offset <= size && length <= size - offset;
}

int elf_validate_image(const void *image, unsigned int size, unsigned int *entry)
{
    const unsigned char *data = (const unsigned char *)image;
    unsigned int phoff;
    unsigned int phnum;
    unsigned int index;
    unsigned int load_segments = 0;
    int entry_executable = 0;

    if (data == 0 || size < ELF32_HEADER_SIZE || data[ELF_IDENT_OFFSET] != 0x7FU ||
        data[1] != 'E' || data[2] != 'L' || data[3] != 'F' || data[4] != ELF_CLASS_32 ||
        data[5] != ELF_DATA_LSB || data[6] != ELF_VERSION_CURRENT ||
        read_u16(data, ELF_TYPE_OFFSET) != ELF_ET_EXEC ||
        read_u16(data, ELF_MACHINE_OFFSET) != ELF_EM_386 ||
        read_u32(data, ELF_VERSION_OFFSET) != ELF_VERSION_CURRENT ||
        read_u16(data, ELF_PHENTSIZE_OFFSET) != ELF32_PROGRAM_HEADER_SIZE) {
        return 0;
    }
    phoff = read_u32(data, ELF_PHOFF_OFFSET);
    phnum = read_u16(data, ELF_PHNUM_OFFSET);
    if (phnum == 0U || phnum > ELF_MAX_PROGRAM_HEADERS ||
        !range_is_inside(phoff, phnum * ELF32_PROGRAM_HEADER_SIZE, size)) {
        return 0;
    }
    if (entry != 0) {
        *entry = read_u32(data, ELF_ENTRY_OFFSET);
    }
    for (index = 0; index < phnum; index++) {
        unsigned int offset = phoff + index * ELF32_PROGRAM_HEADER_SIZE;
        unsigned int type = read_u32(data, offset + ELF_PT_TYPE_OFFSET);
        unsigned int file_offset;
        unsigned int virtual_address;
        unsigned int file_size;
        unsigned int memory_size;
        unsigned int flags;
        unsigned int alignment;

        if (type != ELF_PT_LOAD) {
            continue;
        }
        file_offset = read_u32(data, offset + ELF_PT_OFFSET_OFFSET);
        virtual_address = read_u32(data, offset + ELF_PT_VADDR_OFFSET);
        file_size = read_u32(data, offset + ELF_PT_FILESZ_OFFSET);
        memory_size = read_u32(data, offset + ELF_PT_MEMSZ_OFFSET);
        flags = read_u32(data, offset + ELF_PT_FLAGS_OFFSET);
        alignment = read_u32(data, offset + ELF_PT_ALIGN_OFFSET);
        if (memory_size < file_size || !range_is_inside(file_offset, file_size, size) ||
            virtual_address < USER_MIN_ADDRESS || memory_size > USER_MAX_ADDRESS - virtual_address ||
            ((flags & ELF_FLAG_WRITABLE) != 0U && (flags & ELF_FLAG_EXECUTABLE) != 0U) ||
            (alignment != 0U && alignment != 1U && alignment != 0x1000U) ||
            (flags & ~(ELF_FLAG_EXECUTABLE | ELF_FLAG_WRITABLE | 4U)) != 0U) {
            return 0;
        }
        if (file_size != 0U) {
            load_segments++;
        }
        if ((flags & ELF_FLAG_EXECUTABLE) != 0U &&
            read_u32(data, ELF_ENTRY_OFFSET) >= virtual_address &&
            read_u32(data, ELF_ENTRY_OFFSET) - virtual_address < memory_size) {
            entry_executable = 1;
        }
    }
    return load_segments != 0U && entry_executable;
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

int elf_loader_self_test(void)
{
    unsigned char image[128];
    unsigned int entry;
    unsigned int index;

    for (index = 0; index < sizeof(image); index++) {
        image[index] = 0;
    }
    image[0] = 0x7F;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = ELF_CLASS_32;
    image[5] = ELF_DATA_LSB;
    image[6] = ELF_VERSION_CURRENT;
    set_u16(image, ELF_TYPE_OFFSET, ELF_ET_EXEC);
    set_u16(image, ELF_MACHINE_OFFSET, ELF_EM_386);
    set_u32(image, ELF_VERSION_OFFSET, ELF_VERSION_CURRENT);
    set_u32(image, ELF_ENTRY_OFFSET, USER_MIN_ADDRESS);
    set_u32(image, ELF_PHOFF_OFFSET, ELF32_HEADER_SIZE);
    set_u16(image, ELF_PHENTSIZE_OFFSET, ELF32_PROGRAM_HEADER_SIZE);
    set_u16(image, ELF_PHNUM_OFFSET, 1);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_TYPE_OFFSET, ELF_PT_LOAD);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_OFFSET_OFFSET, 116U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_VADDR_OFFSET, USER_MIN_ADDRESS);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FILESZ_OFFSET, 4U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_MEMSZ_OFFSET, 4096U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET, ELF_FLAG_EXECUTABLE);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_ALIGN_OFFSET, 0x1000U);
    image[116] = 0xC3;
    if (!elf_validate_image(image, sizeof(image), &entry) || entry != USER_MIN_ADDRESS) {
        return 0;
    }
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET, ELF_FLAG_EXECUTABLE | ELF_FLAG_WRITABLE);
    if (elf_validate_image(image, sizeof(image), 0)) {
        return 0;
    }
    return 1;
}
