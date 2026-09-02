#include "ata_irq_state.h"

void ata_irq_state_reset(struct ata_irq_state *state)
{
    if (state == 0) {
        return;
    }
    state->sequence = 0U;
    state->status = 0U;
    state->fallback_count = 0U;
    state->enabled = 0;
}

void ata_irq_state_enable(struct ata_irq_state *state)
{
    if (state != 0) {
        state->enabled = 1;
    }
}

void ata_irq_state_disable(struct ata_irq_state *state)
{
    if (state != 0) {
        state->enabled = 0;
    }
}

void ata_irq_state_record(struct ata_irq_state *state, unsigned char status)
{
    if (state == 0 || !state->enabled) {
        return;
    }
    state->status = status;
    state->sequence++;
}

unsigned int ata_irq_state_snapshot(const struct ata_irq_state *state)
{
    return state == 0 ? 0U : state->sequence;
}

unsigned int ata_irq_state_count(const struct ata_irq_state *state)
{
    return ata_irq_state_snapshot(state);
}

unsigned int ata_irq_state_status(const struct ata_irq_state *state)
{
    return state == 0 ? 0U : state->status;
}

unsigned int ata_irq_state_fallback_count(const struct ata_irq_state *state)
{
    return state == 0 ? 0U : state->fallback_count;
}

int ata_irq_state_is_enabled(const struct ata_irq_state *state)
{
    return state != 0 && state->enabled;
}

void ata_irq_state_record_fallback(struct ata_irq_state *state)
{
    if (state != 0) {
        state->fallback_count++;
    }
}

int ata_irq_state_observe(const struct ata_irq_state *state,
    unsigned int snapshot, unsigned char required,
    unsigned char forbidden, unsigned char busy_mask)
{
    unsigned char status;

    if (state == 0 || !state->enabled || state->sequence == snapshot) {
        return ATA_IRQ_OBSERVE_NONE;
    }
    status = (unsigned char)state->status;
    if ((status & forbidden) != 0U) {
        return ATA_IRQ_OBSERVE_ERROR;
    }
    if ((status & busy_mask) != 0U || (status & required) != required) {
        return ATA_IRQ_OBSERVE_PENDING;
    }
    return ATA_IRQ_OBSERVE_READY;
}
