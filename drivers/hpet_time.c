#include "hpet_time.h"

#define FEMTOSECONDS_PER_NANOSECOND 1000000U
#define HPET_MAX_PERIOD_FEMTOSECONDS 100000000U
#define HPET_U64_MAX (~(hpet_time_u64_t)0U)

static hpet_time_u64_t divide_u64_u32(hpet_time_u64_t value,
    unsigned int divisor, unsigned int *remainder)
{
    hpet_time_u64_t quotient = 0U;
    unsigned int rest = 0U;
    unsigned int bit;

    for (bit = 64U; bit > 0U; bit--) {
        rest = (rest << 1U) |
            (unsigned int)((value >> (bit - 1U)) & 1U);
        if (rest >= divisor) {
            rest -= divisor;
            quotient |= (hpet_time_u64_t)1U << (bit - 1U);
        }
    }
    if (remainder != 0) {
        *remainder = rest;
    }
    return quotient;
}

static hpet_time_u64_t saturating_multiply_u32(hpet_time_u64_t value,
    unsigned int factor)
{
    hpet_time_u64_t result = 0U;

    while (factor != 0U) {
        if ((factor & 1U) != 0U) {
            if (result > HPET_U64_MAX - value) {
                return HPET_U64_MAX;
            }
            result += value;
        }
        factor >>= 1U;
        if (factor != 0U) {
            if (value > HPET_U64_MAX - value) {
                return HPET_U64_MAX;
            }
            value += value;
        }
    }
    return result;
}

hpet_time_u64_t hpet_ticks_to_nanoseconds(hpet_time_u64_t ticks,
    unsigned int period_femtoseconds)
{
    hpet_time_u64_t whole_ticks;
    hpet_time_u64_t whole_nanoseconds;
    hpet_time_u64_t fractional_femtoseconds;
    hpet_time_u64_t fractional_nanoseconds;
    unsigned int remainder;

    if (period_femtoseconds == 0U ||
        period_femtoseconds > HPET_MAX_PERIOD_FEMTOSECONDS) {
        return HPET_U64_MAX;
    }
    whole_ticks = divide_u64_u32(ticks, FEMTOSECONDS_PER_NANOSECOND,
        &remainder);
    whole_nanoseconds = saturating_multiply_u32(whole_ticks,
        period_femtoseconds);
    fractional_femtoseconds = saturating_multiply_u32(remainder,
        period_femtoseconds);
    fractional_nanoseconds = divide_u64_u32(fractional_femtoseconds,
        FEMTOSECONDS_PER_NANOSECOND, 0);
    if (whole_nanoseconds > HPET_U64_MAX - fractional_nanoseconds) {
        return HPET_U64_MAX;
    }
    return whole_nanoseconds + fractional_nanoseconds;
}

hpet_time_u64_t hpet_extend_counter32(unsigned int low,
    unsigned int *last_low, unsigned int *wrap_count)
{
    if (last_low == 0 || wrap_count == 0) {
        return 0U;
    }
    if (low < *last_low) {
        (*wrap_count)++;
    }
    *last_low = low;
    return ((hpet_time_u64_t)*wrap_count << 32U) | low;
}
