#include "ahci_irq_state.h"

void ahci_irq_state_reset(struct ahci_irq_state *state)
{
    if (state == 0) {
        return;
    }
    state->generation = 0U;
    state->observed_generation = 0U;
    state->observed_status = 0U;
    state->interrupt_count = 0U;
    state->fallback_count = 0U;
    state->enabled = 0U;
    state->active = 0U;
}

void ahci_irq_state_enable(struct ahci_irq_state *state)
{
    if (state != 0) {
        state->enabled = 1U;
    }
}

void ahci_irq_state_disable(struct ahci_irq_state *state)
{
    if (state == 0) {
        return;
    }
    state->enabled = 0U;
    state->active = 0U;
    state->observed_generation = 0U;
    state->observed_status = 0U;
}

uint32_t ahci_irq_state_begin(struct ahci_irq_state *state)
{
    uint32_t next;

    if (state == 0 || state->enabled == 0U || state->active != 0U) {
        return 0U;
    }
    next = state->generation + 1U;
    if (next == 0U) {
        next = 1U;
    }
    state->generation = next;
    state->observed_generation = 0U;
    state->observed_status = 0U;
    state->active = 1U;
    return next;
}

void ahci_irq_state_record(struct ahci_irq_state *state, uint32_t status)
{
    if (state == 0 || state->enabled == 0U || status == 0U) {
        return;
    }
    state->interrupt_count++;
    if (state->active != 0U) {
        state->observed_status |= status;
        state->observed_generation = state->generation;
    }
}

int ahci_irq_state_observe(const struct ahci_irq_state *state,
    uint32_t generation, uint32_t completion_mask, uint32_t error_mask)
{
    uint32_t status;

    if (state == 0 || generation == 0U || state->enabled == 0U ||
        state->active == 0U || state->generation != generation ||
        state->observed_generation != generation) {
        return AHCI_IRQ_OBSERVE_NONE;
    }
    status = state->observed_status;
    if ((status & error_mask) != 0U) {
        return AHCI_IRQ_OBSERVE_ERROR;
    }
    return (status & completion_mask) != 0U ?
        AHCI_IRQ_OBSERVE_COMPLETE : AHCI_IRQ_OBSERVE_NONE;
}

int ahci_irq_state_finish(struct ahci_irq_state *state, uint32_t generation)
{
    if (state == 0 || generation == 0U || state->active == 0U ||
        state->generation != generation) {
        return 0;
    }
    state->active = 0U;
    state->observed_generation = 0U;
    state->observed_status = 0U;
    return 1;
}

void ahci_irq_state_record_fallback(struct ahci_irq_state *state)
{
    if (state != 0) {
        state->fallback_count++;
    }
}

int ahci_irq_state_is_enabled(const struct ahci_irq_state *state)
{
    return state != 0 && state->enabled != 0U;
}

uint32_t ahci_irq_state_interrupt_count(const struct ahci_irq_state *state)
{
    return state == 0 ? 0U : state->interrupt_count;
}

uint32_t ahci_irq_state_fallback_count(const struct ahci_irq_state *state)
{
    return state == 0 ? 0U : state->fallback_count;
}

uint32_t ahci_irq_state_status(const struct ahci_irq_state *state)
{
    return state == 0 ? 0U : state->observed_status;
}
