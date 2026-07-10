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

#ifdef USE_SMC_STATE_TRACKER
#include "smc.h"        /* SMC types and stats (must come before forward decl check) */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* smc_state_stats_t is provided by smc.h when USE_SMC_STATE_TRACKER is defined,
 * otherwise we declare it locally for the no-op path. */
#ifndef USE_SMC_STATE_TRACKER
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
 * raster output (no alpha - COLOR_TO_UINT32 ignores it for opaque pixels). */
typedef struct {
    uint8_t glyph;
    uint8_t fg_r, fg_g, fg_b;
    uint8_t bg_r, bg_g, bg_b;
} CellState;

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