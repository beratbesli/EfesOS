#include "ahci_irq_state.h"

static int test_lifecycle(void)
{
    struct ahci_irq_state state;
    uint32_t generation;

    ahci_irq_state_reset(&state);
    if (ahci_irq_state_is_enabled(&state) ||
        ahci_irq_state_begin(&state) != 0U) {
        return 0;
    }
    ahci_irq_state_enable(&state);
    generation = ahci_irq_state_begin(&state);
    if (generation != 1U || ahci_irq_state_begin(&state) != 0U ||
        ahci_irq_state_observe(&state, generation, 1U, 0x100U) !=
            AHCI_IRQ_OBSERVE_NONE) {
        return 0;
    }
    ahci_irq_state_record(&state, 2U);
    if (ahci_irq_state_interrupt_count(&state) != 1U ||
        ahci_irq_state_status(&state) != 2U ||
        ahci_irq_state_observe(&state, generation, 1U, 0x100U) !=
            AHCI_IRQ_OBSERVE_NONE) {
        return 0;
    }
    ahci_irq_state_record(&state, 1U);
    if (ahci_irq_state_observe(&state, generation, 1U, 0x100U) !=
            AHCI_IRQ_OBSERVE_COMPLETE ||
        ahci_irq_state_finish(&state, generation + 1U) ||
        !ahci_irq_state_finish(&state, generation)) {
        return 0;
    }
    ahci_irq_state_record(&state, 1U);
    return ahci_irq_state_interrupt_count(&state) == 3U &&
        ahci_irq_state_status(&state) == 0U &&
        ahci_irq_state_observe(&state, generation, 1U, 0x100U) ==
            AHCI_IRQ_OBSERVE_NONE;
}

static int test_error_precedes_completion(void)
{
    struct ahci_irq_state state;
    uint32_t generation;

    ahci_irq_state_reset(&state);
    ahci_irq_state_enable(&state);
    generation = ahci_irq_state_begin(&state);
    ahci_irq_state_record(&state, 0x101U);
    return ahci_irq_state_observe(&state, generation, 1U, 0x100U) ==
            AHCI_IRQ_OBSERVE_ERROR &&
        ahci_irq_state_observe(&state, generation + 1U, 1U, 0x100U) ==
            AHCI_IRQ_OBSERVE_NONE;
}

static int test_wrap_and_disable(void)
{
    struct ahci_irq_state state;

    ahci_irq_state_reset(&state);
    ahci_irq_state_enable(&state);
    state.generation = 0xFFFFFFFFU;
    if (ahci_irq_state_begin(&state) != 1U) {
        return 0;
    }
    ahci_irq_state_record_fallback(&state);
    ahci_irq_state_record_fallback(&state);
    if (ahci_irq_state_fallback_count(&state) != 2U) {
        return 0;
    }
    ahci_irq_state_disable(&state);
    ahci_irq_state_record(&state, 1U);
    return !ahci_irq_state_is_enabled(&state) && state.active == 0U &&
        ahci_irq_state_interrupt_count(&state) == 0U &&
        ahci_irq_state_begin(&state) == 0U;
}

static int test_null_contract(void)
{
    ahci_irq_state_reset(0);
    ahci_irq_state_enable(0);
    ahci_irq_state_disable(0);
    ahci_irq_state_record(0, 1U);
    ahci_irq_state_record_fallback(0);
    return ahci_irq_state_begin(0) == 0U &&
        ahci_irq_state_observe(0, 1U, 1U, 1U) == AHCI_IRQ_OBSERVE_NONE &&
        !ahci_irq_state_finish(0, 1U) &&
        !ahci_irq_state_is_enabled(0) &&
        ahci_irq_state_interrupt_count(0) == 0U &&
        ahci_irq_state_fallback_count(0) == 0U &&
        ahci_irq_state_status(0) == 0U;
}

int main(void)
{
    return test_lifecycle() && test_error_precedes_completion() &&
        test_wrap_and_disable() && test_null_contract() ? 0 : 1;
}
