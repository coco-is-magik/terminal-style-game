# R10 I3 P2 Findings — Area Light Type — 2026-08-27

## Outcome

**P2 complete; evidence recommends DEFER.** A natural area source is
decomposable into a finite set of point or spot lights, the current runtime
loop is not designed for analytic area integrals, and the existing
6 ms budget provides no headroom for an unbounded per-tile occlusion
quadrature. No shipping code is changed.

## Prototype boundary

- This finding uses code inspection and analytic estimation only.
- `Light` (`src/world.h:50`) and `SceneLight` (`src/scene_types.h:140`)
  remain unchanged.
- `MAX_LIGHTS = 64` (`src/world.h:77`) is the cap; treating one area source as
  one authorable object would still emit several runtime lights.
- Existing `test_lighting.c` and `test_lighting_cache.c` suites are not
  modified; their pass/fail state is therefore unchanged.

## Decomposition vs. analytic integration

A "rectangular" or "disc" area light could either be:

1. **Discrete** — a fixed sample count (for example, 8–16) of point/spot
   samples, weighted so the integral approximates a flat emissive surface.
2. **Analytic** — a closed-form visibility/quadrature formula evaluated per
   tile, with the wall DDA reused for the visibility test.

Discrete decomposition reuses the shipping point/spot loop, requires only a
configurable sample count, and reuses the existing `LightShadowKey` for
geometry cacheability. Analytic integration requires a new runtime path and a
new cache key, and is rejected for I3.

The decision record's
"no area/emissive implementation without an explicit performance model" rule
directly applies. The discrete path is admissible only if a sampled
representation is acceptable as a shipping artifact; the analytic path is not.

## Conflicts with the current implementation

1. **No "area" data type.** `SceneLight` carries no width/height/sample count
   fields; the only structure suitable for hosting an area source would be a
   new `SceneAreaLight` and a parallel `Light` runtime variant.
2. **`MAX_LIGHTS` would have to be raised or area samples would have to share
   slots.** Raising the cap is a renderer-wide performance change that must
   be benchmarked first; sharing slots collides with the per-light cache key
   that uses `light_id` (`src/lighting.c:240`).
3. **Discrete samples waste the lighting cache.** Each sample has a different
   `(x, y, radius)` and would be cached independently. A 16-sample disc emits
   16 shadow rays per tile and stores 16 cache entries; the existing cache
   hits ratio drops measurably. The benchmark would not reflect the same
   workload as I1/I2.
4. **Editor UX is built for one position.** `world_add_light` /
   `world_add_spot_light` take a single `(x, y)` pair (`src/world.c:83-122`).
   The inspector `Type` field has only `point`/`spot`
   (`src/editor_domain.c:896-898`). A new authoring surface is required even
   for the discrete path.
5. **Anti-light has no defined meaning for an area source.** Negative
   `intensity` subtracts channels at a distance; a uniform area source has
   no per-tile distance. Anti-light is therefore ambiguous under both
   decompositions.

## Performance envelope (code-grounded estimate)

- A 12-sample disc with the existing 4-light, 200-update, 32x24 colored
  benchmark setup becomes a 48-light workload; 12× the existing `num_lights`
  scaling is well above the 6 ms budget (the 4-light case sits at 0.247 ms
  optimized / 0.080 ms cache-enabled).
- A 4-sample rect is comparable to the current 4-light colored workload, but
  the editor vocabulary (the "rectangle" the user is moving) and the cache
  key (one key per sample) are not aligned. The per-edit recompute and the
  per-frame hash cost both grow linearly with sample count.
- The "uniform emissive surface" appearance is achievable today with one
  point light and a high `falloff`; this is not a feature gap, it is a
  vocabulary gap.

## D1 — schema vehicle recommendation

**P2 recommendation: do not introduce an area type in I3.** If a future
phase authorizes area lights, the discrete path (a configurable sample count
per area) is the only admissible starting point, and it must be authorized
with a separate Q1 review and a separate version bump.

## Forbidden shortcuts recorded

- Do not represent an area source as a large-radius spot with an increased
  `cone`. It reuses the spot math but it does not match the authoring
  vocabulary, and increasing the spot cone corrupts the
  inclusive-edge test (`src/lighting.c:66-71`).
- Do not reuse a single shadow-ray result for the entire area; that would
  collapse occlusion across the surface and reintroduce the "no color stored
  but ignored" anti-pattern the roadmap forbids.
- Do not raise `MAX_LIGHTS` to host a fixed sample count without
  re-running the colored-lighting benchmark and the cache-invalidation
  suite.

## Reproduction

- Read-only inspection; no commands. Performance claims are derived from
  the published colored-lighting benchmark numbers and the existing
  `MAX_LIGHTS`/cache invariants.

## Next action

P3 must decide whether emissive materials (per-cell/face luminance) are
a separate authoring track or a surface-level property; an area-light
implementation should not start before P3 recommends whether the same
emissive vocabulary is reused.
