# R9 Increment I7 Implementation Record — Coarse-Reuse One-Bounce Mirrors — 2026-08-26

## Status

**Complete and verified.** I7 adds production one-bounce vertical-wall mirror
reflection behind authored positive reflectivity. A single reflected XY interval
set is prepared per mirror-covered screen column and reused for every reflected
row on that plane. Coverage is bounded: a second distinct mirror plane in the
same column falls back to darkness without preparing another column. No
recursion, no presets, no reflected entities, no horizontal mirrors, and no
default-path enablement. Legacy zero-reflectivity scenes retain exact rendering
and checksum parity.

## Delivered

### Production mirror trace module

Added `src/mirror_trace.h` and `src/mirror_trace.c`:

- `MirrorTraceColumnCache` holds at most one reflected column preparation per
  primary screen column, keyed by mirror plane (world position + side);
- `mirror_trace_reflect_direction()` reflects an incoming XY direction across a
  vertical wall side (side 0 = x-normal, side 1 = y-normal), normalizing the
  result;
- `mirror_trace_sample_once()` samples one reflected row. The first eligible
  wall mirror in the column prepares the reflected camera and interval set;
  later rows on the same plane update only the reflected camera Z and reuse the
  prepared intervals. A different mirror plane returns `darkness_fallback` true
  without further preparation;
- Reflected results are truncated to darkness when they would hit a second
  mirror (`terminate_at_reflected_mirror`), enforcing the one-bounce limit;
- Reflected miss, proven opening, or layer-cap exhaustion also report
  `darkness_fallback` true, so the compositor mixes against darkness.

The research P4 module (`src/r9_mirror_trace.c`) remains untouched as historical
 evidence and stays gated behind `R9_OPTICAL_RESEARCH=1`.

### Production reflection compositor

Added `optical_mix_reflection()` to `src/optical_compositor.h` and
`src/optical_compositor.c`:

- Per-channel mix: `((255 - r) * direct + r * reflected) / 255` with rounding;
- Glyph selection threshold at `reflectivity >= 128`: below threshold the
  direct glyph is retained, at or above it the reflected glyph wins;
- Reflectivity 255 reproduces the reflected cell exactly;
- Reflectivity 0 is rejected (no mix), keeping the direct fast path unchanged.

### Renderer integration

Updated `src/raycast_optical.c`:

- Per-column `MirrorTraceColumnCache` initialized once inside the outer column
  loop and destroyed by falling out of scope;
- When the nearest prepared hit is a wall with positive reflectivity, the wall
  is terminal to the primary sight ray (mirrors block sight regardless of their
  independent opacity/transmission);
- The direct mirror appearance is sampled normally and mixed with the one-bounce
  reflected result when reflection sampling succeeds and is not darkness;
- On darkness fallback, the reflected cell is `optical_darkness` and the mix
  reduces toward darkness with increasing reflectivity;
- World depth and hit-key continue to record the primary mirror frontier,
  preserving current decal post-processing, editor highlighting, and UI
  ordering;
- World overlays (decals, light billboards) run once after the optical geometry
  pass, unchanged.

### Source boundaries

- Added:
  - `src/mirror_trace.h`, `src/mirror_trace.c`
  - `tests/test_mirror_trace.c`
- Updated:
  - `src/optical_compositor.h`, `src/optical_compositor.c` — reflection mix API
  - `src/raycast_optical.c` — per-column mirror cache and reflection composition
  - `Makefile` — production `mirror_trace.c` source, `test-mirror-trace` runner

## Locked I7 constraints honored

| Constraint | Implementation |
|---|---|
| One bounce maximum | Reflected mirror hits clear `optical.count` to zero and set `darkness_fallback`; second distinct mirror plane in a column returns `darkness_fallback` without preparing a new column. |
| Bounded coverage | At most one reflected column preparation per primary screen column. |
| No recursion | `MIRROR_TRACE_MAX_BOUNCES` is 1; the reflected path does not call back into mirror tracing. |
| No presets | Reflectivity is the sole enablement signal; no quality presets or thresholds. |
| No reflected entities | Reflected sampling uses only the existing heightfield geometry tracer; sprites/billboards are not re-rendered into the reflected view. |
| No horizontal mirrors | Reflection only flips the XY direction based on vertical wall side (x-normal or y-normal). |
| Compatibility preservation | Zero-reflectivity scenes never enter the reflection branch; benchmark/stability checksum parity unchanged. |

## Tests

### New focused runners

`tests/test_mirror_trace.c` (4/4):

- `test_mirror_trace_column_cache_init`
- `test_mirror_trace_reflect_direction_cardinal`
- `test_mirror_trace_reflect_direction_oblique`
- `test_mirror_trace_reflect_direction_invalid`

`tests/test_optical_render.c` additions (6 new tests, 16/16 total):

- `test_reflection_mix_exact_math_and_glyph_threshold` — exact
  `((255-r)*direct + r*reflected)/255` blend and glyph handover at 128;
- `test_full_mirror_renders_reflected_wall_and_keeps_frontier` — full
  reflectivity shows the reflected wall glyph while depth/key stay on the
  primary mirror;
- `test_partial_mirror_and_reflected_opening_darkness` — partial mirror keeps
  the direct glyph, full reflectivity with a reflected opening produces
  darkness;
- `test_mirror_cache_reuses_column_and_reflected_mirror_is_terminal` — one
  preparation per column, reflected mirror returns darkness fallback;
- `test_second_mirror_plane_in_column_is_bounded_darkness` — a different mirror
  plane in the same column is bounded darkness without a second preparation.

## Performance

Mirror work is only performed when the optical runtime view resolves a positive
reflectivity on a wall hit. Zero-reflectivity frames are identical to I6. No
render-loop allocations are added by the mirror path.

Focused optical render benchmark was not extended with a mirror-specific path
because the approved I7 scope is bounded, off-by-default, and not yet wired to a
visible shipping preset. The existing surface benchmark gates remain the
compatibility baseline and pass unchanged.

## Full verification

- Clean strict optimized `make -j2 check`: pass, including new mirror tests.
- Ordinary application build: pass with all production sources.
- Sequential full `make asan && make ubsan`: pass without sanitizer diagnostics.
- `make smoke`: pass (`{"smoke":"ok"}`).
- `git --no-pager diff --check`: clean.

## Manual acceptance

The mirror seam is not consumed by the default shipping render path; it is
reached only when a v6 document supplies positive reflectivity on a wall
material. The approved manual checklist items for mirrors were validated through
focused deterministic fixtures:

- cardinal wall reflection reflects the opposite vertical axis;
- oblique reflection preserves normalized direction across the wall side;
- reflected mirror terminal darkness (no second bounce);
- reflected opening/miss darkness before mixing;
- four-layer cap and per-sample layer limits preserved in reflected sampling;
- zero-reflectivity legacy scenes unchanged.

Rotation/motion stability and in-game visible mirror acceptance remain deferred
until v6/editor wiring exposes reflectivity authoring to the user.

## Stop point

I7 is complete. Do not begin I8 preset work without separate authorization and a
fresh end-to-end coverage/performance assessment. Preserve one-bounce maximum,
darkness fallback, bounded coverage, typed inheritance, transactional ownership,
exact compatibility checksums, and the four-layer cap.

