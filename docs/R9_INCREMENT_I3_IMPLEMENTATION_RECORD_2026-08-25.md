# R9 Increment I3 Implementation Record — Selective Optical Composition — 2026-08-25

## Status

**Complete and verified.** I3 wires the I2 selective tracer and an I1-resolved
production compositor into an opt-in `raycast_render_height_optical()` seam.
Default rendering still calls `raycast_render_height()` and is unchanged. No
schema, editor, collision, mirror, preset, or visible application behavior
changed.

> **Historical dispatch note:** the opt-in seam described here was superseded on
> 2026-08-27. `raycast_render_height_optical()` is now the sole production and test
> heightfield renderer for both inherited/default and authored optical data. The
> former entry point remains compiler-deprecated for rollback only.

## Delivered

### Production compositor

Added `src/optical_compositor.h` and `src/optical_compositor.c` implementing
far-to-near terminal-cell optical composition per the P3 evidence:

- Blend background-to-foreground using resolved opacity and transmission:
  `out.rgb = surface.rgb * (surface.a/255) + behind.rgb * (transmission/255)`;
- Nearest layer with opacity ≥ 128 supplies the output glyph;
- Explicit darkness fallback when the ray reaches an opening or exhausts the
  layer cap without a blocking surface;
- Distinct terminal states: `terminated_by_surface`, `reached_opening`, and
  `layer_cap_exhausted`;
- Transactional validation rejects impossible layer/state sequences
  (non-transmitting interior layers, contradictory terminal flags, cap without
  four layers, zero layers, null inputs) without mutating the output, while still
  accepting natural range exhaustion (a transmitting final layer with no terminal
  flag) as a valid darkness fallback case.

The research P3 module (`src/r9_optical_compositor.c`) remains untouched as
historical evidence.

### Renderer seam

Added `src/raycast_optical.c` and `src/raycast_internal.h`:

- `raycast_render_height_optical()` borrows the existing grid, map, camera,
  assets, world, authored surfaces, prepared height view, and an I1 optical
  runtime view;
- Invalid, stale-generation, or cell-count-mismatched optical inputs fall back
  fully to `raycast_render_height()`;
- Per sample the renderer resolves the nearest prepared hit through I1:
  - if the resolved hit blocks sight or has zero transmission, emit the ordinary
    sampled cell directly and skip I2 continuation/composition;
  - otherwise continue selectively from the nearest hit and compose the returned
    layers;
- All optical geometry layers are sampled with the existing material, distance
  glyph, palette, scalar light-map, side attenuation, and missing-material
  rules via `raycast_sample_heightfield_hit()`;
- World depth and hit-key always record the nearest layer, preserving current
  decal post-processing, editor highlighting, and UI ordering;
- World overlays (decals, light billboards) run once after the optical geometry
  pass, unchanged.

### Continuation-after-nearest optimization

Added `heightfield_trace_continue_after_nearest()` so the renderer does not
re-resolve or re-sample the nearest hit that it already owns. The I2 cursor
advances past the verified nearest layer and continues only for additional
transmissive layers. The original `heightfield_trace_selective()` API and all
I2 tests remain unchanged.

### Source boundaries

- Added:
  - `src/optical_compositor.h`, `src/optical_compositor.c`
  - `src/raycast_internal.h`
  - `src/raycast_optical.c`
  - `tests/test_optical_render.c`
  - `tests/benchmark_optical_render.c`
- Updated:
  - `src/heightfield_trace.h` — `heightfield_trace_continue_after_nearest()` declaration
  - `src/heightfield_trace_selective.c` — continuation-after-nearest implementation
  - `src/raycast.h`, `src/raycast.c` — extracted private renderer helpers
    `raycast_sample_heightfield_hit()`, `raycast_world_hit_key()`,
    `raycast_heightfield_column_depth()`, `raycast_render_world_overlays()`
  - `Makefile` — `test-optical-render`, `benchmark-optical-render` targets

The shipping surface benchmark retains the exact pre-I2 `SRC_RAYCAST` module set;
optical modules are linked only into focused runners and the full application.

## Focused tests

Added `tests/test_optical_render.c`, 11/11 pass under strict `-Werror`:

1. opaque/stale optical views preserve complete-frame compatibility parity;
2. a transparent wall composes over a farther opaque wall;
3. two transparent layers produce deterministic output;
4. a transparent floor over an opening falls back to darkness;
5. production compositor exact math matches hand-computed blends;
6. compositor glyph threshold, four-layer cap, and transactional validation reject
   invalid states without mutating output;
7. natural range exhaustion is accepted as a valid darkness-fallback terminal case;
8. a generated discontinuity participating in a blend matches direct composition;
9. four transmissive walls render the cap-exhaustion state deterministically;
10. a wall decal on the nearest optical supporting surface remains after the optical
    geometry pass;
11. a light billboard remains after optical geometry.

## Coverage benchmark

Added `tests/benchmark_optical_render.c` and `make benchmark-optical-render`:

- 260 × 160 = 41,600 samples/frame;
- 10 warm-up and 200 measured frames;
- compatibility path, optical opaque path, and localized transmissive coverage;
- zero render-loop allocations;
- exact opaque-frame checksum parity with compatibility rendering.

Representative observed values under the current interactive host (isolated run):

| scenario | checksum |
|---|---:|
| compatibility | `17276792261464593835` |
| optical opaque | `17276792261464593835` (parity) |
| localized transparent | `18106365475393592681` |

Changed cells with one deep transparent wall cell: **378 / 41600 = 0.909%**.

Because the host is under variable interactive load, absolute millisecond times
vary materially between runs. A clean isolated run after optimization reported:

| path | ms | delta from compatibility |
|---|---:|---:|
| compatibility | ~6.35 | — |
| optical opaque | ~6.91 | +0.56 |
| localized transparent | ~7.44 | +1.09 |

These are measurements, not preset approvals. Broad coverage (e.g., an entire
map-wide transmissive plane) exceeds the surface budget and is intentionally out
of scope for I3; presets and coverage limits will be decided in I8 after v6 and
A broad-coverage diagnostic stress fixture (e.g., a map-wide transmissive wall
plane) was used during development and deterministically exceeded the 6 ms
surface budget. That result confirms the I3 seam is viable only under bounded
selective coverage; preset/coverage policy remains deferred to I8.

editor authoring expose real authored scenes.

## Shipping gates

Final clean isolated surface rendering gates pass below the 6 ms budget:

- shipping surface benchmark raised path: **5.058856 ms**;
- shipping surface stability raised path: **5.171748 ms**;
- shipping surface benchmark occluded decal path: **5.256617 ms**;
- shipping surface stability occluded decal path: **5.225264 ms**;
- deterministic and exact checksum invariants retained:
  - flat/default: `5602340901454607159`;
  - raised/decal-occlusion: `16569300432624360523`.

## Full verification

- Clean strict optimized `make -j2 check`: pass, including I3 10/10.
- Ordinary application build: pass with all production sources.
- Sequential full `make asan && make ubsan`: pass without sanitizer diagnostics.
- `make benchmark-heightfield-selective`: exact 0% checksum parity
  `17812527538433777792`, deterministic nonzero checksums, zero timed-loop
  allocations.
- `make benchmark-surface-render`: pass below 6 ms with exact checksum invariants.
- `make stability-surface-render`: pass below 6 ms with exact checksum invariants.
- `git --no-pager diff --check`: clean.

## Manual acceptance

The optical seam is not yet consumed by the application: the default render path
still calls `raycast_render_height()`, and no v6 view is supplied. Therefore
in-game manual optical acceptance is not applicable for I3. The plan's visible
manual checklist items were validated through focused deterministic fixtures:

- default/legacy scenes byte-for-byte unchanged (opaque parity test);
- single translucent over a wall readable/stable (transparent wall composition test);
- two translucent layers legible and deterministic (two-layer deterministic test);
- translucent opening darkness without stale framebuffer (opening darkness test);
- decal ordering correct on the nearest optical surface (wall decal test).

Editor/UI ordering, mirror behavior, and rotation/motion stability remain
unchecked at the manual level because the visible application surface has not
changed; they will be exercised when v6/editor wiring exposes optics to the user.

## Stop point

I3 is complete. I4 is the mandatory pre-v6 stop-gate review. Do not begin v6
schema migration, editor authoring, mirrors, or presets until Review H's four
layers, one bounce, nearest-hit fast path, no-unconditional-composition, and 6 ms
budget constraints are formally reviewed against the I1–I3 evidence.
