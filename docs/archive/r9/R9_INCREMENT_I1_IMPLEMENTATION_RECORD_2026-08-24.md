# R9 Increment I1 Implementation Record — Derived Optical Runtime Lookup — 2026-08-24

## Status

**Complete and verified.** I1 adds the production allocation-free derived optical
lookup seam and stops before I2 selective continuation. It does not change
`SceneDocument`, scene v5, `Material`, collision, tracing, rendering, editor behavior,
or visible output.

## Delivered

### Production borrowed view

Added `src/optical_runtime_view.h/.c`:

- `OpticalExtension`: compact eight-byte independent override data;
- `OpticalResolved`: independent player/ray/light blocking, opacity, transmission,
  and reflectivity;
- `OpticalCellOverride`: 12-byte sparse record keyed by flat cell index;
- `OpticalRuntimeView`: borrowed pointers/counts, cell count, source generation,
  and validity; owns no memory;
- `optical_runtime_view_init`: transactional validation with no allocation;
- `optical_runtime_view_resolve`: legacy → O(1) material default → binary-searched
  sparse cell override;
- `optical_runtime_view_is_current`: generation comparison for stale-view rejection.

The optional material/default and cell-override arrays are future-v6 adapter inputs.
When absent, resolution derives exact current behavior from the legacy occupancy/
blocking value. I1 deliberately adds no authored optical fields before the I4 gate.

### Validation contract

Initialization rejects without modifying the output when:

- output is null or cell count is zero;
- pointer/count pairs disagree;
- material capacity exceeds 65,536;
- sparse count exceeds cell count;
- an extension has an unknown mask bit, non-boolean active field, or nonzero
  reserved byte;
- sparse cell indices are out of range, duplicated, or not strictly increasing.

Lookup rejects invalid views, out-of-range cell indices, and null output without
modifying caller output. Callers compare source generations with
`optical_runtime_view_is_current()` before using a borrowed view. Out-of-range material IDs fall back to legacy
semantics; sparse cell overrides remain authoritative after any material default.

## Tests

Added `tests/test_optical_runtime_view.c`, 7/7 pass:

1. exact occupied/empty legacy semantics;
2. empty optional-input compatibility path and generation metadata;
3. material-default then cell-override precedence;
4. independent fields and out-of-range material fallback;
5. sparse ordering/bounds transactional rejection;
6. invalid extension and pointer/count rejection;
7. lookup invalid-input output preservation.

The runner is part of aggregate `make test`.

## Performance evidence

Added `tests/benchmark_optical_runtime_view.c` and
`make benchmark-optical-runtime-view`:

- 41,600 queries/frame;
- 20 warm-up frames;
- 500 measured frames;
- matched deterministic hashing for direct legacy and empty-view resolution;
- hard overhead gate: ≤0.100 ms/frame;
- exact checksum parity: `17991526238798329539`.

Three final trials:

| Trial | Legacy | View | Delta | Result |
|---:|---:|---:|---:|---|
| 1 | 0.877307 ms | 0.570046 ms | -0.307261 ms | pass |
| 2 | 0.885132 ms | 0.577693 ms | -0.307439 ms | pass |
| 3 | 0.872891 ms | 0.572350 ms | -0.300541 ms | pass |

The negative delta is not claimed as a production speedup; it means no measurable
regression appears in this isolated harness. The unchanged shipping surface gates
remain the authoritative end-to-end evidence.

## Full verification

- Strict optimized focused runner: 7/7 pass under `-Werror`.
- Ordinary application build: pass with the production module and no consumer wiring.
- `make -j2 check`: pass through I1.
- Full sequential `make asan && make ubsan`: pass without diagnostics.
- Shipping surface benchmark: raised path 5.343303 ms, deterministic, below 6 ms.
- Shipping surface stability: raised path 5.114752 ms, deterministic, below 6 ms.
- Exact flat/default checksum parity retained (`5602340901454607159`).
- Occluded-decal/raised checksum equality retained (`16569300432624360523`).
- No per-frame allocation was introduced; the I1 module performs no allocation.

## Manual acceptance

Not applicable to I1. No renderer or visible optical behavior is wired. The user
manual checklist begins at the first visible optical increment.

## Stop point

I1 is complete. Begin I2 selective continuation only after confirmation. Do not
start v6/editor work before I4 passes, and keep mirrors disabled.
