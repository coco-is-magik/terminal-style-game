# R10 I3 P1 Findings — Directional Light Type — 2026-08-27

## Outcome

**P1 complete; evidence recommends DEFER until an integrated optical/prepared
column prototype is available.** Compile-time-gated analysis identifies the
required model, the cost of naive integration, and the conflicts with existing
R9 occlusion rules and the R10 I2 spot geometry. No shipping code, scene data,
cache key, or runtime value is changed.

## Prototype boundary

- This finding uses code inspection only; it introduces no new translation unit.
- `SceneLight` (`src/scene_types.h:140`) and `Light` (`src/world.h:50`) keep
  `type ∈ {POINT, SPOT}`; a `directional` member would require a new variant.
- `scene_format.c:1275-1287` and `scene_format.c:2099-2118` remain v7-only aware.
- No tests are added; the `test_lighting.c` and `test_lighting_cache.c` suites
  are untouched and therefore still pass under strict/ASan/UBSan.

## Required model

A directional source is defined by `direction_unit` (already present for spots),
`luminance` (a per-channel scalar constant analogous to `intensity`), and no
`radius`. Shadow policy must reject tiles whose primary ray from a representative
sample point (for example, a light "anchor" at infinity along `direction_unit`)
hits a wall before reaching the tile.

## Conflicts with the current implementation

1. **`lighting.c` uses a `pos.x, pos.y, radius` bounding box** (`src/lighting.c:213-221`)
   and the tile-center test `dist <= l->radius` (`src/lighting.c:233`). A
   direction has no finite origin, so the bounding box and the per-tile distance
   test no longer describe a directional contribution. Adding the type requires
   either a separate loop or a new synthetic origin, and either choice changes
   the cache key.
2. **`LightShadowKey` (`src/lighting_cache.h:41`) bakes the light's `(x, y,
   radius)` bits** plus a `light_id` integer. A directional light has no `(x, y,
   radius)`, so the same key type cannot describe both. Two key types or a new
   union field is required; the existing `test_lighting_cache.c` is keyed by
   `light_id` and is therefore not invalidated today.
3. **Per-tile shadow ray budget is shared with point/spot lights.** Today the
   per-frame work is `O(num_lights × radius² × raymarch)`. One directional light
   applied to a 64×32 map is 2,048 tiles; ten such lights without a coarse
   pre-cull are 20,480 shadow rays per frame, an order of magnitude more than
   the existing 6 ms budget for the colored-lighting benchmark.
4. **Anti-light semantics are currently per-light, not per-tile.** A point light
   uses `intensity < 0` to subtract channels; a directional source at infinity
   cannot reach a tile's "distance from light," so subtracting tile-lightness
   with `intensity` becomes ambiguous. The R10 decision record forbids the
   "anti-light" use case in directional terms; the prototype must keep
   anti-light as a separate, future authoring choice.

## Spot/directional overlap analysis

- Spot lights with `cone = 2π` and the existing angular test are already
  equivalent to a circular area source with radial falloff. A "directional"
  variant is not a generalization of spot; it is a distinct topology
  (no positional anchor) and the current geometry algebra (a) becomes
  undefined or (b) requires a synthetic anchor in `+direction_unit`.
- Editor UI today exposes a numeric `0..1` type field plus bounded
  `Direction/Step=0.05` (`src/editor_domain.c:899-903`). Adding `directional`
  would either need a third type or a new `kind` enum. Either is a v8 bump
  with strict migration; reusing the spot representation is rejected because
  it would silently redefine the semantics of a documented `spot` light.

## Performance envelope (code-grounded estimate)

- Existing 4-light, 200-update, 32x24 colored benchmark: white 0.247 ms,
  colored 0.226 ms, spot 0.152 ms (cache-on, budget 6.0 ms).
- A single unidirectional pre-cull that skips tiles whose `dot(outward_tile,
  direction_unit) < 0` is a cheap pass but still requires one shadow ray per
  visible tile. Without a per-frame prepared interval walk reused across
  columns, the per-tile cost is the same as a 64-tile radius point light.
- Conclusion: no evidence supports the claim that one directional light fits
  in the existing 6 ms budget on a 64×32 map; the unverified claim must be
  measured before any implementation.

## D1 — schema vehicle recommendation

**P1 recommendation: do not introduce a v8 directional variant in this increment.**
v7 was just locked for spot math; reusing `type` would corrupt spot semantics
and renaming `type` to a `kind` enum would invalidate every saved scene and
test fixture. Any directional product work must be authorized as a separate
phase and start with its own decision record.

## Forbidden shortcuts recorded

- Do not represent a directional light as a spot with `cone = 2π` at a
  synthetic anchor; it would change the spot cache key, the inspector
  Round-trip, and the v7 spec.
- Do not implement a directional path that runs in the same loop as
  point/spot; the bounding-box and per-tile distance tests are not
  meaningful and would silently drop performance-critical pre-culls.
- Do not add a `kind` enum to `SceneLight` without a separate Q1 review.

## Reproduction

- Read-only inspection; no commands. The colored-lighting benchmark
  (`make benchmark-colored-lighting`) provides the performance baseline
  referenced above.

## Next action

P2 must decide whether an area source can reuse the spot/point loop and
whether a directional source requires a new prepared-interval walk; P3 must
revisit the area decision under a single representative engine. Review H
(or an equivalent roadmap checkpoint) must authorize any v8 plan.
