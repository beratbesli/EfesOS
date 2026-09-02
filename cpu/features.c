#include "features.h"

static struct cpu_features detected_features;

static int cpuid_supported(void)
{
    unsigned int original;
    unsigned int toggled;
    unsigned int observed;

    __asm__ volatile("pushfl\n\tpopl %0" : "=r"(original));
    toggled = original ^ (1U << 21U);
    __asm__ volatile("pushl %0\n\tpopfl" : : "r"(toggled) : "cc");
    __asm__ volatile("pushfl\n\tpopl %0" : "=r"(observed));
    __asm__ volatile("pushl %0\n\tpopfl" : : "r"(original) : "cc");
    return ((observed ^ original) & (1U << 21U)) != 0U;
}

static void read_cpuid(unsigned int leaf, unsigned int *eax, unsigned int *ebx,
    unsigned int *ecx, unsigned int *edx)
{
    __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf));
}

void cpu_features_init(void)
{
    unsigned int maximum_leaf;
    unsigned int extended_maximum_leaf;
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    detected_features.cpuid = 0U;
    detected_features.pae = 0U;
    detected_features.nx = 0U;
    detected_features.tsc = 0U;
    detected_features.rdrand = 0U;
    if (!cpuid_supported()) {
        return;
    }
    read_cpuid(0U, &maximum_leaf, &ebx, &ecx, &edx);
    detected_features.cpuid = 1U;
    if (maximum_leaf >= 1U) {
        read_cpuid(1U, &eax, &ebx, &ecx, &edx);
        detected_features.pae = (edx & (1U << 6U)) != 0U;
        detected_features.tsc = (edx & (1U << 4U)) != 0U;
        detected_features.rdrand = (ecx & (1U << 30U)) != 0U;
    }
    read_cpuid(0x80000000U, &extended_maximum_leaf, &ebx, &ecx, &edx);
    if (extended_maximum_leaf >= 0x80000001U) {
        read_cpuid(0x80000001U, &eax, &ebx, &ecx, &edx);
        detected_features.nx = (edx & (1U << 20U)) != 0U;
    }
}

const struct cpu_features *cpu_features_get(void)
{
    return &detected_features;
}

int cpu_random_u32(unsigned int *value)
{
    unsigned int result;
    unsigned char success;
    unsigned int attempt;

    if (value == 0 || detected_features.rdrand == 0U) {
        return 0;
    }
    for (attempt = 0U; attempt < 10U; attempt++) {
        __asm__ volatile("rdrand %0; setc %1"
            : "=r"(result), "=qm"(success) : : "cc");
        if (success != 0U) {
            *value = result;
            return 1;
        }
    }
    return 0;
}
