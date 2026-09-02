#ifndef EFESOS_CPU_FEATURES_H
#define EFESOS_CPU_FEATURES_H

struct cpu_features {
    unsigned int cpuid;
    unsigned int pae;
    unsigned int nx;
    unsigned int tsc;
    unsigned int rdrand;
};

void cpu_features_init(void);
const struct cpu_features *cpu_features_get(void);
int cpu_random_u32(unsigned int *value);

#endif
