#ifndef EFESOS_ATA_IRQ_STATE_H
#define EFESOS_ATA_IRQ_STATE_H

#define ATA_IRQ_OBSERVE_ERROR (-1)
#define ATA_IRQ_OBSERVE_NONE 0
#define ATA_IRQ_OBSERVE_READY 1
#define ATA_IRQ_OBSERVE_PENDING 2

struct ata_irq_state {
    volatile unsigned int sequence;
    volatile unsigned int status;
    unsigned int fallback_count;
    volatile int enabled;
};

void ata_irq_state_reset(struct ata_irq_state *state);
void ata_irq_state_enable(struct ata_irq_state *state);
void ata_irq_state_disable(struct ata_irq_state *state);
void ata_irq_state_record(struct ata_irq_state *state, unsigned char status);
unsigned int ata_irq_state_snapshot(const struct ata_irq_state *state);
unsigned int ata_irq_state_count(const struct ata_irq_state *state);
unsigned int ata_irq_state_status(const struct ata_irq_state *state);
unsigned int ata_irq_state_fallback_count(const struct ata_irq_state *state);
int ata_irq_state_is_enabled(const struct ata_irq_state *state);
void ata_irq_state_record_fallback(struct ata_irq_state *state);
int ata_irq_state_observe(const struct ata_irq_state *state,
    unsigned int snapshot, unsigned char required,
    unsigned char forbidden, unsigned char busy_mask);

#endif
