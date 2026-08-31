#ifndef EFESOS_ELF_LOADER_H
#define EFESOS_ELF_LOADER_H

int elf_validate_image(const void *image, unsigned int size, unsigned int *entry);
int elf_load_image(const void *image, unsigned int size, unsigned int *entry,
    unsigned int *loaded_base, unsigned int *loaded_end);
int elf_unload_image(unsigned int loaded_base, unsigned int loaded_end);
int elf_loader_self_test(void);
int elf_loader_runtime_self_test(void);

#endif
