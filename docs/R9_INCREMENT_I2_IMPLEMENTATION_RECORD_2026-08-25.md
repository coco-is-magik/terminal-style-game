# R9 Increment I2 Implementation Record — Selective Continuation — 2026-08-25

## Status

**Complete and verified.** I2 adds an opt-in, allocation-free selective continuation
API over prepared heightfield intervals and stops before I3 composition. Ordinary
rendering continues to call only the unchanged nearest-hit API. No scene schema,
editor, collision, mirror, preset, compositor, or visible output behavior changed.

## Delivered

### Production selective tracer

Added `heightfield_trace_selective()` and the following bounded result contract in
`src/heightfield_trace.h`:

- at most four `HeightfieldOpticalLayer` records;
- each layer contains the geometric `HeightfieldHit` and its `OpticalResolved`
  semantics, avoiding a second lookup during future I3 composition;
- `terminated_by_surface` for a retained hit whose resolved semantics block sight
  or have zero transmission;
- `reached_opening` only for an explicit missing relevant floor/ceiling;
- `layer_cap_exhausted` when the fourth retained layer remains transmissive;
- natural prepared-range exhaustion remains distinct and sets none of those flags.

Continuation is permitted only when both conditions hold:

```text
!resolved.ray_blocks && resolved.transmission > 0
```

Every yielded hit is resolved through the I1 `OpticalRuntimeView` using the hit's
owner cell index and material ID. Invalid columns, stale optical generations,
mismatched cell counts, invalid screen rows, and null inputs reject without
modifying the caller's output.

### Prepared-interval reuse

`src/heightfield_trace_selective.c` implements a private cursor over the existing
`HeightfieldTraceColumn` interval arrays. Its order is horizontal intersection,
same-interval boundary, then next interval. It performs no DDA and allocates no
memory. The only DDA remains in `heightfield_trace_prepare_column()`.

Shared deterministic horizontal/boundary intersection formulas live in the private
`src/heightfield_trace_internal.h`; this prevents semantic drift without exposing
new public geometry internals.

### Default-path isolation remediation

An initial implementation placed the uncalled selective function in
`src/heightfield_trace.c`. Alternating temporary pre-I2/I2 binaries showed a
consistent translation-unit code-layout/optimization effect on nearest-hit timing,
despite exact checksums and no call sites. I2 was therefore moved to its own
translation unit.

The shipping surface benchmark retains the exact pre-I2 `SRC_RAYCAST` source set.
The ordinary full application includes the production selective module through its
existing complete `src/*.c` build, while focused I2/research runners link it
explicitly. Code search confirms no renderer or editor invokes the I2 API.

## Focused tests

Added `tests/test_heightfield_selective.c`, 9/9 pass under strict `-Werror`:

1. opaque empty-view result exactly matches the shipping nearest hit;
2. transparent material continues to a farther opaque wall;
3. zero transmission stops even when `ray_blocks` is false;
4. `ray_blocks` stops even with positive transmission;
5. transparent layers can reach an explicit proven opening;
6. natural range exhaustion is not mislabeled as an opening;
7. four transparent layers report cap exhaustion, not an opening;
8. sparse owner-cell override beats a material default;
9. invalid/stale/mismatched inputs preserve output bytes.

The runner is part of aggregate `make test`. Historical P2 6/6 and P4 6/6 focused
runners also pass after the production-module split.

## Coverage benchmark

Added `tests/benchmark_heightfield_selective.c` and
`make benchmark-heightfield-selective`:

- 260 × 160 = 41,600 prepared samples/frame;
- 10 warm-up and 200 measured frames;
- coverage points: 0%, 10%, 25%, and diagnostic 100%;
- columns and optical arrays are prepared before timing;
- timed loops allocate zero memory;
- 0% dispatch calls the exact baseline sampler function;
- exact 0% checksum parity: `17812527538433777792`;
- deterministic checksums across three trials after the final terminal-state fix:
  - 10%: `14231836926432377716`;
  - 25%: `11597753142802234765`;
  - 100%: `13655751394315214663`.

The workstation was under visible interactive load, so absolute nonzero-coverage
times varied materially. Final three observed ranges were:

| Coverage | Absolute range | Delta from separately timed baseline |
|---:|---:|---:|
| 0% | 4.516–6.006 ms | -0.051 to +0.983 ms |
| 10% | 5.352–7.489 ms | +0.785 to +2.228 ms |
| 25% | 6.706–8.452 ms | +1.919 to +3.191 ms |
| 100% diagnostic | 11.380–17.071 ms | +6.813 to +11.810 ms |

These deltas are measurements, not preset approvals or linear projections. I2
introduces no unconditional collection. The default 0% path is structurally the
unchanged nearest-hit function; future I3 must use bounded selective coverage and
repeat end-to-end budget validation before exposing visible output.

## Default shipping gates

After translation-unit isolation, the final clean optimized sequence reported:

- shipping surface benchmark raised path: **4.850771 ms**;
- shipping surface stability raised path: **5.460252 ms**;
- both deterministic and below the 6 ms surface budget;
- flat/default checksum equality retained: `5602340901454607159`;
- raised/decal-occlusion checksum equality retained:
  `16569300432624360523`.

Earlier parallel and loaded-system trials exceeded 6 ms while retaining exact
checksums. They were used to trigger investigation, not discarded silently. The
translation-unit A/B identified and removed a real layout sensitivity; final binding
gates were then run sequentially with the pre-I2 shipping module set restored.

## Full verification

- Clean strict optimized `make -j2 check`: pass, including I2 9/9.
- Ordinary application build: pass with all production sources, including the
  isolated selective module.
- Sequential full `make asan && make ubsan`: pass without sanitizer diagnostics.
- ASan exposed one focused-test defect: raw `HeightfieldHit` structure comparison
  included unspecified padding. The assertion now compares every semantic field;
  production behavior was unchanged.
- Final `make benchmark-heightfield-selective`: exact 0% checksum parity,
  deterministic nonzero checksums, and zero timed-loop allocations.
- Final shipping benchmark and stability: pass below 6 ms with exact checksum
  invariants.

## Manual acceptance

Not applicable to I2. The API exposes no visible optical output. Manual optical
acceptance starts when a later increment wires visible composition.

## Stop point

I2 is complete. I3 selective composition remains pending and requires explicit
continuation. Do not begin v6, editor authoring, mirrors, or presets; Review H's four
layers, one bounce, nearest-hit fast path, and no-unconditional-composition
constraints remain binding.