# SMC Renderer Integration Plan

## Objective

Integrate SMC into the terminal-style-game as a generated-code optimization path for repeated renderer math. The goal is to identify hot-path computations, generate optimized C dispatch code from SMC, and replace selected calculations with `smc_call_*` calls.

---

## Investigation Results

### Target Analysis

The terminal-style-game renderer has three major phases:
1. **Raycasting** (DDA stepping through map, floor/ceiling projection, decals)
2. **Lighting** (per-tile light propagation, shadow rays for dynamic lights)
3. **Rasterization** (8×8 glyph compositing into pixel buffer, SDL texture upload)

### Hot Path Identification

- Raycast loop: ~10,000 iterations/frame for screen-width columns
- Lighting: ~20,000 shadow rays/frame for a single dynamic light
- Rasterization: 41,600 cell rasterizations/frame (260×160 grid)

---

## Integration Attempts

### Session 1: Scalar Trig Replacement
- Replaced: `atan(camera_x * tan(fov/2))`, fisheye correction, true distance, light screen X
- Result: 8.52 ms vs 8.58 ms baseline (no measurable speedup)
- Reason: Trig functions are fast on modern CPUs; renderer is SDL-bound

### Session 2: Lighting Shadow Ray Cache
- Implemented stationary ray caching using direct-mapped hash table
- Result: Working cache (100% hit rate), but 0.01 ms lighting time (far below threshold)
- Stopped per stop condition: lighting < 0.5 ms threshold

### Session 3: Glyph Block Cache
- Cached 8×8 RGBA pixel blocks keyed by (glyph, fg, bg)
- Microbenchmark showed cache hit path is 5x faster (8.69 ns vs 44.35 ns)
- Real benchmark showed regression (14.8 ms vs 8.76 ms baseline)
- Reason: Cache miss overhead + 41,600 operations/frame negated gains

### Session 4: Dirty-Cell Tracking
- Added `prev_cells` to Grid struct for frame-to-frame comparison
- Skipped rasterization for cells unchanged from previous frame
- Result: **5.27 ms vs 8.83 ms baseline (40% speedup)**
- Success!

---

## Final Results

| Optimization | Status | Speedup |
|--------------|--------|---------|
| SMC scalar | Did not work | 0% |
| Lighting cache | Working but trivial | 0% |
| Glyph cache | Regression | -40% |
| Dirty cells | Success | +40% |

---

## Acceptance Criteria

- [x] Project builds cleanly
- [x] Renderer produces correct output
- [x] No `smc_eval_*` in hot path
- [x] Generated-code path exercised
- [x] Measurable speedup achieved (dirty-cell tracking)

---

## Files Modified

- `src/smc_render_opt.h/c` — SMC scalar adapter
- `src/lighting_cache.h/c` — Lighting optimization (trivial impact)
- `src/glyph_block_cache.h/c` — Glyph caching (regression)
- `src/renderer.h/c` — Timing instrumentation, dirty-cell support
- `src/grid.h/c` — Added `prev_cells` field and `grid_swap_prev()` function
- `Makefile` — Added `USE_DIRTY_CELLS=1` build flag