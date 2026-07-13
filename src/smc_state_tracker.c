/* smc_state_tracker.c — SMC v2 dirty-state tracking adapter implementation
 *
 * Implements the adapter declared in smc_state_tracker.h. Creates an explicit
 * context for the state tracker to avoid global state and enable deterministic
 * cleanup. Works with the SMC stub runtime (no SBCL dependency).
 */

#include "smc_state_tracker.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_SMC_STATE_TRACKER
#include "smc.h"

/* Internal: owned SMC context for the state tracker */
static smc_context_t *g_smc_state_ctx = NULL;

int smc_state_tracker_init(size_t max_cells) {
    if (g_smc_state_ctx) {
        return 0;  /* Already initialized */
    }
    
    /* Use SMC's default init for the library (initializes global context) */
    int rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_state_tracker_init: smc_init failed: %s\n",
                smc_error_string(rc));
        return rc;
    }
    
    /* Create our own context for isolation */
    g_smc_state_ctx = smc_context_create(1);
    if (!g_smc_state_ctx) {
        smc_shutdown();
        fprintf(stderr, "smc_state_tracker_init: failed to create context\n");
        return SMC_ERR_INIT;
    }
    
    /* Ensure minimum table size to reduce collisions (use larger table for better distribution) */
    /* With 41,600 cells, use 524,288 entries for ~12:1 ratio to minimize collisions */
    size_t entries = max_cells * 12;
    if (entries < 524288) {
        entries = 524288;
    }
    /* Ensure power of 2 for direct-mapped hash */
    size_t temp = 1;
    while (temp < entries) {
        temp <<= 1;
    }
    entries = temp;
    
    smc_state_config_t cfg = {
        .max_entries = entries,
        .max_key_size = sizeof(uint32_t),
        .max_state_size = sizeof(CellState), /* 8-byte packed state */
        .memory_budget_bytes = 0
    };
    
    rc = smc_state_configure(g_smc_state_ctx, &cfg);
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_state_tracker_init: smc_state_configure failed: %s\n",
                smc_error_string(rc));
        smc_context_destroy(g_smc_state_ctx);
        g_smc_state_ctx = NULL;
        return rc;
    }
    
    return SMC_OK;
}

void smc_state_tracker_shutdown(void) {
    if (g_smc_state_ctx) {
        smc_context_destroy(g_smc_state_ctx);
        g_smc_state_ctx = NULL;
    }
    smc_shutdown();
}

int smc_state_tracker_reset(void) {
    if (!g_smc_state_ctx) {
        return SMC_ERR_INIT;
    }
    int rc = smc_state_clear(g_smc_state_ctx);
    if (rc != SMC_OK) {
        return rc;
    }
    return smc_state_reset_stats(g_smc_state_ctx);
}

int smc_state_tracker_cell_changed(uint32_t cell_index,
                                    const CellState *state,
                                    int *out_changed) {
    if (!g_smc_state_ctx || !state || !out_changed) {
        return SMC_ERR_INVALID;
    }
    return smc_state_changed(g_smc_state_ctx,
                              &cell_index, sizeof(cell_index),
                              state, sizeof(CellState),
                              out_changed);
}

int smc_state_tracker_get_stats(smc_state_stats_t *out) {
    if (!g_smc_state_ctx || !out) {
        return SMC_ERR_INVALID;
    }
    return smc_state_get_stats(g_smc_state_ctx, out);
}

#else /* !USE_SMC_STATE_TRACKER */

int smc_state_tracker_init(size_t max_cells) {
    (void)max_cells;
    return 0;
}

void smc_state_tracker_shutdown(void) {
    /* No-op */
}

int smc_state_tracker_reset(void) {
    return 0;
}

int smc_state_tracker_cell_changed(uint32_t cell_index,
                                    const CellState *state,
                                    int *out_changed) {
    (void)cell_index; (void)state;
    if (out_changed) *out_changed = 1;
    return 0;
}

int smc_state_tracker_get_stats(smc_state_stats_t *out) {
    if (out) {
        out->checks = 0;
        out->changed = 0;
        out->unchanged = 0;
        out->stores = 0;
        out->evictions = 0;
        out->bytes_compared = 0;
    }
    return 0;
}

#endif /* USE_SMC_STATE_TRACKER */