# Current Plan: SMC Ray-Result Caching

## Status: READY FOR IMPLEMENTATION

## Goal
Use SMC to reduce renderer frame cost by caching ray-cast results. The ray-result cache is a C-side renderer artifact cache (SMC remains available for generated-code experiments if needed).

## Agreed Approach

### Phase 1: Isolated Benchmarks
Add `--benchmark-raycast-dda-only` and `--benchmark-raycast-math-only` modes to measure:
- DDA traversal cost
- Projection math + grid_set cost
- SDL rendering cost (full-frame)

### Phase 2: Exact Ray-Result Cache
Pure C implementation behind `USE_SMC_RAY_CACHE=1`.

Cache key (stable bit representation for doubles, no coarse quantization):
```c
typedef struct {
    int map_revision;
    int viewport_width;
    uint64_t fov_bits;
    uint64_t cam_x_bits;
    uint64_t cam_y_bits;
    uint64_t cam_angle_bits;
    int column_index;
} RayCacheKey;
```
- Direct-mapped array (~4K entries, ~160 KB total)
- IEEE 754 bit representation stored directly in key
- RayResult struct stored in cache

### Phase 3: Benchmark Cache Impact
Measure exact cache at:
1. DDA-only (isolated)
2. Raycast math-only (no SDL)
3. Full-frame (with SDL)

Report: hit rate, time saved per hit, fallback cost, memory use.

### Phase 4: Approximate Cache (conditional)
Only if Phase 3 shows DDA is a meaningful cost and exact cache provides speedup.

## Key Decisions Made
1. Cache key stores exact IEEE 754 bit representation (no coarse quantization)
2. No `frame_counter` in cache key — must allow hits across frames
3. SMC is prior context; ray cache is pure C unless generated expressions are later needed
4. Approximate cache deferred until exact cache proves value

## What's Done
- Initial scalar trig replacement (proved too small to matter) — documented in `SMC_INTEGRATION_REPORT.md`
- Scalar SMC integration showed plumbing works (6.5M calls/frame, zero fallbacks, zero arity errors)
- Full-frame time stayed within noise (~8.58 ms vs ~8.52 ms)

## Files to Create/Modify
| File | Action | Phase |
|------|--------|-------|
| `src/config.h` | Add `RUN_MODE_BENCHMARK_DDA_ONLY`, `RUN_MODE_BENCHMARK_MATH_ONLY` | 1 |
| `src/app.c` | Add benchmark modes, stats reporting | 1, 3 |
| `src/raycast.c` | Add DDA-only entry point, wrap `raycast_fire` with cache | 1, 2 |
| `src/raycast.h` | Declare new functions | 1, 2 |
| `src/ray_cache.h` | Create cache API | 2 |
| `src/ray_cache.c` | Create cache implementation | 2 |
| `Makefile` | Add `USE_SMC_RAY_CACHE=1` toggle | 2 |