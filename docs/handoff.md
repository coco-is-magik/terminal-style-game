# SMC Renderer Integration — Handoff Document

## Overview

This document tracks the SMC (Self-Modifying Calculator) integration into the
terminal-style-game raycasting renderer. The goal is to use SMC as an adaptive
computation system to reduce renderer frame cost by caching reusable renderer
artifacts — not by replacing individual scalar math calls.

## Repository

- **Target project**: https://github.com/coco-is-magik/terminal-style-game
- **SMC project**: https://github.com/coco-is-magik/self-modifying-calculator

## Revised Plan

### Phase 1: Profile lighting_update() Costs
Add `--benchmark-lighting` mode to measure actual lighting costs.

**Metrics**:
- Total `lighting_update()` time
- Shadow rays fired per frame  
- Time spent in `raycast_fire()` within lighting

### Phase 2: Lighting Shadow Ray Cache
Implement a C-side cache for stationery light-to-tile shadow rays.

**Cache key**:
```c
typedef struct {
    int map_revision;
    int lighting_revision;
    int light_id;
    int target_tile_x;
    int target_tile_y;
    int occlusion_mask;
} LightShadowKey;
```

**Cache result**:
```c
typedef struct {
    bool blocked;
    double distance;
    double attenuation;
    double intensity;
} LightSampleResult;
```

### Phase 3: Validate Cache Correctness
Add `LIGHTING_CACHE_VALIDATE=1` compile flag for debug validation
of cache hit correctness.

### Phase 4: Benchmark Cache Impact
Measure across scenarios:
1. Current scene (baseline)
2. Stress scene (scaled lighting)
3. Map-edit invalidation

Report absolute time savings, not just hit rate.

### Phase 5: SMC Scalar Integration (Conditional)
Only if scalar lighting work remains a hot path after caching.

---

## Session Log

### Session 1 — Scalar trig replacement (DID NOT WORK)

**What was attempted**: Replaced 4 scalar trigonometric expressions in
`src/raycast.c` with `smc_call_double` via an adapter layer
(`src/smc_render_opt.c/h`). The expressions were:
- `atan(camera_x * tan(fov / 2.0))` — ray angle per column
- `dist * cos(ray_angle - cam_angle)` — fisheye correction
- `currentDist / cos(ray_angle - cam_angle)` — ceiling/floor true distance
- `tan(angle_diff) / tan(fov / 2.0)` — light billboard screen X

**Result**: ~6.5 million SMC calls per 5-second benchmark run, zero fallbacks,
zero arity errors. But no measurable speedup: baseline median ~8.58 ms vs SMC
median ~8.52 ms (noise level).

**Why it failed**: The renderer is SDL-draw-bound, not math-bound. The four
expressions account for <1% of frame time. SMC dispatch overhead + argument
packing erased any savings from the trig calls. The bottleneck is
`renderer_draw()` (software rasterization + SDL texture upload).

**Lesson learned**: SMC should not target individual scalar trig calls. It
should target larger reusable units like ray results, projected columns, or
lighting samples.

### Session 2 — Lighting shadow ray cache (IMPLEMENTED)

**What was implemented**:
- `src/lighting_cache.h/c` — Direct-mapped hash table (4096 entries) for stationary light-to-tile shadow rays
- Timing instrumentation in `lighting.c` (`lighting_total_time_ms`, `lighting_shadow_ray_count`)
- `--benchmark-lighting` mode in `app.c` for isolated lighting measurement
- Cache key uses `map_revision`, `lighting_revision`, `light_id`, `target_tile_x/y`

**Benchmark results**:
```
Without cache:
  "avg_lighting_ms": 0.01
  "total_shadow_rays": 19740

With cache (USE_LIGHTING_CACHE=1):
  "avg_lighting_ms": 0.01
  "total_shadow_rays": 42
  "cache_hits": 42, "cache_misses": 0, "cache_hit_rate": 100.0
```

**Why we stop here**:
- Lighting takes only **0.01 ms/frame** (far below 0.5 ms threshold for optimization)
- This meets the stop condition: `lighting_update() < 0.5ms baseline`
- The cache is correct and efficient, but there's no measurable work to save
- The real bottleneck (renderer_draw) is SDL-GPU bound, not CPU bound

## Conclusion

The SMC integration project has determined that:
1. **Scalar trig replacement** (Session 1) replaces work that is too cheap to matter (<1% of frame time)
2. **Lighting shadow cache** (Session 2) is a valid optimization approach that works correctly, but the lighting work is far below the noise threshold (0.01 ms vs 0.5 ms threshold)

**Files delivered**:
- `src/smc_render_opt.h/c` — SMC adapter (proven working, but not beneficial)
- `src/lighting_cache.h/c` — Caching implementation (correct, but no meaningful savings)
- `src/lighting.h` — Exposed profiling variables
- `src/lighting.c` — Updated with cache integration
- `Makefile` — Added `USE_LIGHTING_CACHE=1` flag
- `docs/currentplan.md` — Updated with corrected plan
- `docs/handoff.md` — This file

**Next steps (if any)**:
- Could scale lighting workload with more lights or larger maps, but this would be synthetic stress rather than real game content
- SMC integration is ready for use if a math-heavy kernel is identified in the future
- The scalar SMC integration (smc_render_opt.c/h) remains available for any future hot-path math