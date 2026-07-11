/* smc_indexed_state_tracker.h — SMC v2.1 indexed state tracker adapter for renderer
 *
 * Provides a thin wrapper around SMC's indexed dirty-state APIs.
 * Build flags:
 *   USE_SMC_INDEXED_STATE_TRACKER=1  - enables indexed per-cell API
 *   USE_SMC_BATCH_STATE_TRACKER=1    - enables batch API
 */

#ifndef SMC_INDEXED_STATE_TRACKER_H
#define SMC_INDEXED_STATE_TRACKER_H

#include <stdint.h>
#include <stddef.h>
#include "smc_state_tracker.h" /* CellState and smc_state_stats_t */

/* Stats struct for indexed state tracking */
typedef struct {
    uint64_t checks;
    uint64_t changed;
    uint64_t unchanged;
    uint64_t stores;
    uint64_t bytes_compared;
    uint64_t out_of_range;
    uint64_t clears;
} IndexedStateStats;

int smc_indexed_state_tracker_init(size_t cell_count);
void smc_indexed_state_tracker_shutdown(void);
int smc_indexed_state_tracker_reset(void);
int smc_indexed_state_tracker_cell_changed(uint32_t cell_index, const CellState *state, int *out_changed);
int smc_indexed_state_tracker_diff_batch(const CellState *states, size_t count, uint32_t *dirty_indices, size_t dirty_capacity, size_t *out_dirty_count);
int smc_indexed_state_tracker_get_stats(IndexedStateStats *out);
void smc_indexed_state_tracker_get_buffer_sizes(size_t cell_count, size_t *state_buffer_bytes, size_t *dirty_indices_bytes);

#endif
