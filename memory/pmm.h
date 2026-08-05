#ifndef BEEROS_PMM_H
#define BEEROS_PMM_H

typedef unsigned int uint32_t;

void pmm_init(void);
uint32_t pmm_alloc_block(void);
void pmm_free_block(uint32_t address);
int pmm_self_test(void);

#endif
