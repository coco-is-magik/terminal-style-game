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
 * kernel.  The bit layout is stable and explicit; no padding is exposed.
 *
 * Layout (V1_5_CELL_IDENTITY_DECISION_2026-11-22.md, D2): 16-bit codepoint at 0,
 * fg RGB at 16/24/32, bg RGB at 40/48/56 -- 64 bits exactly, no spare bits. The
 * glyph widened from 8 to 16 bits, so every colour field moved; a fingerprint
 * that still packed the old offsets would compare colours as glyphs and let
 * stale pixels through, which is why this layout is asserted by tests. */
/* Byte sizes of the fields the stream path compares, derived from the types rather than written
   as literals. The bug this prevents is specific and was real: when Cell.glyph widened from 8 to
   16 bits, a hardcoded `1` here kept comparing only the low byte, so a codepoint change that
   altered only the high byte was judged unchanged and the cell kept stale pixels — with no error
   anywhere and every existing test still green. Constants derived from the type cannot drift.
 *
 * SMC requires the sum of the stream field sizes to equal the configured state size, so both are
 * computed from these. */
#define CELL_STREAM_GLYPH_BYTES ((size_t)sizeof(((Cell *)0)->glyph))
#define CELL_STREAM_COLOR_BYTES 3U /* fg RGB, bg RGB; alpha never affects the raster */
#define CELL_STREAM_STATE_BYTES (CELL_STREAM_GLYPH_BYTES + CELL_STREAM_COLOR_BYTES + \
                                 CELL_STREAM_COLOR_BYTES)

#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || \
    defined(USE_SMC_STREAM_STATE_TRACKER)
/* The three stream descriptors the renderer feeds SMC, defined once so the renderer and the test
   that guards it cannot disagree about the field sizes. */
static inline void cell_state_streams(const Cell *cells, smc_state_stream_t out[3]) {
    out[0].data = &cells[0].glyph;
    out[0].stride = sizeof(Cell);
    out[0].field_size = CELL_STREAM_GLYPH_BYTES;
    out[1].data = &cells[0].fg.r;
    out[1].stride = sizeof(Cell);
    out[1].field_size = CELL_STREAM_COLOR_BYTES;
    out[2].data = &cells[0].bg.r;
    out[2].stride = sizeof(Cell);
    out[2].field_size = CELL_STREAM_COLOR_BYTES;
}
#endif

typedef uint64_t CellState;

static inline CellState pack_cell_state(const Cell *cell) {
    CellState state = 0;
    state |= ((CellState)(cell->glyph & 0xFFFFU)) << 0;
    state |= ((CellState)cell->fg.r)       << 16;
    state |= ((CellState)cell->fg.g)       << 24;
    state |= ((CellState)cell->fg.b)       << 32;
    state |= ((CellState)cell->bg.r)       << 40;
    state |= ((CellState)cell->bg.g)       << 48;
    state |= ((CellState)cell->bg.b)       << 56;
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
