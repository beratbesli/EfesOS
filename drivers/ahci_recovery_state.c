#include "ahci_recovery_state.h"

static enum ahci_recovery_action fail_closed(
    struct ahci_recovery_state *state)
{
    if (state != 0) {
        state->action = AHCI_RECOVERY_ACTION_FAIL_CLOSED;
    }
    return AHCI_RECOVERY_ACTION_FAIL_CLOSED;
}

static void increment_saturating(unsigned int *value)
{
    if (*value != ~0U) {
        (*value)++;
    }
}

void ahci_recovery_state_reset(struct ahci_recovery_state *state)
{
    if (state == 0) {
        return;
    }
    state->action = AHCI_RECOVERY_ACTION_NONE;
    state->port_attempts = 0U;
    state->hba_attempts = 0U;
    state->completions = 0U;
}

enum ahci_recovery_action ahci_recovery_begin(
    struct ahci_recovery_state *state)
{
    if (state == 0 || state->action != AHCI_RECOVERY_ACTION_NONE) {
        return fail_closed(state);
    }
    state->action = AHCI_RECOVERY_ACTION_PORT_RESET;
    increment_saturating(&state->port_attempts);
    return AHCI_RECOVERY_ACTION_PORT_RESET;
}

enum ahci_recovery_action ahci_recovery_advance(
    struct ahci_recovery_state *state, enum ahci_recovery_action completed_action,
    int reset_succeeded, int retry_succeeded)
{
    if (state == 0 ||
        (completed_action != AHCI_RECOVERY_ACTION_PORT_RESET &&
         completed_action != AHCI_RECOVERY_ACTION_HBA_RESET) ||
        state->action != (unsigned int)completed_action ||
        (!reset_succeeded && retry_succeeded)) {
        return fail_closed(state);
    }

    if (reset_succeeded && retry_succeeded) {
        increment_saturating(&state->completions);
        state->action = AHCI_RECOVERY_ACTION_NONE;
        return AHCI_RECOVERY_ACTION_COMPLETE;
    }
    if (completed_action == AHCI_RECOVERY_ACTION_PORT_RESET) {
        state->action = AHCI_RECOVERY_ACTION_HBA_RESET;
        increment_saturating(&state->hba_attempts);
        return AHCI_RECOVERY_ACTION_HBA_RESET;
    }
    return fail_closed(state);
}

int ahci_recovery_is_failed(const struct ahci_recovery_state *state)
{
    return state == 0 || state->action == AHCI_RECOVERY_ACTION_FAIL_CLOSED;
}

unsigned int ahci_recovery_port_attempts(
    const struct ahci_recovery_state *state)
{
    return state != 0 ? state->port_attempts : 0U;
}

unsigned int ahci_recovery_hba_attempts(
    const struct ahci_recovery_state *state)
{
    return state != 0 ? state->hba_attempts : 0U;
}

unsigned int ahci_recovery_completions(
    const struct ahci_recovery_state *state)
{
    return state != 0 ? state->completions : 0U;
}
