#ifndef EFESOS_AHCI_RECOVERY_STATE_H
#define EFESOS_AHCI_RECOVERY_STATE_H

enum ahci_recovery_action {
    AHCI_RECOVERY_ACTION_NONE = 0,
    AHCI_RECOVERY_ACTION_PORT_RESET,
    AHCI_RECOVERY_ACTION_HBA_RESET,
    AHCI_RECOVERY_ACTION_COMPLETE,
    AHCI_RECOVERY_ACTION_FAIL_CLOSED
};

struct ahci_recovery_state {
    unsigned int action;
    unsigned int port_attempts;
    unsigned int hba_attempts;
    unsigned int completions;
};

void ahci_recovery_state_reset(struct ahci_recovery_state *state);
enum ahci_recovery_action ahci_recovery_begin(
    struct ahci_recovery_state *state);
enum ahci_recovery_action ahci_recovery_advance(
    struct ahci_recovery_state *state, enum ahci_recovery_action completed_action,
    int reset_succeeded, int retry_succeeded);
int ahci_recovery_is_failed(const struct ahci_recovery_state *state);
unsigned int ahci_recovery_port_attempts(
    const struct ahci_recovery_state *state);
unsigned int ahci_recovery_hba_attempts(
    const struct ahci_recovery_state *state);
unsigned int ahci_recovery_completions(
    const struct ahci_recovery_state *state);

#endif
