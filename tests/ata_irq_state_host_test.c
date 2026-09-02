#include <stdio.h>

#include "ata_irq_state.h"

#define STATUS_ERR 0x01U
#define STATUS_DRQ 0x08U
#define STATUS_DF 0x20U
#define STATUS_BSY 0x80U

int main(void)
{
    struct ata_irq_state state;
    unsigned int snapshot;

    ata_irq_state_reset(&state);
    snapshot = ata_irq_state_snapshot(&state);
    ata_irq_state_record(&state, STATUS_DRQ);
    if (ata_irq_state_count(&state) != 0U ||
        ata_irq_state_observe(&state, snapshot, STATUS_DRQ,
            STATUS_ERR | STATUS_DF, STATUS_BSY) != ATA_IRQ_OBSERVE_NONE) {
        return 1;
    }

    ata_irq_state_enable(&state);
    ata_irq_state_record(&state, STATUS_DRQ);
    if (!ata_irq_state_is_enabled(&state) || ata_irq_state_count(&state) != 1U ||
        ata_irq_state_status(&state) != STATUS_DRQ ||
        ata_irq_state_observe(&state, snapshot, STATUS_DRQ,
            STATUS_ERR | STATUS_DF, STATUS_BSY) != ATA_IRQ_OBSERVE_READY) {
        return 1;
    }

    snapshot = ata_irq_state_snapshot(&state);
    ata_irq_state_record(&state, STATUS_BSY);
    if (ata_irq_state_observe(&state, snapshot, STATUS_DRQ,
        STATUS_ERR | STATUS_DF, STATUS_BSY) != ATA_IRQ_OBSERVE_PENDING) {
        return 1;
    }
    snapshot = ata_irq_state_snapshot(&state);
    ata_irq_state_record(&state, STATUS_ERR);
    if (ata_irq_state_observe(&state, snapshot, 0U,
        STATUS_ERR | STATUS_DF, STATUS_BSY) != ATA_IRQ_OBSERVE_ERROR) {
        return 1;
    }

    ata_irq_state_record_fallback(&state);
    if (ata_irq_state_fallback_count(&state) != 1U) {
        return 1;
    }
    ata_irq_state_disable(&state);
    snapshot = ata_irq_state_snapshot(&state);
    ata_irq_state_record(&state, STATUS_DRQ);
    if (ata_irq_state_is_enabled(&state) ||
        ata_irq_state_snapshot(&state) != snapshot) {
        return 1;
    }

    /* Sequence wrap remains safe because completion uses inequality. */
    state.sequence = 0xFFFFFFFFU;
    ata_irq_state_enable(&state);
    snapshot = ata_irq_state_snapshot(&state);
    ata_irq_state_record(&state, STATUS_DRQ);
    if (ata_irq_state_count(&state) != 0U ||
        ata_irq_state_observe(&state, snapshot, STATUS_DRQ,
            STATUS_ERR | STATUS_DF, STATUS_BSY) != ATA_IRQ_OBSERVE_READY) {
        return 1;
    }

    ata_irq_state_reset(0);
    ata_irq_state_enable(0);
    ata_irq_state_disable(0);
    ata_irq_state_record(0, 0U);
    ata_irq_state_record_fallback(0);
    if (ata_irq_state_is_enabled(0) || ata_irq_state_count(0) != 0U ||
        ata_irq_state_status(0) != 0U || ata_irq_state_fallback_count(0) != 0U ||
        ata_irq_state_observe(0, 0U, 0U, 0U, 0U) != ATA_IRQ_OBSERVE_NONE) {
        return 1;
    }

    puts("ATA IRQ state host self-test passed.");
    return 0;
}
