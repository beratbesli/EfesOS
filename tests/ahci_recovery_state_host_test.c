#include "ahci_recovery_state.h"

static int test_port_recovery(void)
{
    struct ahci_recovery_state state;

    ahci_recovery_state_reset(&state);
    return ahci_recovery_begin(&state) == AHCI_RECOVERY_ACTION_PORT_RESET &&
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 1, 1) ==
            AHCI_RECOVERY_ACTION_COMPLETE &&
        ahci_recovery_port_attempts(&state) == 1U &&
        ahci_recovery_hba_attempts(&state) == 0U &&
        ahci_recovery_completions(&state) == 1U &&
        !ahci_recovery_is_failed(&state);
}

static int test_hba_escalation(void)
{
    struct ahci_recovery_state state;

    ahci_recovery_state_reset(&state);
    if (ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_PORT_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 0, 0) !=
            AHCI_RECOVERY_ACTION_HBA_RESET ||
        ahci_recovery_port_attempts(&state) != 1U ||
        ahci_recovery_hba_attempts(&state) != 1U) {
        return 0;
    }
    return ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_HBA_RESET, 1, 1) ==
            AHCI_RECOVERY_ACTION_COMPLETE &&
        ahci_recovery_completions(&state) == 1U &&
        !ahci_recovery_is_failed(&state);
}

static int test_bounded_failure(void)
{
    struct ahci_recovery_state state;

    ahci_recovery_state_reset(&state);
    if (ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_PORT_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 1, 0) !=
            AHCI_RECOVERY_ACTION_HBA_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_HBA_RESET, 0, 0) !=
            AHCI_RECOVERY_ACTION_FAIL_CLOSED ||
        !ahci_recovery_is_failed(&state)) {
        return 0;
    }
    return ahci_recovery_begin(&state) == AHCI_RECOVERY_ACTION_FAIL_CLOSED &&
        ahci_recovery_port_attempts(&state) == 1U &&
        ahci_recovery_hba_attempts(&state) == 1U &&
        ahci_recovery_completions(&state) == 0U;
}

static int test_invalid_transitions_fail_closed(void)
{
    struct ahci_recovery_state state;

    ahci_recovery_state_reset(&state);
    if (ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 1, 1) !=
            AHCI_RECOVERY_ACTION_FAIL_CLOSED ||
        !ahci_recovery_is_failed(&state)) {
        return 0;
    }
    ahci_recovery_state_reset(&state);
    if (ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_PORT_RESET ||
        ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_FAIL_CLOSED) {
        return 0;
    }
    ahci_recovery_state_reset(&state);
    if (ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_PORT_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 0, 1) !=
            AHCI_RECOVERY_ACTION_FAIL_CLOSED) {
        return 0;
    }
    return ahci_recovery_begin(0) == AHCI_RECOVERY_ACTION_FAIL_CLOSED &&
        ahci_recovery_advance(0, AHCI_RECOVERY_ACTION_PORT_RESET, 0, 0) ==
            AHCI_RECOVERY_ACTION_FAIL_CLOSED &&
        ahci_recovery_is_failed(0) &&
        ahci_recovery_port_attempts(0) == 0U &&
        ahci_recovery_hba_attempts(0) == 0U &&
        ahci_recovery_completions(0) == 0U;
}

static int test_counters_saturate(void)
{
    struct ahci_recovery_state state;

    ahci_recovery_state_reset(&state);
    state.port_attempts = ~0U;
    state.hba_attempts = ~0U;
    state.completions = ~0U;
    if (ahci_recovery_begin(&state) != AHCI_RECOVERY_ACTION_PORT_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_PORT_RESET, 0, 0) !=
            AHCI_RECOVERY_ACTION_HBA_RESET ||
        ahci_recovery_advance(&state, AHCI_RECOVERY_ACTION_HBA_RESET, 1, 1) !=
            AHCI_RECOVERY_ACTION_COMPLETE) {
        return 0;
    }
    return state.port_attempts == ~0U && state.hba_attempts == ~0U &&
        state.completions == ~0U;
}

int main(void)
{
    return test_port_recovery() && test_hba_escalation() &&
        test_bounded_failure() && test_invalid_transitions_fail_closed() &&
        test_counters_saturate() ? 0 : 1;
}
