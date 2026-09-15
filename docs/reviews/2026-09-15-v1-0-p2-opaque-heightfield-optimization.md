# V1-0 P2 Opaque Prepared-Heightfield Optimization — 2026-09-15

## Outcome

**Complete and verified.** The opaque prepared-heightfield surface blocker is closed without
changing the renderer pipeline, visual semantics, benchmark methodology, or 6 ms budget.

## Implementation

`heightfield_trace_prepare_column()` now records contiguous runs whose occupancy, surface
presence, and floor/ceiling heights are equal. The new opaque prepared sampler:

1. binary-searches the projected horizontal distance within each run;
2. samples the actual owning interval so material changes remain visible;
3. evaluates only the terminal interval for missing-surface or generated-boundary behavior;
4. shares that interval evaluator with the original generic sampler.

The generic sampler remains the exact reference. Optical selective continuation,
composition, mirrors, overlays, persistence, and editor behavior were not changed.

## Correctness evidence

The focused tracer fixture compares every optimized hit field with the generic sampler for
all 41 × 40 viewport samples under:

- ground and airborne camera heights;
- level and fractional-pitch views;
- flat and raised floor/ceiling runs;
- differing materials inside one height run;
- missing floor and ceiling surfaces;
- multiple walls and generated boundaries;
- invalid prepared input.

The core renderer's flat parity, authored-height difference, decal, lighting, horizon, and
width tests pass. Optical render tests pass 19/19, including mirror curvature/viewpoint,
one-bounce/cache, darkness fallback, overlays, and exact composition.

All final surface trials retained these checksums:

- null: `11926338986916195251`;
- authored: `17039363608365970015`;
- flat height: `76533440368989277`;
- raised height: `212879036348074338`;
- occluded decal: `212879036348074338`.

## Final isolated performance evidence

Raw outputs are in `build/p2-opaque-closeout/`.

| Mode | Trials | Flat range | Raised range | Result |
|---|---:|---:|---:|---|
| benchmark, 200 iterations/path | 5 | 2.403860–4.817409 ms | 3.896334–4.656326 ms | 5/5 pass |
| stability, 1,000 iterations/path | 3 | 2.440316–3.378517 ms | 3.920074–4.968514 ms | 3/3 pass |

Every trial was deterministic and below the unchanged 6 ms gate. Earlier concurrent or
loaded-system runs included deterministic raised-path overruns; they were not used as
qualifying evidence and were not erased from the session record.

## Verification

- strict GCC and local Clang focused tracer builds: pass;
- tracer owner: 10/10;
- core owner: 61/61;
- optical owner: 19/19;
- mirror owner: 4/4;
- sprite-render owner: 6/6;
- focused ASan/LeakSanitizer and UBSan: pass with no diagnostics;
- strict application build, complete functional aggregate, and `standards-core`: pass.

## Separate finding and next action

`benchmark-headless` and `stability-headless` now pass their surface-render phase, then fail
the independent `benchmark-optical-render` transparent/mirror phase. Opaque optical parity is
about 3.1 ms and exact; localized transparent and one-bounce mirror paths are approximately
10.3–10.4 ms against the same 6 ms budget. Checksums are deterministic and timed-loop
allocations remain zero.

That finding is not waived or folded into P2. The next safe action is a focused optical
composition performance reproduction/optimization increment preserving current checksums,
one-bounce/cache rules, allocation behavior, and budget.