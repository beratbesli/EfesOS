#include "elf_loader.h"
#include "paging.h"
#include "pmm.h"

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
#define ELF_MAX_IMAGE_PAGES 1024U
#define ELF_PAGE_MASK (~(PAGE_SIZE - 1U))

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
    unsigned int load_pages = 0;
    unsigned int image_entry;
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
    image_entry = read_u32(data, ELF_ENTRY_OFFSET);
    if (entry != 0) {
        *entry = image_entry;
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
        unsigned int segment_end;
        unsigned int page_start;
        unsigned int page_end;

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
        if (memory_size == 0U) {
            continue;
        }
        segment_end = virtual_address + memory_size;
        page_start = virtual_address & ELF_PAGE_MASK;
        page_end = (segment_end + PAGE_SIZE - 1U) & ELF_PAGE_MASK;
        if (page_end < segment_end || page_start < USER_MIN_ADDRESS || page_end > USER_MAX_ADDRESS ||
            load_pages > ELF_MAX_IMAGE_PAGES - ((page_end - page_start) / PAGE_SIZE)) {
            return 0;
        }
        if (alignment == 0x1000U && (file_offset & (PAGE_SIZE - 1U)) !=
            (virtual_address & (PAGE_SIZE - 1U))) {
            return 0;
        }
        load_pages += (page_end - page_start) / PAGE_SIZE;
        if (file_size != 0U) {
            load_segments++;
        }
        if ((flags & ELF_FLAG_EXECUTABLE) != 0U &&
            image_entry >= virtual_address && image_entry - virtual_address < memory_size) {
            entry_executable = 1;
        }
    }
    return load_segments != 0U && entry_executable;
}

static void release_pages(const unsigned int *pages, unsigned int count)
{
    unsigned int index;

    for (index = 0; index < count; index++) {
        unsigned int physical = paging_unmap_page(pages[index]);
        if (physical != 0U) {
            pmm_free_block(physical);
        }
    }
}

int elf_load_image(const void *image, unsigned int size, unsigned int *entry,
    unsigned int *loaded_base, unsigned int *loaded_end)
{
    const unsigned char *data = (const unsigned char *)image;
    unsigned int mapped_pages[ELF_MAX_IMAGE_PAGES];
    unsigned int mapped_count = 0;
    unsigned int phoff;
    unsigned int phnum;
    unsigned int index;
    unsigned int image_entry;
    unsigned int image_base = USER_MAX_ADDRESS;
    unsigned int image_end = USER_MIN_ADDRESS;

    if (!elf_validate_image(image, size, &image_entry)) {
        return 0;
    }
    phoff = read_u32(data, ELF_PHOFF_OFFSET);
    phnum = read_u16(data, ELF_PHNUM_OFFSET);
    for (index = 0; index < phnum; index++) {
        unsigned int offset = phoff + index * ELF32_PROGRAM_HEADER_SIZE;
        unsigned int type = read_u32(data, offset + ELF_PT_TYPE_OFFSET);
        unsigned int file_offset;
        unsigned int virtual_address;
        unsigned int file_size;
        unsigned int memory_size;
        unsigned int flags;
        unsigned int segment_end;
        unsigned int page;
        unsigned int page_end;

        if (type != ELF_PT_LOAD) {
            continue;
        }
        file_offset = read_u32(data, offset + ELF_PT_OFFSET_OFFSET);
        virtual_address = read_u32(data, offset + ELF_PT_VADDR_OFFSET);
        file_size = read_u32(data, offset + ELF_PT_FILESZ_OFFSET);
        memory_size = read_u32(data, offset + ELF_PT_MEMSZ_OFFSET);
        flags = read_u32(data, offset + ELF_PT_FLAGS_OFFSET);
        if (memory_size == 0U) {
            continue;
        }
        segment_end = virtual_address + memory_size;
        page = virtual_address & ELF_PAGE_MASK;
        page_end = (segment_end + PAGE_SIZE - 1U) & ELF_PAGE_MASK;
        if (page_end < segment_end || page < USER_MIN_ADDRESS || page_end > USER_MAX_ADDRESS) {
            release_pages(mapped_pages, mapped_count);
            return 0;
        }
        if (page < image_base) {
            image_base = page;
        }
        if (page_end > image_end) {
            image_end = page_end;
        }
        for (; page < page_end; page += PAGE_SIZE) {
            unsigned int physical;
            if (mapped_count == ELF_MAX_IMAGE_PAGES || paging_is_mapped(page)) {
                release_pages(mapped_pages, mapped_count);
                return 0;
            }
            physical = pmm_alloc_block();
            if (physical == 0U || !paging_map_page(page, physical,
                PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
                if (physical != 0U) {
                    pmm_free_block(physical);
                }
                release_pages(mapped_pages, mapped_count);
                return 0;
            }
            mapped_pages[mapped_count++] = page;
            {
                unsigned int byte;
                for (byte = 0; byte < PAGE_SIZE; byte++) {
                    ((unsigned char *)page)[byte] = 0;
                }
            }
        }
        for (page = 0; page < file_size; page++) {
            ((unsigned char *)(virtual_address + page))[0] = data[file_offset + page];
        }
        for (page = 0; page < (page_end - (virtual_address + file_size)); page++) {
            ((unsigned char *)(virtual_address + file_size + page))[0] = 0;
        }
        for (page = virtual_address & ELF_PAGE_MASK; page < page_end; page += PAGE_SIZE) {
            unsigned int page_flags = PAGE_FLAG_USER;
            if ((flags & ELF_FLAG_WRITABLE) != 0U) {
                page_flags |= PAGE_FLAG_WRITABLE;
            }
            if (!paging_protect_page(page, page_flags)) {
                release_pages(mapped_pages, mapped_count);
                return 0;
            }
        }
    }
    if (entry != 0) {
        *entry = image_entry;
    }
    if (loaded_base != 0) {
        *loaded_base = image_base;
    }
    if (loaded_end != 0) {
        *loaded_end = image_end;
    }
    return 1;
}

int elf_unload_image(unsigned int loaded_base, unsigned int loaded_end)
{
    unsigned int page;

    if (loaded_base < USER_MIN_ADDRESS || loaded_base >= loaded_end ||
        (loaded_base & (PAGE_SIZE - 1U)) != 0U ||
        loaded_end > USER_MAX_ADDRESS || (loaded_end & (PAGE_SIZE - 1U)) != 0U) {
        return 0;
    }
    for (page = loaded_base; page < loaded_end; page += PAGE_SIZE) {
        unsigned int physical;
        if (!paging_is_mapped(page)) {
            continue;
        }
        physical = paging_unmap_page(page);
        if (physical != 0U) {
            pmm_free_block(physical);
        }
    }
    return 1;
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
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_ALIGN_OFFSET, 1U);
    image[116] = 0xC3;
    if (!elf_validate_image(image, sizeof(image), &entry) || entry != USER_MIN_ADDRESS) {
        return 0;
    }
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET, ELF_FLAG_EXECUTABLE | ELF_FLAG_WRITABLE);
    if (elf_validate_image(image, sizeof(image), 0)) {
        return 0;
    }
    /* Exercise independent malformed-header/segment cases so future parser
       changes cannot silently remove one of the overflow and policy guards. */
    {
        static const unsigned int malformed_offsets[] = {
            ELF_PHOFF_OFFSET, ELF_PHNUM_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_OFFSET_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_VADDR_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_FILESZ_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_MEMSZ_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET,
            ELF32_HEADER_SIZE + ELF_PT_ALIGN_OFFSET, ELF_ENTRY_OFFSET
        };
        static const unsigned int malformed_values[] = {
            0xFFFFFFFFU, 0U, 0xFFFFFFFFU, 0x003FF000U, 0xFFFFFFFFU,
            3U, 8U, 2U, USER_MIN_ADDRESS + PAGE_SIZE
        };
        unsigned int malformed_index;

        for (malformed_index = 0U;
             malformed_index < sizeof(malformed_offsets) / sizeof(malformed_offsets[0]);
             malformed_index++) {
            unsigned int value = malformed_values[malformed_index];

            for (index = 0U; index < sizeof(image); index++) {
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
            set_u16(image, ELF_PHNUM_OFFSET, 1U);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_TYPE_OFFSET, ELF_PT_LOAD);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_OFFSET_OFFSET, 116U);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_VADDR_OFFSET, USER_MIN_ADDRESS);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FILESZ_OFFSET, 4U);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_MEMSZ_OFFSET, 4096U);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET, ELF_FLAG_EXECUTABLE);
            set_u32(image, ELF32_HEADER_SIZE + ELF_PT_ALIGN_OFFSET, 1U);
            image[116] = 0xC3;
            if (malformed_offsets[malformed_index] == ELF_PHNUM_OFFSET) {
                set_u16(image, malformed_offsets[malformed_index], (unsigned short)value);
            } else {
                set_u32(image, malformed_offsets[malformed_index], value);
            }
            if (elf_validate_image(image, sizeof(image), 0)) {
                return 0;
            }
        }
    }
    return 1;
}

int elf_loader_runtime_self_test(void)
{
    unsigned char image[128];
    unsigned int entry;
    unsigned int loaded_base;
    unsigned int loaded_end;
    unsigned int index;
    unsigned char *loaded;
    int image_loaded;

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
    set_u32(image, ELF_ENTRY_OFFSET, 0x01000000U);
    set_u32(image, ELF_PHOFF_OFFSET, ELF32_HEADER_SIZE);
    set_u16(image, ELF_PHENTSIZE_OFFSET, ELF32_PROGRAM_HEADER_SIZE);
    set_u16(image, ELF_PHNUM_OFFSET, 1);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_TYPE_OFFSET, ELF_PT_LOAD);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_OFFSET_OFFSET, 116U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_VADDR_OFFSET, 0x01000000U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FILESZ_OFFSET, 4U);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_MEMSZ_OFFSET, PAGE_SIZE);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_FLAGS_OFFSET, ELF_FLAG_EXECUTABLE);
    set_u32(image, ELF32_HEADER_SIZE + ELF_PT_ALIGN_OFFSET, 1U);
    image[116] = 0xC3;
    image[117] = 0xEF;
    image[118] = 0x05;
    image[119] = 0xA5;
    image_loaded = elf_load_image(image, sizeof(image), &entry, &loaded_base, &loaded_end);
    if (!image_loaded ||
        entry != 0x01000000U || loaded_base != 0x01000000U ||
        loaded_end != 0x01001000U || !paging_is_mapped(loaded_base)) {
        if (image_loaded) {
            elf_unload_image(loaded_base, loaded_end);
        }
        return 0;
    }
    loaded = (unsigned char *)entry;
    if (loaded[0] != 0xC3 || loaded[1] != 0xEF || loaded[2] != 0x05 || loaded[3] != 0xA5 ||
        loaded[4] != 0U || !elf_unload_image(loaded_base, loaded_end) ||
        paging_is_mapped(loaded_base)) {
        elf_unload_image(loaded_base, loaded_end);
        return 0;
    }
    return 1;
}
