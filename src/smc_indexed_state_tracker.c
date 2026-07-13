/* smc_indexed_state_tracker.c — SMC v2.1 indexed state tracker adapter */

#include "smc_indexed_state_tracker.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER)
#include "smc.h"
static smc_context_t *g_smc_indexed_ctx = NULL;

int smc_indexed_state_tracker_init(size_t cell_count) {
    if (g_smc_indexed_ctx) return 0;
    int rc = smc_init();
    if (rc != SMC_OK) return rc;
    g_smc_indexed_ctx = smc_context_create(1);
    if (!g_smc_indexed_ctx) { smc_shutdown(); return SMC_ERR_INIT; }
    smc_state_indexed_config_t cfg = { .count = cell_count, .state_size = sizeof(CellState), .memory_budget_bytes = 0 }; /* 8-byte packed state */
    rc = smc_state_indexed_configure(g_smc_indexed_ctx, &cfg);
    if (rc != SMC_OK) { smc_context_destroy(g_smc_indexed_ctx); g_smc_indexed_ctx = NULL; smc_shutdown(); return rc; }
    return SMC_OK;
}

void smc_indexed_state_tracker_shutdown(void) {
    if (g_smc_indexed_ctx) { smc_context_destroy(g_smc_indexed_ctx); g_smc_indexed_ctx = NULL; }
    smc_shutdown();
}

int smc_indexed_state_tracker_reset(void) {
    if (!g_smc_indexed_ctx) return SMC_ERR_INIT;
    smc_state_indexed_clear(g_smc_indexed_ctx);
    return smc_state_indexed_reset_stats(g_smc_indexed_ctx);
}

int smc_indexed_state_tracker_cell_changed(uint32_t cell_index, const CellState *state, int *out_changed) {
    if (!g_smc_indexed_ctx || !state || !out_changed) return SMC_ERR_INVALID;
    return smc_state_changed_index(g_smc_indexed_ctx, cell_index, state, sizeof(CellState), out_changed);
}

int smc_indexed_state_tracker_diff_batch(const CellState *states, size_t count, uint32_t *dirty_indices, size_t dirty_capacity, size_t *out_dirty_count) {
    if (!g_smc_indexed_ctx || !states || !out_dirty_count) return SMC_ERR_INVALID;
    return smc_state_diff_indexed_batch(g_smc_indexed_ctx, states, count, sizeof(CellState), dirty_indices, dirty_capacity, out_dirty_count);
}

int smc_indexed_state_tracker_get_stats(IndexedStateStats *out) {
    if (!g_smc_indexed_ctx || !out) return SMC_ERR_INVALID;
    smc_state_indexed_stats_t smc_stats;
    int rc = smc_state_indexed_get_stats(g_smc_indexed_ctx, &smc_stats);
    if (rc != SMC_OK) return rc;
    out->checks = smc_stats.checks; out->changed = smc_stats.changed; out->unchanged = smc_stats.unchanged;
    out->stores = smc_stats.stores; out->bytes_compared = smc_stats.bytes_compared;
    out->out_of_range = smc_stats.out_of_range; out->clears = smc_stats.clears;
    return SMC_OK;
}

#else

int smc_indexed_state_tracker_init(size_t cell_count) { (void)cell_count; return 0; }
void smc_indexed_state_tracker_shutdown(void) {}
int smc_indexed_state_tracker_reset(void) { return 0; }
int smc_indexed_state_tracker_cell_changed(uint32_t cell_index, const CellState *state, int *out_changed) {
    (void)cell_index; (void)state; if (out_changed) *out_changed = 1; return 0; }
int smc_indexed_state_tracker_diff_batch(const CellState *states, size_t count, uint32_t *dirty_indices, size_t dirty_capacity, size_t *out_dirty_count) {
    (void)states; (void)dirty_capacity; (void)dirty_indices; if (out_dirty_count) *out_dirty_count = count; return 0; }
int smc_indexed_state_tracker_get_stats(IndexedStateStats *out) {
    if (out) { out->checks=0; out->changed=0; out->unchanged=0; out->stores=0; out->bytes_compared=0; out->out_of_range=0; out->clears=0; }
    return 0;
}
#endif

void smc_indexed_state_tracker_get_buffer_sizes(size_t cell_count, size_t *state_buffer_bytes, size_t *dirty_indices_bytes) {
    (void)cell_count;
    if (state_buffer_bytes) *state_buffer_bytes = cell_count * sizeof(CellState);
    if (dirty_indices_bytes) *dirty_indices_bytes = cell_count * sizeof(uint32_t);
}
