# R10 I3 RQ5/RQ6 Synthesis — Light-Type Decisions and Performance Envelope — 2026-08-27

## Status and scope

**Research synthesis complete; ready for the R10 Q1 review, not
implementation planning.** This document combines the P1–P3 evidence for
directional, area, and emissive lighting into a single recommendation, a
preserved policy table, and a recorded envelope. It adds no code, ships
no new type, and bumps no scene version.

The individual P1/P2/P3 finding documents are the authoritative evidence:

- `R10_I3_P1_DIRECTIONAL_FINDINGS_2026-08-27.md`
- `R10_I3_P2_AREA_FINDINGS_2026-08-27.md`
- `R10_I3_P3_EMISSIVE_FINDINGS_2026-08-27.md`

The locked Q1 decision record (`R10_DECISION_RECORD_2026-08-27.md`)
governs the routing of this synthesis.

## Required invariants

- v7 is the canonical native writer format; introducing a new light type
  requires a v8 bump with strict migration and reserved mask bits.
- The lighting loop and the `LightShadowKey` are stable for v7. A new
  type must reuse the existing key or be paired with a new
  cache key type covered by `test_lighting_cache.c`.
- Colored/spot/manual-acceptance behavior remains the I1/I2 baseline;
  the I3 evidence does not weaken it.
- The colored-lighting 6 ms budget remains the headroom ceiling; a new
  light type without a measured envelope is not admissible.

## RQ5 — light-type decision matrix

| Candidate | Compatibility with v7 | Conflicts resolved today | Performance model | Recommendation |
|---|---|---|---|---|
| Directional | New v8 type required; v7 `type` is `point`/`spot` only | Bounding box, `LightShadowKey`, anti-light all undefined for an origin-less source | Per-tile occlusion is 64×32 ≈ 2,048 rays/frame before composition; unmeasured but unlikely to fit 6 ms | **DEFER** until an integrated optical/prepared-column prototype measures the envelope |
| Area (discrete) | New v8 object class with a parallel `SceneAreaLight`; not a v7 light | `MAX_LIGHTS`, `LightShadowKey`, and the cache hit ratio are all affected | A 12-sample disc is ≈ 12× the 4-light benchmark; the 4-light case already sits at 0.247 ms optimized / 0.080 ms cache-enabled | **DEFER**; the discrete path is admissible only with a separate Q1 review and a paired benchmark |
| Area (analytic) | New runtime path; not a v7 light | Editor UX, occlusion policy, anti-light all undefined | Quadrature per tile is unbounded without a coarse pre-cull | **REJECT** for I3; no current engine path supports analytic occlusion |
| Emissive surfaces | New v8 typed cell override; v6 mask bit space must be reserved | `palette_sample` order, the v6 mask bit space, and the spot/emit separation are all open | Sparse override is negligible; dense override requires a paired benchmark | **DEFER** with a documented upgrade path (see P3) |

## RQ6 — performance envelope

The colored-lighting benchmark remains the headroom reference:

- 32×24, 4 lights, 200 updates, 6 ms gate
- white 0.247 ms, colored 0.226 ms, spot 0.152 ms
- cache-enabled: spot 0.080 ms
- deterministic checksums:
  `11839672862453039471`, `2975859286826906331`, `1009061497268611021`
- PASS

The benchmark setup is the only quantitative reference available. None
of the three research candidates have a measured envelope on this
benchmark. The synthesis treats any claim of "fits in budget" as
unverified and therefore inadmissible.

## D1–D3 research recommendations

### D1 — schema vehicle

**Recommend v8, gated, for any of the three research candidates that
is later authorized.** The v7 grammar is locked and reserves no mask
bits for new light types. Any future directional/area/emissive work
must include:

- A new `SceneLightType` variant or a new `kind` enum with strict
  type-aware migration from v7.
- New reserved `OPTICAL_OVERRIDE_*` bits for any per-cell emissive
  field, with the existing bits unchanged.
- A new cache key (or a discriminated union on the existing one)
  for any new light type whose geometry does not fit the v7
  `(x, y, radius, direction, cone, falloff)` schema.

### D2 — property placement

**Recommend material-level defaults plus sparse per-cell overrides for
emissive surfaces**, mirroring R9 P1 D2. Directional and area lights are
not per-cell properties and therefore are not covered by D2.

### D3 — caps and presets

**Recommend a hard cap of one new light type per phase.** Do not
introduce directional, area, and emissive in a single implementation
increment; each requires its own decision record, benchmark, and
review checkpoint.

## Rejected approaches retained for evidence

| Attempt | Result | Reason rejected |
|---|---|---|
| Represent a directional light as a spot with `cone = 2π` at a synthetic anchor | Reuses the spot math and cache | Corrupts v7 spot semantics and the cache key; invalidates the I2 spec |
| Decompose an area light as one authorable object with N runtime samples | Reuses the shipping point/spot loop | Each sample is a separate cache key; raises the effective `num_lights` past the 6 ms budget; editor UX is wrong |
| Implement analytic occlusion for area lights | Correct in principle | No current engine path supports analytic occlusion; unbounded per-tile cost |
| Attach `emit` to the existing `Light` array | Reuses the per-instance model | Per-cell surface property vs. per-instance point/spot — wrong data model; would re-introduce the "invisible flag" anti-pattern |
| Author a v6 amendment for emissive | No new version number | v6 mask bit space is reserved; v6 must not be amended in flight |

## Remaining unknowns before any future implementation

1. Whether a directional source can reuse a new prepared-interval walk
   shared with the R9 optical compositor without breaking the 6 ms
   headroom.
2. Whether the colored-lighting benchmark can be extended with a
   representative directional workload without changing the published
   numbers, or whether a paired benchmark is required.
3. Whether a discrete area light can reuse the spot cache key with
   `cone = 2π` semantics preserved (i.e., without redefining the spot
   edge test).
4. Whether the R9 optical extension's reserved mask bits are sufficient
   for an emissive overlay, or whether the v6 spec must be extended in
   a v8.
5. What authoring surface (material default, per-cell override, or
   per-face override) carries the new property for emissive.
6. Whether the v7 spot `direction`/`cone`/`falloff` fields survive an
   unaltered v8 save when the v7 type is preserved as a v7-only record.

## Stop reason

I3 evidence is sufficient to recommend **DEFER** for all three research
candidates, to record the v8 schema vehicle as the only admissible
path for any future authorization, and to identify the paired benchmark
as a mandatory next step. Additional research would not change the
recommendation without a paired prototype that measures the 6 ms
envelope and the cache hit ratio.

## Cross-references

- Decision record: `R10_DECISION_RECORD_2026-08-27.md` (5. Increment
  ordering, 7. I2 spot — v7 bump + migration, Open follow-ups).
- Plan: `R10_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-27.md` (I3 —
  Research tracks).
- v7 spec: `R10_NATIVE_SCENE_V7_SPEC_2026-08-27.md`.
- I2 implementation record:
  `R10_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-27.md`.
- R9 research plan: `R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md`.
- R9 synthesis: `R9_RQ5_RQ6_SYNTHESIS_2026-08-24.md`.
