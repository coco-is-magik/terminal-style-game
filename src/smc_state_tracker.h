/* smc_state_tracker.h — SMC v2 dirty-state tracking adapter for renderer optimization
 *
 * This header provides an isolated interface for the renderer to use SMC's
 * dirty-state tracking API. It creates an explicit context (not global) for
 * benchmark isolation and deterministic cleanup.
 *
 * When USE_SMC_STATE_TRACKER is defined at build time, the functions interface
 * with the SMC state tracker. When not defined, they are no-ops.
 */

#ifndef SMC_STATE_TRACKER_H
#define SMC_STATE_TRACKER_H

#include "grid.h"       /* Cell struct for state composition */
#include <stdint.h>
#include <stddef.h>

#if defined(USE_SMC_STATE_TRACKER) || defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc.h"        /* SMC types and stats (must come before forward decl check) */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* smc_state_stats_t is provided by smc.h when any SMC state mode is defined,
 * otherwise we declare it locally for the no-op path. */
#if !defined(USE_SMC_STATE_TRACKER) && !defined(USE_SMC_INDEXED_STATE_TRACKER) && !defined(USE_SMC_BATCH_STATE_TRACKER) && !defined(USE_SMC_STREAM_STATE_TRACKER)
typedef struct {
    uint64_t checks;
    uint64_t changed;
    uint64_t unchanged;
    uint64_t stores;
    uint64_t evictions;
    uint64_t bytes_compared;
} smc_state_stats_t;
#endif

/* Cell state used for change detection. Includes only fields that affect
 * raster output (no alpha - COLOR_TO_UINT32 ignores it for opaque pixels).
 *
 * Packed into a uint64_t so the SMC batch path can use the 8-byte fixed-size
 * kernel.  The bit layout is stable and explicit; no padding is exposed. */
typedef uint64_t CellState;

static inline CellState pack_cell_state(const Cell *cell) {
    CellState state = 0;
    state |= ((CellState)cell->glyph)      << 0;
    state |= ((CellState)cell->fg.r)       << 8;
    state |= ((CellState)cell->fg.g)       << 16;
    state |= ((CellState)cell->fg.b)       << 24;
    state |= ((CellState)cell->bg.r)       << 32;
    state |= ((CellState)cell->bg.g)       << 40;
    state |= ((CellState)cell->bg.b)       << 48;
    return state;
}

/* Initialize the SMC state tracker. Must be called before any other functions.
 * max_cells is used to size the hash table (we use 65536 as minimum).
 * Returns 0 on success, non-zero on failure. */
int smc_state_tracker_init(size_t max_cells);

/* Shutdown the SMC state tracker and free all resources. */
void smc_state_tracker_shutdown(void);

/* Reset state and stats. Useful for benchmark runs. */
int smc_state_tracker_reset(void);

/* Check if a cell's state has changed.
 * Returns SMC_OK on success, sets *out_changed to 0 or 1.
 * Falls back to conservative rerasterization on error. */
int smc_state_tracker_cell_changed(uint32_t cell_index,
                                    const CellState *state,
                                    int *out_changed);

/* Get statistics from the state tracker.
 * Returns SMC_OK on success, fills out with current stats. */
int smc_state_tracker_get_stats(smc_state_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SMC_STATE_TRACKER_H */