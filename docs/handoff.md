# SMC Renderer Integration — Handoff Document

## Overview

This document tracks the SMC (Self-Modifying Calculator) integration into the
terminal-style-game raycasting renderer. The goal is to use SMC as an adaptive
computation system to reduce renderer frame cost by caching reusable renderer
artifacts — not by replacing individual scalar math calls.

## Repository

- **Target project**: https://github.com/coco-is-magik/terminal-style-game
- **SMC project**: https://github.com/coco-is-magik/self-modifying-calculator

---

## Session 1 — Scalar Trig Replacement (DID NOT WORK)

Replaced 4 scalar trigonometric expressions in `raycast.c` with SMC dispatch.

**Result**: No speedup (8.58 ms vs 8.52 ms) - renderer is SDL-bound, not math-bound.

---

## Session 2 — Lighting Shadow Ray Cache (STOPPED)

Implemented stationary light-to-tile shadow ray cache in `src/lighting_cache.h/c`.

**Result**: Cache works (100% hit rate) but lighting is only 0.01 ms/frame - stop condition met.

---

## Session 3 — Glyph Block Cache (REGRESSION - DIAGNOSED)

Implemented glyph block cache in `src/glyph_block_cache.h/c`.

### Microbenchmark Results (pure C, no SDL):
| Operation | Time | ns/cell |
|-----------|------|---------|
| Original rasterization | 18.7 ms | 44.35 |
| Cache hit (memcpy) | 3.6 ms | 8.69 |
| Cache hit (loop) | 4.3 ms | 10.36 |

Cache hit path IS faster (5x speedup potential).

### Real Benchmark Results (with SDL):
| Mode | Configuration | avg_render_ms | raster_ms |
|------|---------------|-------------|-----------|
| raycast | baseline | 8.97 | ~5.8 |
| raycast | glyph cache (1st version) | 14.8 | 9.8 |
| raycast | glyph cache (unrolled hit) | 10.33 | 6.26 |

**Diagnosis**:
1. Cache miss path adds ~8 ns/cell overhead (copy back to cache)
2. Cache hit path adds ~2-3 ns overhead (lookup + key construction)
3. First frame pays full cost + cache population
4. Even 100% hit rate: 41,600 cache operations/frame

**The glyph cache is faster per-cell but has overhead that negates benefits.**

---

## Session 4 — Dirty-Cell Tracking (SUCCESS - 40% SPEEDUP)

### Benchmark Results
| Mode | Configuration | avg_render_ms |
|------|---------------|-------------|
| raycast | baseline | 8.83 |
| raycast | USE_DIRTY_CELLS=1 | 5.27 |

**Result: 40% speedup** achieved by skipping rasterization of unchanged cells.

### Why this works
- First frame: All cells are "dirty" (prev_cells is initialized to zeros)
- Subsequent frames: Only the HUD overlay and moving entities actually change
- The raycast world itself is static with fixed camera position
- Dirty-cell check adds ~3 ns/cell overhead, but saves ~44 ns/cell for skipped cells

### Implementation Notes
- Uses `prev_cells` array in Grid struct (swapped each frame via `grid_swap_prev`)
- Comparison is cheap: glyph (uint8_t) + RGB colors only (ignoring alpha)
- No per-cell allocations or tracking bits needed

---

## Conclusion

The SMC scalar dispatch replacement showed no measurable speedup because the renderer hot path is memory-bound. However, dirty-cell tracking (similar architectural pattern) achieved **~40% speedup** by eliminating redundant work rather than caching it.
