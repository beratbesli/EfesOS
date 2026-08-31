#ifndef EFESOS_ELF_LOADER_H
#define EFESOS_ELF_LOADER_H

int elf_validate_image(const void *image, unsigned int size, unsigned int *entry);
int elf_loader_self_test(void);

#endif
