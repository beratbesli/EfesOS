#include "hpet_time.h"

#include <stdio.h>

#define U64_MAX_VALUE (~(hpet_time_u64_t)0U)

int main(void)
{
    hpet_time_u64_t previous;
    hpet_time_u64_t extended;
    unsigned int last_low = 0xFFFFFFF0U;
    unsigned int wraps = 2U;
    unsigned int index;

    if (hpet_ticks_to_nanoseconds(0U, 10000000U) != 0U ||
        hpet_ticks_to_nanoseconds(1U, 10000000U) != 10U ||
        hpet_ticks_to_nanoseconds(3U, 100000000U) != 300U ||
        hpet_ticks_to_nanoseconds(1000000U, 10000000U) != 10000000U ||
        hpet_ticks_to_nanoseconds(1234567U, 10000000U) != 12345670U ||
        hpet_ticks_to_nanoseconds(1U, 0U) != U64_MAX_VALUE ||
        hpet_ticks_to_nanoseconds(1U, 100000001U) != U64_MAX_VALUE ||
        hpet_ticks_to_nanoseconds(U64_MAX_VALUE, 100000000U) !=
            U64_MAX_VALUE) {
        return 1;
    }
    extended = hpet_extend_counter32(0xFFFFFFF5U, &last_low, &wraps);
    if (extended != 0x00000002FFFFFFF5ULL || last_low != 0xFFFFFFF5U ||
        wraps != 2U) {
        return 1;
    }
    extended = hpet_extend_counter32(5U, &last_low, &wraps);
    if (extended != 0x0000000300000005ULL || last_low != 5U || wraps != 3U ||
        hpet_extend_counter32(1U, 0, &wraps) != 0U ||
        hpet_extend_counter32(1U, &last_low, 0) != 0U) {
        return 1;
    }
    previous = 0U;
    for (index = 0U; index < 100000U; index++) {
        hpet_time_u64_t converted = hpet_ticks_to_nanoseconds(index,
            69841279U);

        if (converted < previous) {
            return 1;
        }
        previous = converted;
    }
    puts("HPET time conversion host self-test passed.");
    return 0;
}
