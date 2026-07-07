# SMC Renderer Integration — Handoff Document

## Overview

This document tracks the SMC (Self-Modifying Calculator) integration into the
terminal-style-game raycasting renderer. The goal is to use SMC as an adaptive
computation system to reduce renderer frame cost by caching or generating
reusable renderer artifacts — not by replacing individual scalar math calls.

## Repository

- **Target project**: https://github.com/coco-is-magik/terminal-style-game
- **SMC project**: https://github.com/coco-is-magik/self-modifying-calculator

## Plan (agreed)

### Phase 1: Isolated benchmarks
Add `--benchmark-raycast-dda-only` and `--benchmark-raycast-math-only` modes
to measure where the renderer time actually goes (DDA traversal vs projection
math vs SDL draw).

### Phase 2: Exact ray-result cache
Implement a C-side cache keyed by `(map_revision, camera_pose_revision, column_index)`.
`camera_pose_revision` covers: pos.x, pos.y, angle, fov, viewport_width.
No SMC hash functions — pure C direct-mapped ring buffer.

### Phase 3: Approximate ray-result cache
Separate experiment with quantized origin + angle buckets. Requires correctness
validation against the exact cache.

### Phase 4: Three-level measurement
1. DDA-only (without cache vs with exact cache vs with approximate cache)
2. Raycast math-only (no SDL draw)
3. Full-frame (including SDL draw)

Report hit/miss stats, time saved per hit, fallback cost, memory use.

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

### Session 2 — Plan revision

**What changed**: Based on review feedback, the approach shifted from "replace
math" to "cache renderer artifacts." The agreed plan is documented above.

**Key design decisions**:
- Cache key uses `camera_pose_revision` covering all camera state that affects
  ray generation (pos.x, pos.y, angle, fov, viewport_width)
- No `frame_counter` in the cache key — hits across frames must be possible
- Exact cache first (keyed by column_index + pose revision), then approximate
  cache (quantized origin + angle buckets) as a separate experiment
- SMC's role is orchestration and generated metadata, not hash computation

### Session 3 — Phase 1: Isolated benchmarks (IN PROGRESS)

**What's being done**: Adding `--benchmark-raycast-dda-only` and
`--benchmark-raycast-math-only` modes to measure the true cost distribution.

**Challenges**:
- Need to separate DDA time from projection math time without duplicating code
- The existing benchmark infrastructure measures `renderer_draw()` time, not
  raycast math time
- Need to add new `RunMode` values to `config.h`

---

## Files created/modified

| File | Status | Description |
|------|--------|-------------|
| `src/smc_render_opt.h` | Created (Session 1) | SMC adapter header |
| `src/smc_render_opt.c` | Created (Session 1) | SMC adapter implementation |
| `scripts/generate-smc-renderer.lisp` | Created (Session 1) | SMC warm-cache + C generator |
| `SMC_INTEGRATION_REPORT.md` | Created (Session 1) | Initial integration report |
| `docs/handoff.md` | Created (Session 2) | This file |

## Open questions

- How much of the frame time is DDA traversal vs projection math vs grid_set
  vs SDL draw? (Phase 1 will answer this.)
- Is the DDA loop expensive enough that caching ray results saves meaningful
  time? (Phase 2 will answer this.)
- Can quantized angle caching produce correct results within floating-point
  tolerance? (Phase 3 will answer this.)
