# R10 Increment I3 Implementation Record — Light-Type Research — 2026-08-27

## Status

**Research track complete; no implementation.** All three candidates
recorded an evidence write-up; two DEFER with documented upgrade paths,
one REJECT, and all three are gated behind a future Q1 review and a
paired benchmark. No shipping code, scene data, cache key, or runtime
value is changed; v7 and the I1/I2 behavior remain the active baseline.

## Evidence documents

- `R10_I3_P1_DIRECTIONAL_FINDINGS_2026-08-27.md` — DEFER until an
  integrated optical/prepared-column prototype measures the 6 ms
  envelope.
- `R10_I3_P2_AREA_FINDINGS_2026-08-27.md` — discrete DEFER, analytic
  REJECT for I3; the discrete path is admissible only with a separate
  Q1 review and a paired benchmark.
- `R10_I3_P3_EMISSIVE_FINDINGS_2026-08-27.md` — DEFER with a documented
  v8 upgrade path: material default + sparse per-cell override,
  reserved `OPTICAL_OVERRIDE_*` bit, applied at sample time after the
  light-map read and before per-channel clamp.
- `R10_I3_RQ5_RQ6_SYNTHESIS_2026-08-27.md` — combined policy table,
  RQ6 envelope (colored-lighting 6 ms gate; white 0.247 ms /
  colored 0.226 ms / spot 0.152 ms; cache-enabled 0.080 ms), D1–D3
  recommendations (v8 schema vehicle, gated, one new type per phase).

## Per-candidate summary

| Candidate | Decision | Required v8 surface | Required performance evidence |
|---|---|---|---|
| Directional | DEFER | New `kind` enum or new `SceneLightType` variant; new cache key or discriminated union | Directional workload added to `make benchmark-colored-lighting`; cache hit ratio measured |
| Area (discrete) | DEFER | Parallel `SceneAreaLight`; per-sample cache key; bounded sample count | A representative N-sample disc benchmark inside the 6 ms gate |
| Area (analytic) | REJECT | (no v8 surface admissible today) | (no engine path) |
| Emissive | DEFER | New `OPTICAL_OVERRIDE_EMISSIVE` bit; material default + sparse per-cell override; sample-time apply rule | Sparse and dense paired benchmark |

## Forbidden shortcuts recorded

- Do not represent a directional light as a spot with `cone = 2π` at a
  synthetic anchor; it would corrupt the v7 spot semantics and the
  cache key.
- Do not represent an area source as a wide spot or raise
  `MAX_LIGHTS` to host a fixed sample count without a paired
  benchmark.
- Do not attach `emit` to the existing `Light` array; the data model
  is per-cell surface vs. per-instance point/spot.
- Do not amend the v6 optical extension; the v6 mask bit space is
  reserved and any new field requires a v8 with strict migration.

## Required invariants

- v7 grammar, `LightShadowKey`, and the colored/spot runtime remain
  stable; I3 evidence does not weaken them.
- The colored-lighting 6 ms budget remains the headroom ceiling; any
  claim of "fits in budget" without a paired measurement is treated
  as unverified.
- The roadmap "no area/emissive implementation without an explicit
  performance model" rule remains binding.

## Next action

The I3 record is closed. The combined R10 I1–I2 manual visual/input
acceptance passed on 2026-08-27; I1 and I2 are Verified. Any future
directional/area/emissive work must
start with a separate Q1 decision record and a paired
`make benchmark-colored-lighting` extension that includes a
representative workload.
