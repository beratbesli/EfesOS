#ifndef EFESOS_AHCI_IRQ_STATE_H
#define EFESOS_AHCI_IRQ_STATE_H

#include <stdint.h>

#define AHCI_IRQ_OBSERVE_NONE 0
#define AHCI_IRQ_OBSERVE_COMPLETE 1
#define AHCI_IRQ_OBSERVE_ERROR (-1)

struct ahci_irq_state {
    volatile uint32_t generation;
    volatile uint32_t observed_generation;
    volatile uint32_t observed_status;
    volatile uint32_t interrupt_count;
    volatile uint32_t fallback_count;
    volatile uint8_t enabled;
    volatile uint8_t active;
};

void ahci_irq_state_reset(struct ahci_irq_state *state);
void ahci_irq_state_enable(struct ahci_irq_state *state);
void ahci_irq_state_disable(struct ahci_irq_state *state);
uint32_t ahci_irq_state_begin(struct ahci_irq_state *state);
void ahci_irq_state_record(struct ahci_irq_state *state, uint32_t status);
int ahci_irq_state_observe(const struct ahci_irq_state *state,
    uint32_t generation, uint32_t completion_mask, uint32_t error_mask);
int ahci_irq_state_finish(struct ahci_irq_state *state, uint32_t generation);
void ahci_irq_state_record_fallback(struct ahci_irq_state *state);
int ahci_irq_state_is_enabled(const struct ahci_irq_state *state);
uint32_t ahci_irq_state_interrupt_count(const struct ahci_irq_state *state);
uint32_t ahci_irq_state_fallback_count(const struct ahci_irq_state *state);
uint32_t ahci_irq_state_status(const struct ahci_irq_state *state);

#endif
